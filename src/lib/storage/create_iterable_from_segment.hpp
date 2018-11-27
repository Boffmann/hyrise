#pragma once

#include "storage/dictionary_segment/dictionary_segment_iterable.hpp"
#include "storage/encoding_type.hpp"
#include "storage/segment_iterables/any_segment_iterable.hpp"

#include "storage/frame_of_reference/frame_of_reference_iterable.hpp"
#include "storage/reference_segment.hpp"
#include "storage/run_length_segment/run_length_segment_iterable.hpp"
#include "storage/value_segment/value_segment_iterable.hpp"
#include "resolve_type.hpp"

namespace opossum {

template <typename T>
class ReferenceSegmentIterable;

/**
 * @defgroup Uniform interface to create an iterable from a segment
 *
 * These methods cannot be part of the segments' interfaces because
 * reference segment are not templated and thus don’t know their type.
 *
 * All iterables implement the same interface using static polymorphism
 * (i.e. the CRTP pattern, see segment_iterables/.hpp).
 *
 * In debug mode, create_iterable_from_segment returns a type erased
 * iterable, i.e., all iterators have the same type
 *
 * @{
 */

template <typename T>
auto create_iterable_from_segment(const ValueSegment<T>& segment) {
  return erase_type_from_iterable_if_debug(ValueSegmentIterable<T>{segment});
}

template <typename T>
auto create_iterable_from_segment(const DictionarySegment<T>& segment) {
  return erase_type_from_iterable_if_debug(DictionarySegmentIterable<T, pmr_vector<T>>{segment});
}

template <typename T>
auto create_iterable_from_segment(const RunLengthSegment<T>& segment) {
  return erase_type_from_iterable_if_debug(RunLengthSegmentIterable<T>{segment});
}

template <typename T>
auto create_iterable_from_segment(const FixedStringDictionarySegment<T>& segment) {
  return erase_type_from_iterable_if_debug(DictionarySegmentIterable<T, FixedStringVector>{segment});
}

template <typename T>
auto create_iterable_from_segment(const FrameOfReferenceSegment<T>& segment) {
  return erase_type_from_iterable_if_debug(FrameOfReferenceIterable<T>{segment});
}

/**
 * This function must be forward-declared because ReferenceSegmentIterable
 * includes this file leading to a circular dependency
 */
template <typename T>
auto create_iterable_from_segment(const ReferenceSegment& segment);

/**@}*/

template<typename ColumnDataType>
AnySegmentIterable<ColumnDataType> create_any_segment_iterable(const BaseSegment& base_segment) {
  auto any_segment_iterable = std::optional<AnySegmentIterable<ColumnDataType>>{};

  resolve_segment_type<ColumnDataType>(base_segment, [&](const auto& segment) {
    const auto iterable = create_iterable_from_segment(segment);
    any_segment_iterable.emplace(erase_type_from_iterable(iterable));
  });

  return *any_segment_iterable;
}

template<typename ColumnDataType, typename Functor>
void for_each_segment_value(const BaseSegment& base_segment, const Functor& functor) {
#if IS_DEBUG
  const auto iterable = create_any_segment_iterable<ColumnDataType>(base_segment);
  iterable.for_each(functor);
#else

#endif
}

}  // namespace opossum

#include "create_iterable_from_segment.ipp"
