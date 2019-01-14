#include "segment_statistics.hpp" // NEEDEDINCLUDE



#include "min_max_filter.hpp" // NEEDEDINCLUDE
#include "range_filter.hpp" // NEEDEDINCLUDE
#include "storage/segment_iterate.hpp" // NEEDEDINCLUDE

namespace opossum {

template <typename T>
static std::shared_ptr<SegmentStatistics> build_statistics_from_dictionary(const pmr_vector<T>& dictionary) {
  auto statistics = std::make_shared<SegmentStatistics>();
  // only create statistics when the compressed dictionary is not empty
  if (!dictionary.empty()) {
    // no range filter for strings
    // clang-format off
    if constexpr(std::is_arithmetic_v<T>) {
      auto range_filter = RangeFilter<T>::build_filter(dictionary);
      statistics->add_filter(std::move(range_filter));
    } else {
      // we only need the min-max filter if we cannot have a range filter
      auto min_max_filter = std::make_unique<MinMaxFilter<T>>(dictionary.front(), dictionary.back());
      statistics->add_filter(std::move(min_max_filter));
    }
    // clang-format on
  }
  return statistics;
}

std::shared_ptr<SegmentStatistics> SegmentStatistics::build_statistics(
    DataType data_type, const std::shared_ptr<const BaseSegment>& segment) {
  std::shared_ptr<SegmentStatistics> statistics;

  resolve_data_and_segment_type(*segment, [&statistics](auto type, auto& typed_segment) {
    using SegmentType = std::decay_t<decltype(typed_segment)>;
    using DataTypeT = typename decltype(type)::type;

    // clang-format off
    if constexpr(std::is_same_v<SegmentType, DictionarySegment<DataTypeT>>) {
        // we can use the fact that dictionary segments have an accessor for the dictionary
        const auto& dictionary = *typed_segment.dictionary();
        statistics = build_statistics_from_dictionary(dictionary);
    } else {
      // if we have a generic segment we create the dictionary ourselves
      auto iterable = create_iterable_from_segment<DataTypeT>(typed_segment);
      std::unordered_set<DataTypeT> values;
      iterable.for_each([&](const auto& position) {
        // we are only interested in non-null values
        if (!position.is_null()) {
          values.insert(position.value());
        }
      });
      pmr_vector<DataTypeT> dictionary{values.cbegin(), values.cend()};
      std::sort(dictionary.begin(), dictionary.end());
      statistics = build_statistics_from_dictionary(dictionary);
    }
    // clang-format on
  });
  return statistics;
}
void SegmentStatistics::add_filter(std::shared_ptr<AbstractFilter> filter) { _filters.emplace_back(filter); }

bool SegmentStatistics::can_prune(const PredicateCondition predicate_type, const AllTypeVariant& variant_value,
                                  const std::optional<AllTypeVariant>& variant_value2) const {
  for (const auto& filter : _filters) {
    if (filter->can_prune(predicate_type, variant_value, variant_value2)) {
      return true;
    }
  }
  return false;
}
}  // namespace opossum
