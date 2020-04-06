#include <iostream>
#include <memory>
#include <utility>

#include "base_test.hpp"

#include "operators/abstract_read_only_operator.hpp"
#include "operators/join_nested_loop.hpp"
#include "operators/print.hpp"
#include "operators/sort.hpp"
#include "operators/table_scan.hpp"
#include "operators/table_wrapper.hpp"
#include "operators/union_all.hpp"
#include "storage/chunk_encoder.hpp"
#include "storage/table.hpp"
#include "types.hpp"

namespace opossum {

class OperatorsSortTest : public BaseTestWithParam<EncodingType> {
 protected:
  void SetUp() override {
    _encoding_type = GetParam();

    _table_wrapper = std::make_shared<TableWrapper>(load_table("resources/test_data/tbl/int_float.tbl", 2));
    _table_wrapper_null =
        std::make_shared<TableWrapper>(load_table("resources/test_data/tbl/int_float_with_null.tbl", 2));

    auto table = load_table("resources/test_data/tbl/int_float.tbl", 2);
    ChunkEncoder::encode_all_chunks(table, _encoding_type);

    auto table_dict = load_table("resources/test_data/tbl/int_float_with_null.tbl", 2);
    ChunkEncoder::encode_all_chunks(table_dict, _encoding_type);

    _table_wrapper_dict = std::make_shared<TableWrapper>(std::move(table));
    _table_wrapper_dict->execute();

    _table_wrapper_null_dict = std::make_shared<TableWrapper>(std::move(table_dict));
    _table_wrapper_null_dict->execute();

    _table_wrapper_outer_join = std::make_shared<TableWrapper>(load_table("resources/test_data/tbl/int_float2.tbl", 2));
    _table_wrapper_outer_join->execute();

    _table_wrapper->execute();
    _table_wrapper_null->execute();
  }

 protected:
  std::shared_ptr<TableWrapper> _table_wrapper, _table_wrapper_null, _table_wrapper_dict, _table_wrapper_null_dict,
      _table_wrapper_outer_join;
  EncodingType _encoding_type;
};

auto sort_test_formatter = [](const ::testing::TestParamInfo<EncodingType> info) {
  return std::to_string(static_cast<uint32_t>(info.param));
};

// As long as two implementation of dictionary encoding exist, this ensure to run the tests for both.
INSTANTIATE_TEST_SUITE_P(DictionaryEncodingTypes, OperatorsSortTest, ::testing::Values(EncodingType::Dictionary),
                         sort_test_formatter);

TEST_P(OperatorsSortTest, AscendingSortOfOneColumn) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_sorted.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, AscendingSortOFilteredColumn) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_filtered_sorted.tbl", 2);

  auto input = std::make_shared<TableWrapper>(load_table("resources/test_data/tbl/int_float.tbl", 1));
  input->execute();

  auto scan = create_table_scan(input, ColumnID{0}, PredicateCondition::NotEquals, 123);
  scan->execute();

  auto sort = std::make_shared<Sort>(
      scan, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, AscendingSortOfOneColumnWithoutChunkSize) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_sorted.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}});
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, DoubleSortOfOneColumn) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_sorted.tbl", 2);

  auto sort1 = std::make_shared<Sort>(
      _table_wrapper, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Descending}},
      2u);
  sort1->execute();

  auto sort2 = std::make_shared<Sort>(
      sort1, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}}, 2u);
  sort2->execute();

  EXPECT_TABLE_EQ_ORDERED(sort2->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, DescendingSortOfOneColumn) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_reverse.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Descending}},
      2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, MultipleColumnSortIsStable) {
  auto table_wrapper = std::make_shared<TableWrapper>(load_table("resources/test_data/tbl/int_float4.tbl", 2));
  table_wrapper->execute();

  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float2_sorted.tbl", 2);

  auto sort_definitions =
      std::vector<SortColumnDefinition>{{SortColumnDefinition{ColumnID{0}, SortMode::Ascending},
                                         SortColumnDefinition{ColumnID{1}, SortMode::Ascending}}};
  auto sort = std::make_shared<Sort>(table_wrapper, sort_definitions, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, MultipleColumnSortIsStableMixedOrder) {
  auto table_wrapper = std::make_shared<TableWrapper>(load_table("resources/test_data/tbl/int_float4.tbl", 2));
  table_wrapper->execute();

  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float2_sorted_mixed.tbl", 2);

  auto sort_definitions =
      std::vector<SortColumnDefinition>{{SortColumnDefinition{ColumnID{0}, SortMode::Ascending},
                                         SortColumnDefinition{ColumnID{1}, SortMode::Descending}}};
  auto sort = std::make_shared<Sort>(table_wrapper, sort_definitions, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, AscendingSortOfOneColumnWithNull) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_null_sorted_asc.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_null, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}},
      2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, DescendingSortOfOneColumnWithNull) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_null_sorted_desc.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_null,
      std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Descending}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, AscendingSortOfOneColumnWithNullsLast) {
  std::shared_ptr<Table> expected_result =
      load_table("resources/test_data/tbl/int_float_null_sorted_asc_nulls_last.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_null,
      std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::AscendingNullsLast}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, DescendingSortOfOneColumnWithNullsLast) {
  std::shared_ptr<Table> expected_result =
      load_table("resources/test_data/tbl/int_float_null_sorted_desc_nulls_last.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_null,
      std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::DescendingNullsLast}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, AscendingSortOfOneDictSegmentWithNull) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_null_sorted_asc.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_null_dict,
      std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, DescendingSortOfOneDictSegmentWithNull) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_null_sorted_desc.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_null_dict,
      std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Descending}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, AscendingSortOfOneDictSegment) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_sorted.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_dict, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}},
      2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, DescendingSortOfOneDictSegment) {
  std::shared_ptr<Table> expected_result = load_table("resources/test_data/tbl/int_float_reverse.tbl", 2);

  auto sort = std::make_shared<Sort>(
      _table_wrapper_dict,
      std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Descending}}, 2u);
  sort->execute();

  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

TEST_P(OperatorsSortTest, SortAfterOuterJoin) {
  auto join =
      std::make_shared<JoinNestedLoop>(_table_wrapper, _table_wrapper_outer_join, JoinMode::FullOuter,
                                       OperatorJoinPredicate{{ColumnID{0}, ColumnID{0}}, PredicateCondition::Equals});
  join->execute();

  auto sort = std::make_shared<Sort>(
      join, std::vector<SortColumnDefinition>{SortColumnDefinition{ColumnID{0}, SortMode::Ascending}});
  sort->execute();

  std::shared_ptr<Table> expected_result =
      load_table("resources/test_data/tbl/join_operators/int_outer_join_sorted_asc.tbl", 2);
  EXPECT_TABLE_EQ_ORDERED(sort->get_output(), expected_result);
}

}  // namespace opossum
