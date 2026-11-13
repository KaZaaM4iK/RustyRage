#include "rr/core/BinaryReader.hpp"

#include <bit>
#include <cstring>

namespace rr::core {

BinaryReader::BinaryReader(std::span<const std::byte> data) noexcept
    : data_(data) {
}

bool BinaryReader::require(std::size_t count) {
    if (count > remaining()) {
        return fail("truncated input");
    }
    return true;
}

bool BinaryReader::fail(std::string_view message) {
    if (error_.empty()) {
        error_ = message;
    }
    return false;
}

bool BinaryReader::read_u8(std::uint8_t& value) {
    if (!require(1)) {
        return false;
    }
    value = std::to_integer<std::uint8_t>(data_[offset_]);
    ++offset_;
    return true;
}

bool BinaryReader::read_u16(std::uint16_t& value) {
    if (!require(2)) {
        return false;
    }
    const auto low = std::to_integer<std::uint16_t>(data_[offset_]);
    const auto high = std::to_integer<std::uint16_t>(data_[offset_ + 1]);
    value = static_cast<std::uint16_t>(low | (high << 8u));
    offset_ += 2;
    return true;
}

bool BinaryReader::read_u32(std::uint32_t& value) {
    if (!require(4)) {
        return false;
    }
    std::uint32_t result = 0;
    for (unsigned index = 0; index < 4; ++index) {
        result |= std::to_integer<std::uint32_t>(data_[offset_ + index]) << (index * 8u);
    }
    value = result;
    offset_ += 4;
    return true;
}

bool BinaryReader::read_u64(std::uint64_t& value) {
    if (!require(8)) {
        return false;
    }
    std::uint64_t result = 0;
    for (unsigned index = 0; index < 8; ++index) {
        result |= std::to_integer<std::uint64_t>(data_[offset_ + index]) << (index * 8u);
    }
    value = result;
    offset_ += 8;
    return true;
}

bool BinaryReader::read_i32(std::int32_t& value) {
    std::uint32_t raw = 0;
    if (!read_u32(raw)) {
        return false;
    }
    value = std::bit_cast<std::int32_t>(raw);
    return true;
}

bool BinaryReader::read_f32(float& value) {
    std::uint32_t raw = 0;
    if (!read_u32(raw)) {
        return false;
    }
    value = std::bit_cast<float>(raw);
    return true;
}

bool BinaryReader::read_bool(bool& value) {
    std::uint8_t raw = 0;
    if (!read_u8(raw)) {
        return false;
    }
    if (raw > 1u) {
        return fail("invalid bool encoding");
    }
    value = raw == 1u;
    return true;
}

bool BinaryReader::read_string(std::string& value, std::size_t max_bytes) {
    std::uint16_t length = 0;
    if (!read_u16(length)) {
        return false;
    }
    if (length > max_bytes) {
        return fail("string length exceeds configured bound");
    }
    if (!require(length)) {
        return false;
    }

    const char* first = reinterpret_cast<const char*>(data_.data() + offset_);
    value.assign(first, length);
    offset_ += length;
    return true;
}

bool BinaryReader::read_bytes(std::span<std::byte> destination) {
    if (!require(destination.size())) {
        return false;
    }
    std::memcpy(destination.data(), data_.data() + offset_, destination.size());
    offset_ += destination.size();
    return true;
}

bool BinaryReader::ok() const noexcept {
    return error_.empty();
}

std::string_view BinaryReader::error() const noexcept {
    return error_;
}

std::size_t BinaryReader::position() const noexcept {
    return offset_;
}

std::size_t BinaryReader::remaining() const noexcept {
    return data_.size() - offset_;
}

std::span<const std::byte> BinaryReader::remaining_bytes() const noexcept {
    return data_.subspan(offset_);
}

} // namespace rr::core
