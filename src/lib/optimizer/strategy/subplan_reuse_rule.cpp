#include "subplan_reuse_rule.hpp"

#include <unordered_map>

#include "expression/binary_predicate_expression.hpp"
#include "expression/expression_utils.hpp"
#include "expression/lqp_column_expression.hpp"
#include "logical_query_plan/abstract_lqp_node.hpp"
#include "logical_query_plan/lqp_utils.hpp"

namespace {

using namespace opossum;  // NOLINT

std::unordered_set<LQPColumnReference> get_column_references(
    const std::vector<std::shared_ptr<AbstractExpression>>& expressions) {
  auto column_references = std::unordered_set<LQPColumnReference>{};
  for (const auto& expression : expressions) {
    visit_expression(expression, [&](const auto& sub_expression) {
      if (const auto column_expression = std::dynamic_pointer_cast<LQPColumnExpression>(sub_expression)) {
        column_references.emplace(column_expression->column_reference);
      }
      return ExpressionVisitation::VisitArguments;
    });
  }
  return column_references;
}

using ColumnReplacementMappings = std::unordered_map<LQPColumnReference, LQPColumnReference>;

void add_to_column_mapping(const std::shared_ptr<AbstractExpression>& from_expression, const std::shared_ptr<AbstractExpression>& to_expression, ColumnReplacementMappings& mappings) {
  Assert(from_expression->type == to_expression->type, "Expected same type");

  // TODO test that LQPColumnExpressions hidden in an argument (e.g., SUM(x) or AND(x, y)) are also detected

  if (const auto from_column_expression = std::dynamic_pointer_cast<LQPColumnExpression>(from_expression)) {
    const auto to_column_expression = std::dynamic_pointer_cast<LQPColumnExpression>(to_expression);
    mappings.emplace(from_column_expression->column_reference, to_column_expression->column_reference);
  } else {
    auto from_expressions_arguments_iter = from_expression->arguments.begin();
    auto from_expressions_arguments_end_iter = from_expression->arguments.end();
    auto to_expressions_arguments_iter = to_expression->arguments.begin();

    DebugAssert(from_expressions_arguments_end_iter - from_expressions_arguments_iter == to_expression->arguments.end() - to_expressions_arguments_iter, "Mismatching number of expression arguments");

    while (from_expressions_arguments_iter != from_expressions_arguments_end_iter) {
      add_to_column_mapping(*from_expressions_arguments_iter, *to_expressions_arguments_iter, mappings);
      ++from_expressions_arguments_iter;
      ++to_expressions_arguments_iter;
    }
  }
}

ColumnReplacementMappings create_column_mapping(const AbstractLQPNode& from_node, const AbstractLQPNode& to_node) {
  auto mapping = std::unordered_map<LQPColumnReference, LQPColumnReference>{};

  const auto& from_expressions = from_node.column_expressions();
  const auto& to_expressions = to_node.column_expressions();

  Assert(from_expressions.size() == to_expressions.size(), "Expected same number of expressions");

  for (auto column_id = ColumnID{0}; column_id < from_expressions.size(); ++column_id) {
    add_to_column_mapping(from_expressions[column_id], to_expressions[column_id], mapping);
  }

  return mapping;
}

void apply_column_replacement_mappings(std::shared_ptr<AbstractExpression>& expression,
                                      const ColumnReplacementMappings& column_replacement_mappings) {

  for ([[maybe_unused]] const auto& [from, to] : column_replacement_mappings) {
    // Not sure if this is a valid assertion. In any case, lineage on the `from` side is unhandled.
    // std::cout << "\t" << from << " -> " << to << std::endl;
    DebugAssert(from.lineage.empty(), "Expected lineage of `from` side to be empty");
  }

  // need copy so that we do not manipulate upstream expressions
  auto expression_copy = expression->deep_copy();
  auto replacement_occured = false;
  visit_expression(expression_copy, [&column_replacement_mappings, &replacement_occured](auto& sub_expression) {
    if (const auto column_expression = std::dynamic_pointer_cast<LQPColumnExpression>(sub_expression)) {
      // std::cout << "\tshould I replace " << column_expression->column_reference << "?" << std::endl;
      auto column_reference_without_lineage = LQPColumnReference{column_expression->column_reference.original_node(), column_expression->column_reference.original_column_id()};
      const auto column_replacement_iter = column_replacement_mappings.find(column_reference_without_lineage);
      if (column_replacement_iter != column_replacement_mappings.end()) {
        auto new_column_reference = column_replacement_iter->second;
        // Restore lineage
        for (const auto& via : column_expression->column_reference.lineage) {
          new_column_reference.lineage.emplace_back(via);
        }

        sub_expression = std::make_shared<LQPColumnExpression>(new_column_reference);

        replacement_occured = true;
      }
    }
    return ExpressionVisitation::VisitArguments;
  });
  if (replacement_occured) expression = expression_copy;
}

void apply_column_replacement_mappings(std::vector<std::shared_ptr<AbstractExpression>>& expressions,
                                      const ColumnReplacementMappings& column_replacement_mappings) {
  for (auto& expression : expressions) {
    apply_column_replacement_mappings(expression, column_replacement_mappings);
  }
}

void apply_column_replacement_mappings_upwards(const std::shared_ptr<AbstractLQPNode>& node,
                                      ColumnReplacementMappings& column_replacement_mappings,
                                      std::unordered_map<std::shared_ptr<AbstractLQPNode>, ColumnReplacementMappings>& per_node_replacements) {
  visit_lqp_upwards(node, [&column_replacement_mappings, &per_node_replacements](const auto& sub_node) {

    auto column_replacement_mappings_local = column_replacement_mappings;


    if (const auto join_node = std::dynamic_pointer_cast<JoinNode>(sub_node)) {
      const auto left_column_references = get_column_references(join_node->left_input()->column_expressions());
      const auto right_column_references = get_column_references(join_node->right_input()->column_expressions());


      ColumnReplacementMappings updated_mappings;

      for (auto& [from, to] : column_replacement_mappings) {
        if (join_node->join_mode != JoinMode::Semi && join_node->join_mode != JoinMode::AntiNullAsTrue && join_node->join_mode != JoinMode::AntiNullAsFalse) {
          DebugAssert(!left_column_references.contains(from) || !right_column_references.contains(from), "Ambiguous mapping 1");
          DebugAssert(!left_column_references.contains(to) || !right_column_references.contains(to), "Ambiguous mapping 2");
        }


        if (left_column_references.contains(from) && right_column_references.contains(to)) {
          auto updated_to_left = to;
          updated_to_left.lineage.emplace_back(join_node->shared_from_this(), LQPInputSide::Left);
          updated_mappings[from] = updated_to_left;

          auto updated_to_right = to;
          updated_to_right.lineage.emplace_back(join_node->shared_from_this(), LQPInputSide::Right);
          updated_mappings[to] = updated_to_right;

        }

        if (right_column_references.contains(from) && left_column_references.contains(to)) {
          auto updated_to_right = to;
          updated_to_right.lineage.emplace_back(join_node->shared_from_this(), LQPInputSide::Right);
          updated_mappings[from] = updated_to_right;

          auto updated_to_left = to;
          updated_to_left.lineage.emplace_back(join_node->shared_from_this(), LQPInputSide::Left);
          updated_mappings[to] = updated_to_left;

        }
      }

      for (const auto& [from, to] : updated_mappings) {
        column_replacement_mappings_local[from] = to;
        if (join_node->join_mode != JoinMode::Semi && join_node->join_mode != JoinMode::AntiNullAsTrue && join_node->join_mode != JoinMode::AntiNullAsFalse) {
          column_replacement_mappings[from] = to;
        }
      }
    }

    per_node_replacements[sub_node] = column_replacement_mappings_local;
    return LQPUpwardVisitation::VisitOutputs;
  });
}

}  // namespace

