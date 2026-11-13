#include "rr/net/Runtime.hpp"

#include "rr/core/BinaryReader.hpp"
#include "rr/core/BinaryWriter.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <utility>

namespace rr::net {
namespace {

bool decode_frame(std::span<const std::byte> bytes,
                  FrameHeader& header,
                  std::span<const std::byte>& payload,
                  std::string& error) {
    core::BinaryReader reader(bytes);
    const auto decoded = read_frame_header(reader, error);
    if (!decoded.has_value()) {
        return false;
    }
    if (decoded->sequence == 0) {
        error = "frame sequence must be non-zero";
        return false;
    }
    if (decoded->payload_size != reader.remaining()) {
        error = "frame payload size does not match the received bytes";
        return false;
    }

    header = *decoded;
    payload = reader.remaining_bytes();
    return true;
}

} // namespace

ServerRuntime::ServerRuntime(std::unique_ptr<TransportServer> transport)
    : transport_(std::move(transport)) {
}

ServerRuntime::~ServerRuntime() {
    stop();
}

bool ServerRuntime::start(const Endpoint& endpoint, std::string& error) {
    if (transport_ == nullptr) {
        error = "server transport is not available";
        return false;
    }
    if (running_) {
        error = "server runtime already started";
        return false;
    }
    if (!transport_->start(endpoint, error)) {
        return false;
    }
    running_ = true;
    error.clear();
    return true;
}

void ServerRuntime::poll() {
    if (!running_ || transport_ == nullptr) {
        return;
    }

    transport_->poll();
    const std::vector<TransportEvent> events = transport_->take_events();

    for (const TransportEvent& event : events) {
        switch (event.type) {
            case TransportEventType::Connected: {
                const auto exists = std::find_if(
                    sessions_.begin(), sessions_.end(),
                    [&event](const Session& session) {
                        return session.connection == event.connection;
                    });
                if (exists == sessions_.end()) {
                    Session session;
                    session.connection = event.connection;
                    sessions_.push_back(std::move(session));
                }
                break;
            }

            case TransportEventType::Disconnected: {
                sessions_.erase(
                    std::remove_if(sessions_.begin(), sessions_.end(),
                                   [&event](const Session& session) {
                                       return session.connection == event.connection;
                                   }),
                    sessions_.end());
                break;
            }

            case TransportEventType::Message: {
                const auto session_iterator = std::find_if(
                    sessions_.begin(), sessions_.end(),
                    [&event](const Session& session) {
                        return session.connection == event.connection;
                    });
                if (session_iterator == sessions_.end()) {
                    break;
                }

                FrameHeader header;
                std::span<const std::byte> payload;
                std::string error;
                if (!decode_frame(event.payload, header, payload, error)) {
                    transport_->disconnect(event.connection, error);
                    break;
                }

                core::BinaryReader reader(payload);
                if (header.type == MessageType::ClientHello) {
                    if (session_iterator->id != kInvalidClientId) {
                        break;
                    }
                    const auto hello = read_client_hello(reader, error);
                    if (!hello.has_value()) {
                        transport_->disconnect(event.connection, error);
                        break;
                    }
                    if (next_client_id_ > kMaxClientId) {
                        transport_->disconnect(event.connection, error);
                        break;
                    }

                    session_iterator->id = next_client_id_++;
                    session_iterator->player_name = hello->player_name;

                    ServerWelcome welcome;
                    welcome.client_id = session_iterator->id;
                    welcome.server_tick = 0;

                    core::BinaryWriter welcome_writer;
                    write_server_welcome(welcome_writer, welcome);
                    if (!send_frame(session_iterator->connection,
                                    MessageType::ServerWelcome,
                                    welcome_writer,
                                    DeliveryMode::Reliable,
                                    error)) {
                        transport_->disconnect(event.connection, error);
                        break;
                    }
                    session_iterator->welcomed = true;
                } else if (header.type == MessageType::ClientInput) {
                    if (!session_iterator->welcomed) {
                        break;
                    }
                    const auto input = read_client_input(reader, error);
                    if (!input.has_value()) {
                        transport_->disconnect(event.connection, error);
                        break;
                    }
                    pending_inputs_.push_back(ReceivedInput{session_iterator->id, *input});
                } else if (header.type == MessageType::Disconnect) {
                    const auto disconnect_message = read_disconnect(reader, error);
                    if (!disconnect_message.has_value()) {
                        transport_->disconnect(event.connection, error);
                    } else {
                        ++disconnect_messages_;
                    }
                } else {
                    // Client-to-server direction only accepts hello, input and
                    // disconnect. Other known message types are ignored.
                }
                break;
            }
        }
    }
}

bool ServerRuntime::send_tick(ClientId client, const ServerTick& tick, std::string& error) {
    const auto iterator = std::find_if(
        sessions_.begin(), sessions_.end(),
        [client](const Session& session) {
            return session.id == client;
        });
    if (iterator == sessions_.end() || !iterator->welcomed) {
        error = "unknown or unwelcomed client";
        return false;
    }

    core::BinaryWriter payload;
    write_server_tick(payload, tick);
    return send_frame(iterator->connection,
                      MessageType::ServerTick,
                      payload,
                      DeliveryMode::Reliable,
                      error);
}

bool ServerRuntime::send_disconnect(ClientId client, DisconnectReason reason, std::string& error) {
    const auto iterator = std::find_if(
        sessions_.begin(), sessions_.end(),
        [client](const Session& session) {
            return session.id == client;
        });
    if (iterator == sessions_.end() || !iterator->welcomed) {
        error = "unknown or unwelcomed client";
        return false;
    }

    core::BinaryWriter payload;
    write_disconnect(payload, Disconnect{reason});
    return send_frame(iterator->connection,
                      MessageType::Disconnect,
                      payload,
                      DeliveryMode::Reliable,
                      error);
}

bool ServerRuntime::disconnect_client(ClientId client, std::string& error) {
    const auto iterator = std::find_if(
        sessions_.begin(), sessions_.end(),
        [client](const Session& session) {
            return session.id == client;
        });
    if (iterator == sessions_.end()) {
        error = "unknown client";
        return false;
    }
    return transport_->disconnect(iterator->connection, error);
}

std::vector<ServerClientInfo> ServerRuntime::clients() const {
    std::vector<ServerClientInfo> result;
    for (const Session& session : sessions_) {
        if (session.id != kInvalidClientId) {
            result.push_back(ServerClientInfo{session.id, session.connection, session.player_name});
        }
    }
    return result;
}

std::vector<ReceivedInput> ServerRuntime::consume_inputs() {
    std::vector<ReceivedInput> result;
    result.swap(pending_inputs_);
    return result;
}

std::uint16_t ServerRuntime::local_port() const {
    return transport_ != nullptr ? transport_->local_port() : 0;
}

std::uint32_t ServerRuntime::disconnect_messages() const {
    return disconnect_messages_;
}

void ServerRuntime::stop() {
    if (transport_ != nullptr) {
        transport_->stop();
    }
    sessions_.clear();
    pending_inputs_.clear();
    running_ = false;
}

bool ServerRuntime::send_frame(ConnectionId connection,
                               MessageType type,
                               core::BinaryWriter& payload,
                               DeliveryMode mode,
                               std::string& error) {
    const std::vector<std::byte> payload_bytes = payload.take();
    if (payload_bytes.size() > kMaxPayloadBytes) {
        error = "server payload exceeds protocol limit";
        return false;
    }

    FrameHeader header;
    header.type = type;
    header.payload_size = static_cast<std::uint16_t>(payload_bytes.size());
    header.sequence = next_sequence_++;

    core::BinaryWriter frame;
    write_frame_header(frame, header);
    frame.write_bytes(payload_bytes);
    return transport_->send(connection, frame.bytes(), mode, error);
}

ClientRuntime::ClientRuntime(std::unique_ptr<TransportClient> transport,
                             std::string player_name,
                             std::uint32_t client_nonce)
    : transport_(std::move(transport)),
      player_name_(std::move(player_name)),
      client_nonce_(client_nonce) {
}

ClientRuntime::~ClientRuntime() {
    stop();
}

bool ClientRuntime::connect(const Endpoint& endpoint, std::string& error) {
    if (transport_ == nullptr) {
        error = "client transport is not available";
        return false;
    }
    if (!transport_->connect(endpoint, error)) {
        return false;
    }
    last_error_.clear();
    return true;
}

void ClientRuntime::poll() {
    if (transport_ == nullptr) {
        return;
    }

    transport_->poll();
    const std::vector<TransportEvent> events = transport_->take_events();

    for (const TransportEvent& event : events) {
        switch (event.type) {
            case TransportEventType::Connected: {
                if (!hello_sent_) {
                    ClientHello hello;
                    hello.client_nonce = client_nonce_;
                    hello.player_name = player_name_;

                    core::BinaryWriter payload;
                    std::string error;
                    try {
                        write_client_hello(payload, hello);
                    } catch (const std::exception& exception) {
                        last_error_ = exception.what();
                        break;
                    }
                    if (send_frame(MessageType::ClientHello,
                                   payload,
                                   DeliveryMode::Reliable,
                                   error)) {
                        hello_sent_ = true;
                    } else {
                        last_error_ = error;
                    }
                }
                break;
            }

            case TransportEventType::Disconnected:
                if (!event.detail.empty()) {
                    last_error_ = event.detail;
                }
                break;

            case TransportEventType::Message: {
                FrameHeader header;
                std::span<const std::byte> payload;
                std::string error;
                if (!decode_frame(event.payload, header, payload, error)) {
                    last_error_ = error;
                    break;
                }

                core::BinaryReader reader(payload);
                if (header.type == MessageType::ServerWelcome) {
                    const auto welcome = read_server_welcome(reader, error);
                    if (!welcome.has_value()) {
                        last_error_ = error;
                    } else {
                        welcome_ = *welcome;
                    }
                } else if (header.type == MessageType::ServerTick) {
                    const auto tick = read_server_tick(reader, error);
                    if (!tick.has_value()) {
                        last_error_ = error;
                    } else {
                        last_server_tick_ = *tick;
                    }
                } else if (header.type == MessageType::Disconnect) {
                    const auto disconnect = read_disconnect(reader, error);
                    if (!disconnect.has_value()) {
                        last_error_ = error;
                    } else if (!last_error_.empty()) {
                        // Keep the transport-level error detail when present.
                    }
                }
                break;
            }
        }
    }
}

bool ClientRuntime::send_input(const ClientInput& input, DeliveryMode mode, std::string& error) {
    if (!welcome_.has_value()) {
        error = "client has not completed the welcome handshake";
        return false;
    }

    std::string validation_error;
    if (!validate_client_input(input, validation_error)) {
        error = validation_error;
        return false;
    }

    core::BinaryWriter payload;
    write_client_input(payload, input);
    return send_frame(MessageType::ClientInput, payload, mode, error);
}

bool ClientRuntime::send_disconnect(std::string& error) {
    core::BinaryWriter payload;
    write_disconnect(payload, Disconnect{DisconnectReason::Normal});
    return send_frame(MessageType::Disconnect, payload, DeliveryMode::Reliable, error);
}

bool ClientRuntime::connected() const {
    return transport_ != nullptr && transport_->connected();
}

bool ClientRuntime::welcomed() const {
    return welcome_.has_value();
}

ClientId ClientRuntime::client_id() const {
    return welcome_.has_value() ? welcome_->client_id : kInvalidClientId;
}

const std::optional<ServerWelcome>& ClientRuntime::welcome() const {
    return welcome_;
}

const std::optional<ServerTick>& ClientRuntime::last_server_tick() const {
    return last_server_tick_;
}

const std::string& ClientRuntime::last_error() const {
    return last_error_;
}

void ClientRuntime::disconnect() {
    if (transport_ != nullptr) {
        transport_->disconnect();
    }
}

void ClientRuntime::stop() {
    if (transport_ != nullptr) {
        transport_->stop();
    }
    hello_sent_ = false;
    welcome_.reset();
    last_server_tick_.reset();
}

bool ClientRuntime::send_frame(MessageType type,
                               core::BinaryWriter& payload,
                               DeliveryMode mode,
                               std::string& error) {
    const std::vector<std::byte> payload_bytes = payload.take();
    if (payload_bytes.size() > kMaxPayloadBytes) {
        error = "client payload exceeds protocol limit";
        return false;
    }

    FrameHeader header;
    header.type = type;
    header.payload_size = static_cast<std::uint16_t>(payload_bytes.size());
    header.sequence = next_sequence_++;

    core::BinaryWriter frame;
    write_frame_header(frame, header);
    frame.write_bytes(payload_bytes);
    return transport_->send(frame.bytes(), mode, error);
}

} // namespace rr::net
