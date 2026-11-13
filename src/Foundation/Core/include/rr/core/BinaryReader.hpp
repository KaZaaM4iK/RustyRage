#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rr::core {

/// Bounds-checked little-endian reader for explicitly serialized fields.
///
/// A failed read leaves the reader in a failed state and does not advance the
/// read position. Callers must check the returned bool (or ok()) before using
/// an output value.
class BinaryReader {
public:
    explicit BinaryReader(std::span<const std::byte> data) noexcept;

    bool read_u8(std::uint8_t& value);
    bool read_u16(std::uint16_t& value);
    bool read_u32(std::uint32_t& value);
    bool read_u64(std::uint64_t& value);
    bool read_i32(std::int32_t& value);
    bool read_f32(float& value);
    bool read_bool(bool& value);

    /// Reads a u16 length and then that many bytes.
    /// Fails when the length exceeds max_bytes or the remaining input.
    bool read_string(std::string& value, std::size_t max_bytes);

    bool read_bytes(std::span<std::byte> destination);

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] std::string_view error() const noexcept;
    [[nodiscard]] std::size_t position() const noexcept;
    [[nodiscard]] std::size_t remaining() const noexcept;
    [[nodiscard]] std::span<const std::byte> remaining_bytes() const noexcept;

private:
    bool fail(std::string_view message);
    bool require(std::size_t count);

    std::span<const std::byte> data_;
    std::size_t offset_{0};
    std::string_view error_{};
};

} // namespace rr::core
