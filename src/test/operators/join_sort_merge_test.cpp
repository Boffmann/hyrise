#include "base_test.hpp"

#include "operators/join_sort_merge.hpp"
#include "operators/print.hpp"
#include "operators/projection.hpp"
#include "operators/table_wrapper.hpp"

namespace opossum {

class OperatorsJoinSortMergeTest : public BaseTest {
 public:
  void SetUp() override {
    const auto dummy_table =
        std::make_shared<Table>(TableColumnDefinitions{{"a", DataType::Int, false}}, TableType::Data);
    dummy_input = std::make_shared<TableWrapper>(dummy_table);
  }

  std::shared_ptr<AbstractOperator> dummy_input;
};

TEST_F(OperatorsJoinSortMergeTest, DescriptionAndName) {
  const auto primary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{0}}, PredicateCondition::Equals};
  const auto secondary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{0}}, PredicateCondition::NotEquals};

  const auto join_operator =
      std::make_shared<JoinSortMerge>(dummy_input, dummy_input, JoinMode::Inner, primary_predicate,
                                      std::vector<OperatorJoinPredicate>{secondary_predicate});

  EXPECT_EQ(join_operator->description(DescriptionMode::SingleLine),
            "JoinSortMerge (Inner Join where Column #0 = Column #0 AND Column #0 != Column #0)");
  EXPECT_EQ(join_operator->description(DescriptionMode::MultiLine),
            "JoinSortMerge\n(Inner Join where Column #0 = Column #0 AND Column #0 != Column #0)");

  dummy_input->execute();
  EXPECT_EQ(join_operator->description(DescriptionMode::SingleLine),
            "JoinSortMerge (Inner Join where a = a AND a != a)");
  EXPECT_EQ(join_operator->description(DescriptionMode::MultiLine),
            "JoinSortMerge\n(Inner Join where a = a AND a != a)");

  EXPECT_EQ(join_operator->name(), "JoinSortMerge");
}

TEST_F(OperatorsJoinSortMergeTest, DeepCopy) {
  const auto primary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{0}}, PredicateCondition::Equals};
  const auto join_operator =
      std::make_shared<JoinSortMerge>(dummy_input, dummy_input, JoinMode::Left, primary_predicate);
  const auto abstract_join_operator_copy = join_operator->deep_copy();
  const auto join_operator_copy = std::dynamic_pointer_cast<JoinSortMerge>(join_operator);

  ASSERT_TRUE(join_operator_copy);

  EXPECT_EQ(join_operator_copy->mode(), JoinMode::Left);
  EXPECT_EQ(join_operator_copy->primary_predicate(), primary_predicate);
  EXPECT_NE(join_operator_copy->input_left(), nullptr);
  EXPECT_NE(join_operator_copy->input_right(), nullptr);
}

TEST_F(OperatorsJoinSortMergeTest, ValueClusteringFlag) {
  const auto test_table = std::make_shared<Table>(
      TableColumnDefinitions{{"a", DataType::Int, false}, {"b", DataType::Int, false}, {"c", DataType::Int, false}},
      TableType::Data);

  test_table->append({1, 2, 3});
  test_table->append({2, 1, 4});
  test_table->append({1, 2, 5});

  const auto test_input = std::make_shared<TableWrapper>(test_table);

  const auto primary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{1}}, PredicateCondition::Equals};
  const auto join_operator = std::make_shared<JoinSortMerge>(test_input, test_input, JoinMode::Left, primary_predicate);

  test_input->execute();
  join_operator->execute();

  const auto output_table = join_operator->get_output();

  const std::vector<ColumnID> expected_value_clustering{ColumnID{0}, ColumnID{4}};
  for (auto chunk_id = ChunkID{0}; chunk_id < output_table->chunk_count(); ++chunk_id) {
    const auto actual_value_clustering = *(output_table->get_chunk(chunk_id)->value_clustered_by());
    EXPECT_EQ(expected_value_clustering, actual_value_clustering);
  }
}

TEST_F(OperatorsJoinSortMergeTest, MaintainSortedness) {
  const auto test_table = std::make_shared<Table>(
      TableColumnDefinitions{{"a", DataType::Int, false}, {"b", DataType::Int, false}, {"c", DataType::Int, false}},
      TableType::Data);

  test_table->append({1, 2, 3});
  test_table->append({2, 1, 4});
  test_table->append({1, 2, 5});

  const auto test_input = std::make_shared<TableWrapper>(test_table);

  const auto primary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{1}}, PredicateCondition::Equals};
  const auto join_operator = std::make_shared<JoinSortMerge>(test_input, test_input, JoinMode::Left, primary_predicate);

  test_input->execute();
  join_operator->execute();

  const auto output_table = join_operator->get_output();

  const std::vector<std::pair<ColumnID, OrderByMode>> expected_sorted_columns{
      std::make_pair(ColumnID{0}, OrderByMode::Ascending), std::make_pair(ColumnID{4}, OrderByMode::Ascending)};
  for (auto chunk_id = ChunkID{0}; chunk_id < output_table->chunk_count(); ++chunk_id) {
    const auto actual_sorted_columns = *(output_table->get_chunk(chunk_id)->ordered_by());
    EXPECT_EQ(expected_sorted_columns, actual_sorted_columns);
  }
}

}  // namespace opossum
