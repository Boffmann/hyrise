#include "table_scan_features.hpp"

namespace opossum {
namespace cost_model {

const std::map<std::string, AllTypeVariant> TableScanFeatures::serialize() const {
  std::map<std::string, AllTypeVariant> table_scan_features = {
      {"is_column_comparison", is_column_comparison},
      {"scan_operator_type", pmr_string{scan_operator_type}},
      {"computable_or_column_expression_count", static_cast<int64_t>(computable_or_column_expression_count)},
      {"effective_chunk_count", static_cast<int64_t>(effective_chunk_count)},
  };

  const auto serialized_first_column = first_column.serialize();
  const auto serialized_second_column = second_column.serialize();
  const auto serialized_third_column = third_column.serialize();

  table_scan_features.insert(serialized_first_column.begin(), serialized_first_column.end());
  table_scan_features.insert(serialized_second_column.begin(), serialized_second_column.end());
  table_scan_features.insert(serialized_third_column.begin(), serialized_third_column.end());

  return table_scan_features;
}

const std::unordered_map<std::string, float> TableScanFeatures::to_cost_model_features() const {
  std::unordered_map<std::string, float> table_scan_features = {
      // TODO(Sven): Hot-One-Encoding
      {"is_column_comparison", static_cast<float>(is_column_comparison)},
      //              {"scan_operator_type", scan_operator_type},
      {"computable_or_column_expression_count", static_cast<float>(computable_or_column_expression_count)},
      {"effective_chunk_count", static_cast<float>(effective_chunk_count)},
  };

  const auto serialized_first_column = first_column.to_cost_model_features();
  const auto serialized_second_column = second_column.to_cost_model_features();
  const auto serialized_third_column = third_column.to_cost_model_features();

  table_scan_features.insert(serialized_first_column.begin(), serialized_first_column.end());
  table_scan_features.insert(serialized_second_column.begin(), serialized_second_column.end());
  table_scan_features.insert(serialized_third_column.begin(), serialized_third_column.end());

  return table_scan_features;
}

}  // namespace cost_model
}  // namespace opossum