#include "gtest/gtest.h"

#include "expression/expression_functional.hpp"
#include "logical_query_plan/delete_node.hpp"
#include "logical_query_plan/insert_node.hpp"
#include "logical_query_plan/join_node.hpp"
#include "logical_query_plan/mock_node.hpp"
#include "logical_query_plan/predicate_node.hpp"
#include "logical_query_plan/projection_node.hpp"
#include "logical_query_plan/sort_node.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "logical_query_plan/union_node.hpp"
#include "logical_query_plan/update_node.hpp"
#include "optimizer/strategy/dependent_group_by_reduction_rule.hpp"

#include "strategy_base_test.hpp"
#include "testing_assert.hpp"

using namespace opossum::expression_functional;  // NOLINT

namespace opossum {

class DependentGroupByReductionRuleTest : public StrategyBaseTest {
 public:
  void SetUp() override {
    auto& storage_manager = Hyrise::get().storage_manager;

    TableColumnDefinitions column_definitions{{"column0", DataType::Int, false}, {"column1", DataType::Int, false}, {"column2", DataType::Int, false}};

    table_a = std::make_shared<Table>(column_definitions, TableType::Data, 2, UseMvcc::Yes);
    table_a->add_soft_unique_constraint({ColumnID{0}}, IsPrimaryKey::Yes);
    storage_manager.add_table("table_a", table_a);
    stored_table_node_a = StoredTableNode::make("table_a");
    column_a_0 = stored_table_node_a->get_column("column0");
    column_a_1 = stored_table_node_a->get_column("column1");
    column_a_2 = stored_table_node_a->get_column("column2");

    table_b = std::make_shared<Table>(column_definitions, TableType::Data, 2, UseMvcc::Yes);
    table_b->add_soft_unique_constraint({ColumnID{0}, ColumnID{1}}, IsPrimaryKey::No);
    storage_manager.add_table("table_b", table_b);
    stored_table_node_b = StoredTableNode::make("table_b");
    column_b_0 = stored_table_node_b->get_column("column0");
    column_b_1 = stored_table_node_b->get_column("column1");
    column_b_2 = stored_table_node_b->get_column("column2");

    table_c = std::make_shared<Table>(column_definitions, TableType::Data, 2, UseMvcc::Yes);
    table_c->add_soft_unique_constraint({ColumnID{0}, ColumnID{2}}, IsPrimaryKey::Yes);
    storage_manager.add_table("table_c", table_c);
    stored_table_node_c = StoredTableNode::make("table_c");
    column_c_0 = stored_table_node_c->get_column("column0");
    column_c_1 = stored_table_node_c->get_column("column1");
    column_c_2 = stored_table_node_c->get_column("column2");

    rule = std::make_shared<DependentGroupByReductionRule>();
  }

  std::shared_ptr<DependentGroupByReductionRule> rule;

  std::shared_ptr<Table> table_a, table_b, table_c;
  std::shared_ptr<StoredTableNode> stored_table_node_a, stored_table_node_b, stored_table_node_c;
  LQPColumnReference column_a_0, column_a_1, column_a_2;
  LQPColumnReference column_b_0, column_b_1, column_b_2;
  LQPColumnReference column_c_0, column_c_1, column_c_2;
};

// same columns from two sides, once it will be ANY'd, once not
// single key does work
// two of two does not
// one of two does not work
// any expressions on a column do no longer work even though is would theoretically be possible (at least +1)

TEST_F(DependentGroupByReductionRuleTest, SingleKeyReduction) {
  // clang-format off
  auto lqp =
  AggregateNode::make(expression_vector(column_a_0, column_a_1),
                      expression_vector(sum_(column_a_0), sum_(column_a_1),
                                        sum_(column_a_2)),
                      stored_table_node_a);

  const auto actual_lqp = apply_rule(rule, lqp);

  const auto expected_lqp =
  AggregateNode::make(expression_vector(column_a_0),
                      expression_vector(any_(column_a_0), any_(column_a_1),
                                        any_(column_a_2)),
                      stored_table_node_a);
  // clang-format on

  EXPECT_LQP_EQ(actual_lqp, expected_lqp);
}

TEST_F(DependentGroupByReductionRuleTest, IncompleteKey) {
  // clang-format off
  auto lqp =
  AggregateNode::make(expression_vector(column_b_0),
                      expression_vector(sum_(column_b_0), sum_(column_b_1),
                                        sum_(column_b_2)),
                      stored_table_node_b);

  const auto actual_lqp = apply_rule(rule, lqp);

  const auto expected_lqp =
  AggregateNode::make(expression_vector(column_b_0),
                      expression_vector(sum_(column_b_0), sum_(column_b_1),
                                        sum_(column_b_2)),
                      stored_table_node_b);
  // clang-format on

  EXPECT_LQP_EQ(actual_lqp, expected_lqp);
}

TEST_F(DependentGroupByReductionRuleTest, UnnecessaryGroupByColumn) {
  // clang-format off
  auto lqp =
  AggregateNode::make(expression_vector(column_a_0, column_a_1),
                      expression_vector(sum_(column_a_0)),
                      stored_table_node_a);

  const auto actual_lqp = apply_rule(rule, lqp);

  const auto expected_lqp =
  AggregateNode::make(expression_vector(column_a_0),
                      expression_vector(any_(column_a_0)),
                      stored_table_node_a);
  // clang-format on

  EXPECT_LQP_EQ(actual_lqp, expected_lqp);
}

}  // namespace opossum