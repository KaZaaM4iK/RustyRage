#pragma once

#include "rr/core/BinaryReader.hpp"
#include "rr/core/BinaryWriter.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rr::net {

/// Explicit, versioned wire protocol.
///
/// Every field is serialized with a defined width and little-endian byte order.
/// No C++ structure is ever copied into the wire format. Incoming messages are
/// untrusted: type, version, payload size, ranges and string bounds are checked
/// before use.

inline constexpr std::uint32_t kProtocolMagic = 0x52525046u; // "RRPF"
inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::uint16_t kMaxPayloadBytes = 1024;
inline constexpr std::size_t kMaxPlayerNameBytes = 32;
inline constexpr std::uint32_t kMaxClientId = 8;

enum class MessageType : std::uint8_t {
    ClientHello = 1,
    ServerWelcome = 2,
    ClientInput = 3,
    ServerTick = 4,
    Disconnect = 5,
};

[[nodiscard]] std::string_view to_string(MessageType type) noexcept;
[[nodiscard]] bool is_known_message_type(std::uint8_t raw) noexcept;

struct FrameHeader {
    std::uint32_t magic{kProtocolMagic};
    std::uint16_t protocol_version{kProtocolVersion};
    MessageType type{MessageType::ClientHello};
    std::uint16_t payload_size{0};
    std::uint32_t sequence{0};
};

void write_frame_header(core::BinaryWriter& writer, const FrameHeader& header);
[[nodiscard]] std::optional<FrameHeader> read_frame_header(
    core::BinaryReader& reader,
    std::string& error);

bool validate_frame_header(const FrameHeader& header, std::string& error);

struct ClientHello {
    std::uint32_t client_nonce{0};
    std::string player_name{};
};

void write_client_hello(core::BinaryWriter& writer, const ClientHello& message);
[[nodiscard]] std::optional<ClientHello> read_client_hello(
    core::BinaryReader& reader,
    std::string& error);

struct ServerWelcome {
    std::uint32_t client_id{0};
    std::uint32_t server_tick{0};
};

void write_server_welcome(core::BinaryWriter& writer, const ServerWelcome& message);
[[nodiscard]] std::optional<ServerWelcome> read_server_welcome(
    core::BinaryReader& reader,
    std::string& error);

struct ClientInput {
    std::uint32_t tick{0};
    float throttle{0.0f};
    float steering{0.0f};
    float brake{0.0f};
    float handbrake{0.0f};
};

bool validate_client_input(const ClientInput& input, std::string& error);

void write_client_input(core::BinaryWriter& writer, const ClientInput& message);
[[nodiscard]] std::optional<ClientInput> read_client_input(
    core::BinaryReader& reader,
    std::string& error);

struct ServerTick {
    std::uint32_t tick{0};
    std::uint32_t last_processed_input{0};
};

void write_server_tick(core::BinaryWriter& writer, const ServerTick& message);
[[nodiscard]] std::optional<ServerTick> read_server_tick(
    core::BinaryReader& reader,
    std::string& error);

enum class DisconnectReason : std::uint8_t {
    Normal = 0,
    ProtocolError = 1,
    Timeout = 2,
    ServerShutdown = 3,
};

struct Disconnect {
    DisconnectReason reason{DisconnectReason::Normal};
};

void write_disconnect(core::BinaryWriter& writer, const Disconnect& message);
[[nodiscard]] std::optional<Disconnect> read_disconnect(
    core::BinaryReader& reader,
    std::string& error);

} // namespace rr::net
