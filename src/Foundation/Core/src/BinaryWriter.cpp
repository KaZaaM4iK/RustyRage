#include "rr/core/BinaryWriter.hpp"

#include <limits>
#include <utility>

namespace rr::core {

void BinaryWriter::write_u8(std::uint8_t value) {
    buffer_.push_back(static_cast<std::byte>(value));
}

void BinaryWriter::write_u16(std::uint16_t value) {
    write_u8(static_cast<std::uint8_t>(value & 0xFFu));
    write_u8(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
}

void BinaryWriter::write_u32(std::uint32_t value) {
    write_u8(static_cast<std::uint8_t>(value & 0xFFu));
    write_u8(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
    write_u8(static_cast<std::uint8_t>((value >> 16u) & 0xFFu));
    write_u8(static_cast<std::uint8_t>((value >> 24u) & 0xFFu));
}

void BinaryWriter::write_u64(std::uint64_t value) {
    for (unsigned shift = 0; shift < 64u; shift += 8u) {
        write_u8(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

void BinaryWriter::write_i32(std::int32_t value) {
    write_u32(std::bit_cast<std::uint32_t>(value));
}

void BinaryWriter::write_f32(float value) {
    write_u32(std::bit_cast<std::uint32_t>(value));
}

void BinaryWriter::write_bool(bool value) {
    write_u8(value ? 1u : 0u);
}

void BinaryWriter::write_string(std::string_view value, std::size_t max_bytes) {
    if (max_bytes > std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error("BinaryWriter::write_string max_bytes exceeds u16 range");
    }
    if (value.size() > max_bytes) {
        throw std::length_error("BinaryWriter::write_string value exceeds max_bytes");
    }

    write_u16(static_cast<std::uint16_t>(value.size()));
    if (value.empty()) {
        return;
    }
    const auto* first = reinterpret_cast<const std::byte*>(value.data());
    buffer_.insert(buffer_.end(), first, first + value.size());
}

void BinaryWriter::write_bytes(std::span<const std::byte> value) {
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

std::span<const std::byte> BinaryWriter::bytes() const noexcept {
    return std::span<const std::byte>(buffer_.data(), buffer_.size());
}

std::size_t BinaryWriter::size() const noexcept {
    return buffer_.size();
}

bool BinaryWriter::empty() const noexcept {
    return buffer_.empty();
}

std::vector<std::byte> BinaryWriter::take() {
    return std::move(buffer_);
}

void BinaryWriter::clear() noexcept {
    buffer_.clear();
}

} // namespace rr::core
