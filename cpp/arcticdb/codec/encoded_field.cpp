/* Copyright 2026 Man Group Operations Limited
 *
 * Use of this software is governed by the Business Source License 1.1 included in the file licenses/BSL.txt.
 *
 * As of the Change Date specified in that file, in accordance with the Business Source License, use of this software
 * will be governed by the Apache License, version 2.0.
 */

#include <arcticdb/codec/encoded_field.hpp>
#include <arcticdb/codec/segment.hpp>

namespace arcticdb {

std::pair<const uint8_t*, const uint8_t*> get_segment_begin_end(const Segment& segment, const SegmentHeader& hdr) {
    const uint8_t* data = segment.buffer().data();
    util::check(data != nullptr, "Got null data ptr from segment in get_segment_begin_end");
    const uint8_t* begin = data;
    const auto fields_offset = hdr.footer_offset();
    const auto buffer_bytes = segment.buffer().bytes();
    // footer_offset is read from the untrusted on-disk header; reject values that would place the body
    // end past the segment buffer so downstream decode bounds checks operate on a valid range.
    codec::check<ErrorCode::E_DECODE_ERROR>(
            fields_offset <= buffer_bytes,
            "Segment footer offset {} exceeds buffer size {}",
            fields_offset,
            buffer_bytes
    );
    const auto end = begin + fields_offset;
    return {begin, end};
}

} // namespace arcticdb