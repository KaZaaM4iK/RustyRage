#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rr::core {

/// Explicit little-endian binary writer.
///
/// The writer never serializes C++ object layouts. Every field is written with
/// a defined width and byte order. Strings are length-prefixed with a u16 and
/// are bounded by the caller-supplied maximum.
class BinaryWriter {
public:
    void write_u8(std::uint8_t value);
    void write_u16(std::uint16_t value);
    void write_u32(std::uint32_t value);
    void write_u64(std::uint64_t value);
    void write_i32(std::int32_t value);
    void write_f32(float value);
    void write_bool(bool value);

    /// Writes a u16 byte length followed by the exact bytes.
    /// Throws std::length_error when the value exceeds max_bytes or 65535.
    void write_string(std::string_view value, std::size_t max_bytes);

    void write_bytes(std::span<const std::byte> value);

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    std::vector<std::byte> take();
    void clear() noexcept;

private:
    std::vector<std::byte> buffer_;
};

} // namespace rr::core
