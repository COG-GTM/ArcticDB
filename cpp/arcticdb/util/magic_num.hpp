/* Copyright 2026 Man Group Operations Limited
 *
 * Use of this software is governed by the Business Source License 1.1 included in the file licenses/BSL.txt.
 *
 * As of the Change Date specified in that file, in accordance with the Business Source License, use of this software
 * will be governed by the Apache License, version 2.0.
 */

#pragma once

#include <arcticdb/util/preconditions.hpp>

#include <cstdint>
#include <cstddef>
#include <climits>
#include <string_view>

namespace arcticdb::util {

// Validates that `size` bytes can be read starting at `ptr` without running past `end`.
// When `end` is null the caller has not supplied a bound and the check is skipped, preserving
// the behaviour of decode paths that operate on already-validated in-memory data.
inline void check_buffer_bounds(const uint8_t* ptr, std::size_t size, const uint8_t* end, std::string_view context) {
    if (end == nullptr)
        return;
    const auto remaining = ptr <= end ? static_cast<std::size_t>(end - ptr) : std::size_t(0);
    codec::check<ErrorCode::E_DECODE_ERROR>(
            ptr <= end && size <= remaining,
            "Buffer overflow while decoding {}: require {} bytes but only {} remain before the end of the segment",
            context,
            size,
            remaining
    );
}

template<char a, char b, char c, char d>
struct MagicNum {
    static constexpr uint64_t Magic =
            a << (CHAR_BIT * 0) | b << (CHAR_BIT * 1) | c << (CHAR_BIT * 2) | d << (CHAR_BIT * 3);

    ~MagicNum() { magic_ = ~magic_; }

    // Set log_only to true if calling from destructors to avoid undefined behaviour of throwing
    void check(bool log_only = false) const {
        if (magic_ != Magic) {
            std::string_view expected(reinterpret_cast<const char*>(&Magic), 4);
            std::string message(fmt::format("Magic number failure, expected {}({}) got {}", Magic, expected, magic_));
            if (log_only) {
                log::version().warn(message);
            } else {
                util::raise_rte(message);
            }
        }
    }

  private:
    volatile uint64_t magic_ = Magic;
};

template<char a, char b>
struct SmallMagicNum {
    static constexpr uint16_t Magic = a << CHAR_BIT * 0 | b << CHAR_BIT * 1;

    ~SmallMagicNum() { magic_ = ~magic_; }

    [[nodiscard]] uint16_t magic() const { return magic_; }

    void check() const {
        std::string_view expected(reinterpret_cast<const char*>(&Magic), 2);
        util::check(magic_ == Magic, "Small magic number failure, expected {}({}) got {}", Magic, expected, magic_);
    }

  private:
    volatile uint16_t magic_ = Magic;
};

template<typename MagicNumType>
void check_magic_in_place(const uint8_t*& pos) {
    const auto magic_num = reinterpret_cast<const MagicNumType*>(pos);
    magic_num->check();
}

template<typename MagicNumType>
void check_magic(const uint8_t*& pos) {
    check_magic_in_place<MagicNumType>(pos);
    pos += sizeof(MagicNumType);
}

// Bounds-checked variant: verifies that the magic number lies within `[pos, end)` before reading it,
// so a truncated or forged on-disk header cannot drive a read past the segment buffer.
template<typename MagicNumType>
void check_magic(const uint8_t*& pos, const uint8_t* end) {
    check_buffer_bounds(pos, sizeof(MagicNumType), end, "magic number");
    check_magic_in_place<MagicNumType>(pos);
    pos += sizeof(MagicNumType);
}

template<typename MagicNumType>
void write_magic(uint8_t*& pos) {
    const auto magic_num = new (pos) MagicNumType;
    magic_num->check();
    pos += sizeof(MagicNumType);
}

} // namespace arcticdb::util