// TODO Test the things from the whiteboard

namespace opossum {

using LQPNodeUnorderedSet =  // TODO move to abstract_lqp_node.hpp
    std::unordered_set<std::shared_ptr<AbstractLQPNode>, LQPNodeSharedPtrHash, LQPNodeSharedPtrEqual>;  

// TODO Is it necessary to update the lineage? Test this

void SubplanReuseRule::apply_to(const std::shared_ptr<AbstractLQPNode>& root) const {
  Assert(root->type == LQPNodeType::Root, "SubplanReuseRule needs root to hold onto");

  // std::cout << "\n\n\n=== IN ===" << std::endl;
  // std::cout << *root << std::endl;

  bool more = true;
  while (more) {
    LQPNodeUnorderedSet primary_subplans;
    more = false;
    visit_lqp(root, [&](const auto& node) {
      if (more) return LQPVisitation::DoNotVisitInputs;
      const auto [primary_subplan_iter, is_primary_subplan] = primary_subplans.emplace(node);
      if (is_primary_subplan) return LQPVisitation::VisitInputs;

      // We have seen this plan before and can reuse it
      const auto& primary_subplan = *primary_subplan_iter;

      auto column_mapping = create_column_mapping(*node, *primary_subplan);

      // std::cout << "initial mapping" << std::endl;
      // for (const auto& [from, to] : column_mapping) {
      //   std::cout << "\t" << from << " -> " << to << std::endl;
      // }

      std::unordered_map<std::shared_ptr<AbstractLQPNode>, ColumnReplacementMappings> per_node_replacements;  // TODO does this have to be a map?

      apply_column_replacement_mappings_upwards(node, column_mapping, per_node_replacements);

      for (const auto& [node, mapping] : per_node_replacements) {
        // std::cout << node->description() << std::endl;
        apply_column_replacement_mappings(node->node_expressions, mapping);
      }

      for (const auto& [output, input_side] : node->output_relations()) {
        output->set_input(input_side, primary_subplan);
      }

      // more = true;

      return LQPVisitation::DoNotVisitInputs;
    });
  }

  // std::cout << "\n\n\n=== OUT ===" << std::endl;
  // std::cout << *root << std::endl;
}

}  // namespace opossum
