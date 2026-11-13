#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace rr::net {

using ConnectionId = std::uint64_t;
inline constexpr ConnectionId kInvalidConnectionId = 0;

enum class DeliveryMode : std::uint8_t {
    Reliable = 0,
    Unreliable = 1,
};

enum class TransportEventType : std::uint8_t {
    Connected = 0,
    Disconnected = 1,
    Message = 2,
};

struct TransportEvent {
    TransportEventType type{TransportEventType::Message};
    ConnectionId connection{kInvalidConnectionId};
    std::vector<std::byte> payload{};
    std::string detail{};
};

struct Endpoint {
    std::string address{"127.0.0.1"};
    std::uint16_t port{0};
};

/// Transport boundary. The rest of RustyRage must not include a concrete
/// transport SDK. GNS is currently the PROPOSED implementation behind this
/// interface; replacing it must not change rr_net_proto or rr_sim.
class TransportServer {
public:
    virtual ~TransportServer() = default;

    TransportServer(const TransportServer&) = delete;
    TransportServer& operator=(const TransportServer&) = delete;

    virtual bool start(const Endpoint& endpoint, std::string& error) = 0;
    virtual void poll() = 0;
    virtual std::vector<TransportEvent> take_events() = 0;

    virtual bool send(ConnectionId connection,
                      std::span<const std::byte> payload,
                      DeliveryMode mode,
                      std::string& error) = 0;
    virtual bool disconnect(ConnectionId connection, std::string& error) = 0;

    virtual void stop() = 0;
    [[nodiscard]] virtual std::uint16_t local_port() const = 0;

protected:
    TransportServer() = default;
};

class TransportClient {
public:
    virtual ~TransportClient() = default;

    TransportClient(const TransportClient&) = delete;
    TransportClient& operator=(const TransportClient&) = delete;

    virtual bool connect(const Endpoint& endpoint, std::string& error) = 0;
    virtual void poll() = 0;
    virtual std::vector<TransportEvent> take_events() = 0;

    virtual bool send(std::span<const std::byte> payload,
                      DeliveryMode mode,
                      std::string& error) = 0;
    virtual void disconnect() = 0;
    virtual void stop() = 0;

    [[nodiscard]] virtual bool connected() const = 0;

protected:
    TransportClient() = default;
};

[[nodiscard]] std::unique_ptr<TransportServer> create_gns_server();
[[nodiscard]] std::unique_ptr<TransportClient> create_gns_client();

} // namespace rr::net
