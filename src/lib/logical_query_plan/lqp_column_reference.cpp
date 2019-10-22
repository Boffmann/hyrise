#include "lqp_column_reference.hpp"

#include "boost/functional/hash.hpp"

#include "abstract_lqp_node.hpp"
#include "hyrise.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "storage/table.hpp"
#include "utils/assert.hpp"

namespace opossum {

LQPColumnReference::LQPColumnReference(const std::shared_ptr<const AbstractLQPNode>& original_node,
                                       ColumnID original_column_id)
    : _original_node(original_node), _original_column_id(original_column_id) {}

std::shared_ptr<const AbstractLQPNode> LQPColumnReference::original_node() const { return _original_node.lock(); }

ColumnID LQPColumnReference::original_column_id() const { return _original_column_id; }

bool LQPColumnReference::operator==(const LQPColumnReference& rhs) const {
  if (_original_column_id != rhs._original_column_id) return false;
  if (lineage.size() != rhs.lineage.size()) return false;
  if (original_node() != rhs.original_node()) return false;

  for (auto lineage_iter = lineage.begin(), rhs_lineage_iter = rhs.lineage.begin(); lineage_iter != lineage.end(); ++lineage_iter, ++rhs_lineage_iter) {
    if (lineage_iter->second != rhs_lineage_iter->second) return false;
    if (lineage_iter->first.lock() != rhs_lineage_iter->first.lock()) return false;
  }

  return true;
}

bool LQPColumnReference::operator!=(const LQPColumnReference& rhs) const {
  return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& os, const LQPColumnReference& column_reference) {
  const auto original_node = column_reference.original_node();
  Assert(original_node, "OriginalNode has expired");

  const auto stored_table_node = std::static_pointer_cast<const StoredTableNode>(column_reference.original_node());
  const auto table = Hyrise::get().storage_manager.get_table(stored_table_node->table_name);

  // os << table->column_name(column_reference.original_column_id());

  os << '"' << table->column_name(column_reference.original_column_id()) << " from " << column_reference.original_node();
  for (const auto& step : column_reference.lineage) {
    os << " via " << step.first.lock() << "(" << (step.second == LQPInputSide::Left ? "left" : "right") << ")";
  }
  os << '"';

  return os;
}
}  // namespace opossum

namespace std {

size_t hash<opossum::LQPColumnReference>::operator()(const opossum::LQPColumnReference& column_reference) const {
  // It is important not to combine the pointer of the original_node with the hash code as it was done before #1795.
  // If this pointer is combined with the return hash code, equal LQP nodes that are not identical and that have
  // LQPColumnExpressions or child nodes with LQPColumnExpressions would have different hash codes.

  // We could include `column_reference.original_node()->hash()` in the hash, but since hashing an LQP node has a
  // certain cost, we allow those collisions and rely on operator== to sort it out.

  // DebugAssert(column_reference.original_node(), "OriginalNode has expired");
  // TODO reactivate
  return column_reference.original_column_id();

  // TODO include lineage?
}

}  // namespace std
