#pragma once

#include "rr/net/Protocol.hpp"
#include "rr/net/Transport.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rr::net {

using ClientId = std::uint32_t;
inline constexpr ClientId kInvalidClientId = 0;

struct ServerClientInfo {
    ClientId id{kInvalidClientId};
    ConnectionId connection{kInvalidConnectionId};
    std::string player_name{};
};

struct ReceivedInput {
    ClientId client{kInvalidClientId};
    ClientInput input{};
};

/// Authoritative server-side network runtime.
///
/// It owns a transport abstraction, validates framing/protocol/message bounds,
/// performs the hello/welcome handshake and exposes validated inputs. It does
/// not know game rules or a concrete transport SDK.
class ServerRuntime {
public:
    explicit ServerRuntime(std::unique_ptr<TransportServer> transport);
    ~ServerRuntime();

    ServerRuntime(const ServerRuntime&) = delete;
    ServerRuntime& operator=(const ServerRuntime&) = delete;

    bool start(const Endpoint& endpoint, std::string& error);
    void poll();

    bool send_tick(ClientId client, const ServerTick& tick, std::string& error);
    bool send_disconnect(ClientId client, DisconnectReason reason, std::string& error);
    bool disconnect_client(ClientId client, std::string& error);

    [[nodiscard]] std::vector<ServerClientInfo> clients() const;
    [[nodiscard]] std::vector<ReceivedInput> consume_inputs();
    [[nodiscard]] std::uint16_t local_port() const;
    [[nodiscard]] std::uint32_t disconnect_messages() const;

    void stop();

private:
    struct Session {
        ClientId id{kInvalidClientId};
        ConnectionId connection{kInvalidConnectionId};
        std::string player_name{};
        bool welcomed{false};
    };

    bool send_frame(ConnectionId connection,
                    MessageType type,
                    core::BinaryWriter& payload,
                    DeliveryMode mode,
                    std::string& error);

    std::unique_ptr<TransportServer> transport_;
    std::vector<Session> sessions_;
    std::vector<ReceivedInput> pending_inputs_;
    std::uint32_t next_client_id_{1};
    std::uint32_t next_sequence_{1};
    std::uint32_t disconnect_messages_{0};
    bool running_{false};
};

/// Client-side network runtime. Owns a transport abstraction and performs
/// explicit protocol framing/handshake. It never assumes unreliable delivery.
class ClientRuntime {
public:
    ClientRuntime(std::unique_ptr<TransportClient> transport,
                  std::string player_name,
                  std::uint32_t client_nonce);
    ~ClientRuntime();

    ClientRuntime(const ClientRuntime&) = delete;
    ClientRuntime& operator=(const ClientRuntime&) = delete;

    bool connect(const Endpoint& endpoint, std::string& error);
    void poll();

    bool send_input(const ClientInput& input, DeliveryMode mode, std::string& error);
    bool send_disconnect(std::string& error);

    [[nodiscard]] bool connected() const;
    [[nodiscard]] bool welcomed() const;
    [[nodiscard]] ClientId client_id() const;
    [[nodiscard]] const std::optional<ServerWelcome>& welcome() const;
    [[nodiscard]] const std::optional<ServerTick>& last_server_tick() const;
    [[nodiscard]] const std::string& last_error() const;

    void disconnect();
    void stop();

private:
    bool send_frame(MessageType type,
                    core::BinaryWriter& payload,
                    DeliveryMode mode,
                    std::string& error);

    std::unique_ptr<TransportClient> transport_;
    std::string player_name_;
    std::uint32_t client_nonce_{0};
    std::uint32_t next_sequence_{1};
    bool hello_sent_{false};
    std::optional<ServerWelcome> welcome_;
    std::optional<ServerTick> last_server_tick_;
    std::string last_error_{};
};

} // namespace rr::net
