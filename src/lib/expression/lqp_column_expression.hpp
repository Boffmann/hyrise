#pragma once

#include "abstract_expression.hpp"

#include "logical_query_plan/abstract_lqp_node.hpp"
#include "logical_query_plan/lqp_column_reference.hpp"
#include "types.hpp"

namespace opossum {

class LQPColumnExpression : public AbstractExpression {
 public:
  explicit LQPColumnExpression(const LQPColumnReference& init_column_reference);

  std::shared_ptr<AbstractExpression> deep_copy() const override;
  std::string description(const DescriptionMode mode) const override;
  DataType data_type() const override;
  bool requires_computation() const override;

  const LQPColumnReference column_reference;

 protected:
  bool _shallow_equals(const AbstractExpression& expression) const override;
  size_t _shallow_hash() const override;
  bool _on_is_nullable_on_lqp(const AbstractLQPNode& lqp) const override;
};

}  // namespace opossum
