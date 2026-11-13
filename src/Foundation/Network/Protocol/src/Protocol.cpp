#include "rr/net/Protocol.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rr::net {
namespace {

bool is_safe_player_name_char(char value) {
    const unsigned char byte = static_cast<unsigned char>(value);
    return byte >= 0x20u && byte <= 0x7Eu;
}

bool is_finite_in_range(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

} // namespace

std::string_view to_string(MessageType type) noexcept {
    switch (type) {
        case MessageType::ClientHello:
            return "ClientHello";
        case MessageType::ServerWelcome:
            return "ServerWelcome";
        case MessageType::ClientInput:
            return "ClientInput";
        case MessageType::ServerTick:
            return "ServerTick";
        case MessageType::Disconnect:
            return "Disconnect";
    }
    return "Unknown";
}

bool is_known_message_type(std::uint8_t raw) noexcept {
    return raw >= static_cast<std::uint8_t>(MessageType::ClientHello) &&
           raw <= static_cast<std::uint8_t>(MessageType::Disconnect);
}

bool validate_frame_header(const FrameHeader& header, std::string& error) {
    if (header.magic != kProtocolMagic) {
        error = "invalid protocol magic";
        return false;
    }
    if (header.protocol_version != kProtocolVersion) {
        error = "unsupported protocol version";
        return false;
    }
    if (!is_known_message_type(static_cast<std::uint8_t>(header.type))) {
        error = "unknown message type";
        return false;
    }
    if (header.payload_size > kMaxPayloadBytes) {
        error = "payload exceeds protocol limit";
        return false;
    }

    error.clear();
    return true;
}

void write_frame_header(core::BinaryWriter& writer, const FrameHeader& header) {
    std::string error;
    if (!validate_frame_header(header, error)) {
        throw std::invalid_argument("FrameHeader::write: " + error);
    }

    writer.write_u32(header.magic);
    writer.write_u16(header.protocol_version);
    writer.write_u8(static_cast<std::uint8_t>(header.type));
    writer.write_u16(header.payload_size);
    writer.write_u32(header.sequence);
}

std::optional<FrameHeader> read_frame_header(core::BinaryReader& reader, std::string& error) {
    FrameHeader header;
    std::uint8_t raw_type = 0;

    if (!reader.read_u32(header.magic) ||
        !reader.read_u16(header.protocol_version) ||
        !reader.read_u8(raw_type) ||
        !reader.read_u16(header.payload_size) ||
        !reader.read_u32(header.sequence)) {
        error = "truncated frame header: ";
        error += reader.error();
        return std::nullopt;
    }

    if (!is_known_message_type(raw_type)) {
        error = "unknown message type";
        return std::nullopt;
    }
    header.type = static_cast<MessageType>(raw_type);

    if (!validate_frame_header(header, error)) {
        return std::nullopt;
    }

    error.clear();
    return header;
}

void write_client_hello(core::BinaryWriter& writer, const ClientHello& message) {
    if (message.client_nonce == 0) {
        throw std::invalid_argument("ClientHello: client_nonce must be non-zero");
    }
    if (message.player_name.empty() || message.player_name.size() > kMaxPlayerNameBytes) {
        throw std::invalid_argument("ClientHello: player_name length is invalid");
    }
    if (!std::all_of(message.player_name.begin(), message.player_name.end(), is_safe_player_name_char)) {
        throw std::invalid_argument("ClientHello: player_name contains control characters");
    }

    writer.write_u32(message.client_nonce);
    writer.write_string(message.player_name, kMaxPlayerNameBytes);
}

std::optional<ClientHello> read_client_hello(core::BinaryReader& reader, std::string& error) {
    ClientHello message;

    if (!reader.read_u32(message.client_nonce) ||
        !reader.read_string(message.player_name, kMaxPlayerNameBytes)) {
        error = "truncated ClientHello: ";
        error += reader.error();
        return std::nullopt;
    }

    if (message.client_nonce == 0) {
        error = "ClientHello: client_nonce must be non-zero";
        return std::nullopt;
    }
    if (message.player_name.empty() ||
        !std::all_of(message.player_name.begin(), message.player_name.end(), is_safe_player_name_char)) {
        error = "ClientHello: player_name is invalid";
        return std::nullopt;
    }

    error.clear();
    return message;
}

void write_server_welcome(core::BinaryWriter& writer, const ServerWelcome& message) {
    if (message.client_id == 0 || message.client_id > kMaxClientId) {
        throw std::invalid_argument("ServerWelcome: client_id is outside the supported range");
    }

    writer.write_u32(message.client_id);
    writer.write_u32(message.server_tick);
}

std::optional<ServerWelcome> read_server_welcome(core::BinaryReader& reader, std::string& error) {
    ServerWelcome message;

    if (!reader.read_u32(message.client_id) ||
        !reader.read_u32(message.server_tick)) {
        error = "truncated ServerWelcome: ";
        error += reader.error();
        return std::nullopt;
    }

    if (message.client_id == 0 || message.client_id > kMaxClientId) {
        error = "ServerWelcome: client_id is outside the supported range";
        return std::nullopt;
    }

    error.clear();
    return message;
}

bool validate_client_input(const ClientInput& input, std::string& error) {
    if (input.tick == 0) {
        error = "ClientInput: tick must be non-zero";
        return false;
    }
    if (!is_finite_in_range(input.throttle, -1.0f, 1.0f)) {
        error = "ClientInput: throttle is out of range";
        return false;
    }
    if (!is_finite_in_range(input.steering, -1.0f, 1.0f)) {
        error = "ClientInput: steering is out of range";
        return false;
    }
    if (!is_finite_in_range(input.brake, 0.0f, 1.0f)) {
        error = "ClientInput: brake is out of range";
        return false;
    }
    if (!is_finite_in_range(input.handbrake, 0.0f, 1.0f)) {
        error = "ClientInput: handbrake is out of range";
        return false;
    }

    error.clear();
    return true;
}

void write_client_input(core::BinaryWriter& writer, const ClientInput& message) {
    std::string error;
    if (!validate_client_input(message, error)) {
        throw std::invalid_argument("ClientInput::write: " + error);
    }

    writer.write_u32(message.tick);
    writer.write_f32(message.throttle);
    writer.write_f32(message.steering);
    writer.write_f32(message.brake);
    writer.write_f32(message.handbrake);
}

std::optional<ClientInput> read_client_input(core::BinaryReader& reader, std::string& error) {
    ClientInput message;

    if (!reader.read_u32(message.tick) ||
        !reader.read_f32(message.throttle) ||
        !reader.read_f32(message.steering) ||
        !reader.read_f32(message.brake) ||
        !reader.read_f32(message.handbrake)) {
        error = "truncated ClientInput: ";
        error += reader.error();
        return std::nullopt;
    }

    if (!validate_client_input(message, error)) {
        return std::nullopt;
    }

    error.clear();
    return message;
}

void write_server_tick(core::BinaryWriter& writer, const ServerTick& message) {
    if (message.tick == 0) {
        throw std::invalid_argument("ServerTick: tick must be non-zero");
    }

    writer.write_u32(message.tick);
    writer.write_u32(message.last_processed_input);
}

std::optional<ServerTick> read_server_tick(core::BinaryReader& reader, std::string& error) {
    ServerTick message;

    if (!reader.read_u32(message.tick) ||
        !reader.read_u32(message.last_processed_input)) {
        error = "truncated ServerTick: ";
        error += reader.error();
        return std::nullopt;
    }

    if (message.tick == 0) {
        error = "ServerTick: tick must be non-zero";
        return std::nullopt;
    }

    error.clear();
    return message;
}

void write_disconnect(core::BinaryWriter& writer, const Disconnect& message) {
    const auto reason = static_cast<std::uint8_t>(message.reason);
    if (reason > static_cast<std::uint8_t>(DisconnectReason::ServerShutdown)) {
        throw std::invalid_argument("Disconnect: reason is outside the supported range");
    }

    writer.write_u8(reason);
}

std::optional<Disconnect> read_disconnect(core::BinaryReader& reader, std::string& error) {
    std::uint8_t raw_reason = 0;
    if (!reader.read_u8(raw_reason)) {
        error = "truncated Disconnect: ";
        error += reader.error();
        return std::nullopt;
    }

    if (raw_reason > static_cast<std::uint8_t>(DisconnectReason::ServerShutdown)) {
        error = "Disconnect: reason is outside the supported range";
        return std::nullopt;
    }

    error.clear();
    return Disconnect{static_cast<DisconnectReason>(raw_reason)};
}

} // namespace rr::net
