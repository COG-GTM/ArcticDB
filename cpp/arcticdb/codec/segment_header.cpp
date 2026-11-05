/* Copyright 2026 Man Group Operations Limited
 *
 * Use of this software is governed by the Business Source License 1.1 included in the file licenses/BSL.txt.
 *
 * As of the Change Date specified in that file, in accordance with the Business Source License, use of this software
 * will be governed by the Apache License, version 2.0.
 */
#include <arcticdb/codec/segment_header.hpp>
#include <arcticdb/column_store/memory_segment.hpp>
#include <arcticdb/column_store/string_pool.hpp>

namespace arcticdb {

size_t field_collection_encoded_field_bytes(const FieldCollection& fields) {
    return calc_field_bytes(
            fields.num_blocks() == 0 ? 0 : fields.num_blocks() + 1
    ); // Non-empty field collection always has shapes buffer
}

void check_expected_bytes_match(std::optional<size_t> expected_bytes, size_t bytes_written) {
    util::check(
            !expected_bytes || (bytes_written == *expected_bytes),
            "Mismatch between actual and expected bytes: {} != {}",
            bytes_written,
            expected_bytes ? *expected_bytes : 0
    );
}

size_t SegmentHeader::serialize_to_bytes(uint8_t* dst, std::optional<size_t> expected_bytes) const {
    const auto* begin = dst;
    data_.field_buffer_.fields_bytes_ = static_cast<uint32_t>(header_fields_.data_bytes());
    data_.field_buffer_.offset_bytes_ = static_cast<uint32_t>(header_fields_.offset_bytes());
    memcpy(dst, &data_, sizeof(HeaderData));
    dst += sizeof(HeaderData);
    ARCTICDB_TRACE(log::codec(), "Wrote header data in {} bytes", dst - begin);
    header_fields_.write_data_to(dst);
    memcpy(dst, header_fields_.offsets_buffer(), header_fields_.offset_bytes());
    dst += header_fields_.offset_bytes();
    memcpy(dst, &offset_, sizeof(offset_));
    ARCTICDB_TRACE(log::codec(), "Wrote header fields in {} bytes", dst - begin);
    dst += sizeof(offset_);
    ARCTICDB_TRACE(log::codec(), "Wrote offsets in {} bytes", dst - begin);
    size_t bytes_written = dst - begin;
    check_expected_bytes_match(expected_bytes, bytes_written);
    ARCTICDB_TRACE(
            log::codec(), "Wrote V2 header with {} bytes ({} expected)", bytes_written, expected_bytes.value_or(0)
    );
    return bytes_written;
}

size_t calc_required_header_fields_bytes(const SegmentInMemory& in_mem_seg) {
    size_t required = 0UL;
    if (in_mem_seg.has_index_descriptor()) {
        const auto index_descriptor_size =
                field_collection_encoded_field_bytes(in_mem_seg.index_descriptor().fields()) + sizeof(uint64_t);
        required += index_descriptor_size;
        ARCTICDB_TRACE(log::codec(), "Index descriptor size {}", index_descriptor_size);
    }

    if (in_mem_seg.has_string_pool()) {
        const auto string_pool_size = calc_field_bytes(in_mem_seg.const_string_pool().num_blocks() + 1) +
                                      sizeof(uint64_t); // String pool has a custom shapes buffer
        required += string_pool_size;
        ARCTICDB_TRACE(log::codec(), "String pool size {}", string_pool_size);
    }

    if (!in_mem_seg.descriptor().empty()) {
        const auto descriptor_size =
                field_collection_encoded_field_bytes(in_mem_seg.descriptor().fields()) + sizeof(uint64_t);
        required += descriptor_size;
        ARCTICDB_TRACE(log::codec(), "Descriptor size {}", descriptor_size);
    }

    // Metadata and column fields are allocated in one contiguous buffer with dimension 1
    if (in_mem_seg.metadata()) {
        const auto metadata_size = calc_field_bytes(2) + sizeof(uint64_t);
        required += metadata_size;
        ARCTICDB_TRACE(log::codec(), "Metadata size {}", metadata_size);
    }

    if (in_mem_seg.row_count() > 0) {
        const auto column_fields_size = calc_field_bytes(1) + sizeof(uint64_t);
        ARCTICDB_TRACE(log::codec(), "Column fields size {}", column_fields_size);
        required += column_fields_size;
    }

    ARCTICDB_TRACE(log::codec(), "Required header bytes: {}", required);
    return required;
}

namespace {
void check_header_bytes_available(size_t required, size_t remaining, std::string_view what) {
    codec::check<ErrorCode::E_DECODE_ERROR>(
            required <= remaining,
            "Truncated segment header reading {}: need {} bytes but only {} remain",
            what,
            required,
            remaining
    );
}
} // namespace

void SegmentHeader::deserialize_from_bytes(const uint8_t* data, size_t header_bytes, bool copy_data) {
    size_t remaining = header_bytes;
    check_header_bytes_available(sizeof(HeaderData), remaining, "header data");
    memcpy(&data_, data, sizeof(HeaderData));
    data += sizeof(HeaderData);
    remaining -= sizeof(HeaderData);
    ChunkedBuffer fields_buffer;
    const auto fields_bytes = data_.field_buffer_.fields_bytes_;
    check_header_bytes_available(fields_bytes, remaining, "header fields");

    if (copy_data) {
        fields_buffer.ensure(fields_bytes);
        memcpy(fields_buffer.data(), data, fields_bytes);
    } else {
        fields_buffer.add_external_block(data, fields_bytes);
    }

    data += fields_bytes;
    remaining -= fields_bytes;
    const auto offset_bytes = data_.field_buffer_.offset_bytes_;
    check_header_bytes_available(offset_bytes, remaining, "header field offsets");
    codec::check<ErrorCode::E_DECODE_ERROR>(
            offset_bytes % sizeof(uint64_t) == 0, "Invalid header field offsets size {}", offset_bytes
    );
    Buffer offsets_buffer{offset_bytes};
    memcpy(offsets_buffer.data(), data, offset_bytes);
    data += offset_bytes;
    remaining -= offset_bytes;
    header_fields_ = EncodedFieldCollection{std::move(fields_buffer), std::move(offsets_buffer)};
    check_header_bytes_available(sizeof(offset_), remaining, "field positions");
    memcpy(offset_.data(), data, sizeof(offset_));
    validate_header_field_bounds();
}

void SegmentHeader::validate_header_field_bounds() const {
    const auto data_bytes = header_fields_.data_bytes();
    for (auto i = 0U; i < static_cast<size_t>(FieldOffset::COUNT); ++i) {
        const auto field_offset = FieldOffset(i);
        if (!has_field(field_offset))
            continue;

        const auto pos = offset_[i];
        codec::check<ErrorCode::E_DECODE_ERROR>(
                pos < header_fields_.size(),
                "Segment header field {} position {} out of range ({} fields)",
                offset_name(field_offset),
                pos,
                header_fields_.size()
        );
        const auto field_start = header_fields_.get_offset(pos);
        codec::check<ErrorCode::E_DECODE_ERROR>(
                field_start <= data_bytes && EncodedFieldImpl::Size <= data_bytes - field_start,
                "Segment header field {} at {} overflows header fields of size {}",
                offset_name(field_offset),
                field_start,
                data_bytes
        );
        const auto field_bytes = encoded_field_bytes(header_fields_.to_field(field_start));
        codec::check<ErrorCode::E_DECODE_ERROR>(
                field_bytes <= data_bytes - field_start,
                "Segment header field {} at {} with {} bytes overflows header fields of size {}",
                offset_name(field_offset),
                field_start,
                field_bytes,
                data_bytes
        );
    }
}

size_t SegmentHeader::required_bytes(const SegmentInMemory& in_mem_seg) {
    size_t required = 0UL;
    required += FIXED_HEADER_SIZE;
    required += sizeof(HeaderData);
    required += sizeof(offset_);
    ARCTICDB_TRACE(
            log::codec(),
            "Overhead size {} + {} + {} = {}",
            FIXED_HEADER_SIZE,
            sizeof(HeaderData),
            sizeof(offset_),
            required
    );

    required += calc_required_header_fields_bytes(in_mem_seg);
    ARCTICDB_TRACE(log::codec(), "Total calculated header size: {}", required);
    return required;
}

} // namespace arcticdb
