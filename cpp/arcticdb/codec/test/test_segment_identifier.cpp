/* Copyright 2026 Man Group Operations Limited
 *
 * Use of this software is governed by the Business Source License 1.1 included in the file licenses/BSL.txt.
 *
 * As of the Change Date specified in that file, in accordance with the Business Source License, use of this software
 * will be governed by the Apache License, version 2.0.
 */
#include <gtest/gtest.h>
#include <arcticdb/codec/codec.hpp>
#include <arcticdb/codec/segment.hpp>
#include <arcticdb/codec/segment_identifier.hpp>
#include <arcticdb/stream/index.hpp>
#include <arcticdb/util/test/test_utils.hpp>

#include <algorithm>
#include <limits>

using namespace arcticdb;

namespace {

constexpr std::string_view bounds_symbol = "segment_identifier_bounds_symbol";

std::vector<uint8_t> identifier_buffer(const StreamId& stream_id) {
    Buffer buffer{identifier_bytes(stream_id)};
    std::ptrdiff_t pos = 0;
    write_identifier(buffer, pos, stream_id);
    return {buffer.data(), buffer.data() + pos};
}

SegmentIdentifier* find_string_identifier(uint8_t* begin, uint8_t* end, std::string_view name) {
    auto it = std::search(begin + SegmentIdentifierSize, end, name.begin(), name.end());
    if (it == end)
        return nullptr;
    auto* identifier = reinterpret_cast<SegmentIdentifier*>(it - SegmentIdentifierSize);
    if (identifier->header_.type_ != IdentifierType::STRING || identifier->header_.size_ != name.size())
        return nullptr;
    return identifier;
}

Segment encode_bounds_segment() {
    const auto stream_desc = stream_descriptor(
            StreamId{std::string{bounds_symbol}}, RowCountIndex{}, {scalar_field(DataType::UINT8, "ints")}
    );
    SegmentInMemory in_mem_seg{stream_desc.clone()};
    in_mem_seg.set_scalar<uint8_t>(0, 23);
    in_mem_seg.end_row();
    return encode_v2(std::move(in_mem_seg), codec::default_lz4_codec());
}

} // namespace

TEST(SegmentIdentifier, RoundtripString) {
    auto bytes = identifier_buffer(StreamId{std::string{"thing"}});
    const uint8_t* data = bytes.data();
    const uint8_t* end = bytes.data() + bytes.size();
    ASSERT_EQ(read_identifier(data, end), StreamId{std::string{"thing"}});
    ASSERT_EQ(data, end);
    data = bytes.data();
    skip_identifier(data, end);
    ASSERT_EQ(data, end);
}

TEST(SegmentIdentifier, RoundtripNumeric) {
    auto bytes = identifier_buffer(StreamId{NumericId{42}});
    const uint8_t* data = bytes.data();
    const uint8_t* end = bytes.data() + bytes.size();
    ASSERT_EQ(read_identifier(data, end), StreamId{NumericId{42}});
    ASSERT_EQ(data, end);
}

TEST(SegmentIdentifier, StringLengthExceedsBuffer) {
    auto bytes = identifier_buffer(StreamId{std::string{"thing"}});
    reinterpret_cast<SegmentIdentifier*>(bytes.data())->header_.size_ = std::numeric_limits<uint32_t>::max();
    const uint8_t* data = bytes.data();
    const uint8_t* end = bytes.data() + bytes.size();
    ASSERT_THROW(read_identifier(data, end), CodecException);
    ASSERT_EQ(data, bytes.data());
    ASSERT_THROW(skip_identifier(data, end), CodecException);
    ASSERT_EQ(data, bytes.data());
}

TEST(SegmentIdentifier, StringLengthOneByteTooLong) {
    auto bytes = identifier_buffer(StreamId{std::string{"thing"}});
    const uint8_t* data = bytes.data();
    ASSERT_THROW(read_identifier(data, bytes.data() + bytes.size() - 1), CodecException);
}

TEST(SegmentIdentifier, TruncatedHeader) {
    auto bytes = identifier_buffer(StreamId{NumericId{42}});
    const uint8_t* data = bytes.data();
    ASSERT_THROW(read_identifier(data, bytes.data() + SegmentIdentifierSize - 1), CodecException);
    ASSERT_THROW(skip_identifier(data, bytes.data() + SegmentIdentifierSize - 1), CodecException);
}

TEST(SegmentIdentifier, FromBytesRejectsOversizedIdentifier) {
    auto seg = encode_bounds_segment();
    std::vector<uint8_t> vec(seg.calculate_size());
    seg.write_to(vec.data());
    ASSERT_EQ(Segment::from_bytes(vec.data(), vec.size()).descriptor().id(), StreamId{std::string{bounds_symbol}});

    auto* identifier = find_string_identifier(vec.data(), vec.data() + vec.size(), bounds_symbol);
    ASSERT_NE(identifier, nullptr);
    identifier->header_.size_ = std::numeric_limits<uint32_t>::max();
    ASSERT_THROW(Segment::from_bytes(vec.data(), vec.size()), CodecException);
}

TEST(SegmentIdentifier, FromBytesRejectsTruncatedHeader) {
    auto seg = encode_bounds_segment();
    std::vector<uint8_t> vec(seg.calculate_size());
    seg.write_to(vec.data());
    ASSERT_THROW(Segment::from_bytes(vec.data(), FIXED_HEADER_SIZE - 1), CodecException);
    ASSERT_THROW(Segment::from_bytes(vec.data(), FIXED_HEADER_SIZE), CodecException);
}

TEST(SegmentIdentifier, DecodeV2RejectsOversizedIdentifier) {
    auto seg = encode_bounds_segment();
    const auto stream_desc = seg.descriptor().clone();
    auto buffer = seg.buffer();
    auto* identifier = find_string_identifier(buffer.data(), buffer.data() + buffer.bytes(), bounds_symbol);
    ASSERT_NE(identifier, nullptr);
    identifier->header_.size_ = std::numeric_limits<uint32_t>::max();
    SegmentInMemory decoded{stream_desc.clone()};
    ASSERT_THROW(decode_v2(seg, seg.header(), decoded, seg.descriptor()), CodecException);
}
