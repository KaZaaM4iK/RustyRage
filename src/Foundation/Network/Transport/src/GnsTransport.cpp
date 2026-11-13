#include "rr/net/Transport.hpp"

#include <steam/isteamnetworkingutils.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rr::net {
namespace {

constexpr std::size_t kMaxTransportPayloadBytes = 64u * 1024u;

class GnsServer;
class GnsClient;

void connection_status_callback(SteamNetConnectionStatusChangedCallback_t* info);

class CallbackRegistry {
public:
    static CallbackRegistry& instance() {
        static CallbackRegistry registry;
        return registry;
    }

    void add_server(HSteamListenSocket socket, GnsServer* server) {
        std::lock_guard<std::mutex> lock(mutex_);
        servers_[socket] = server;
    }

    void remove_server(HSteamListenSocket socket) {
        std::lock_guard<std::mutex> lock(mutex_);
        servers_.erase(socket);
    }

    GnsServer* find_server(HSteamListenSocket socket) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto iterator = servers_.find(socket);
        return iterator == servers_.end() ? nullptr : iterator->second;
    }

    void add_client(HSteamNetConnection connection, GnsClient* client) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[connection] = client;
    }

    void remove_client(HSteamNetConnection connection) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(connection);
    }

    GnsClient* find_client(HSteamNetConnection connection) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto iterator = clients_.find(connection);
        return iterator == clients_.end() ? nullptr : iterator->second;
    }

private:
    std::mutex mutex_;
    std::unordered_map<HSteamListenSocket, GnsServer*> servers_;
    std::unordered_map<HSteamNetConnection, GnsClient*> clients_;
};

class GnsRuntime {
public:
    static GnsRuntime& instance() {
        static GnsRuntime runtime;
        return runtime;
    }

    bool acquire(std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (references_ == 0) {
            SteamNetworkingErrMsg err{};
            if (!GameNetworkingSockets_Init(nullptr, err)) {
                error = "GameNetworkingSockets_Init failed: ";
                error += err;
                return false;
            }
        }
        ++references_;
        error.clear();
        return true;
    }

    void release() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (references_ == 0) {
            return;
        }
        --references_;
        if (references_ == 0) {
            GameNetworkingSockets_Kill();
        }
    }

    ISteamNetworkingSockets* sockets() {
        return SteamNetworkingSockets();
    }

    void run_callbacks() {
        std::lock_guard<std::mutex> lock(mutex_);
        ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
        if (sockets != nullptr) {
            sockets->RunCallbacks();
        }
    }

private:
    GnsRuntime() = default;
    ~GnsRuntime() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (references_ != 0) {
            references_ = 0;
            GameNetworkingSockets_Kill();
        }
    }

    std::mutex mutex_;
    int references_{0};
};

bool parse_endpoint(const Endpoint& endpoint, SteamNetworkingIPAddr& address, std::string& error) {
    address.Clear();
    if (!endpoint.address.empty()) {
        if (!address.ParseString(endpoint.address.c_str())) {
            error = "invalid endpoint address: " + endpoint.address;
            return false;
        }
    }
    if (endpoint.port != 0) {
        address.m_port = endpoint.port;
    }
    return true;
}

int delivery_flags(DeliveryMode mode) {
    return mode == DeliveryMode::Reliable
               ? k_nSteamNetworkingSend_ReliableNoNagle
               : k_nSteamNetworkingSend_UnreliableNoDelay;
}

class GnsServer final : public TransportServer {
public:
    ~GnsServer() override {
        stop();
    }

    bool start(const Endpoint& endpoint, std::string& error) override {
        if (running_) {
            error = "server already started";
            return false;
        }

        if (!runtime_.acquire(error)) {
            return false;
        }
        runtime_acquired_ = true;

        sockets_ = GnsRuntime::instance().sockets();
        if (sockets_ == nullptr) {
            error = "SteamNetworkingSockets() returned null";
            release_runtime();
            return false;
        }

        SteamNetworkingIPAddr address;
        if (!parse_endpoint(endpoint, address, error)) {
            release_runtime();
            return false;
        }

        SteamNetworkingConfigValue_t option;
        option.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                      reinterpret_cast<void*>(&connection_status_callback));

        listen_socket_ = sockets_->CreateListenSocketIP(address, 1, &option);
        if (listen_socket_ == k_HSteamListenSocket_Invalid) {
            error = "GNS CreateListenSocketIP failed";
            release_runtime();
            return false;
        }

        CallbackRegistry::instance().add_server(listen_socket_, this);
        running_ = true;

        SteamNetworkingIPAddr bound{};
        if (sockets_->GetListenSocketAddress(listen_socket_, &bound)) {
            local_port_ = bound.m_port;
        } else {
            local_port_ = endpoint.port;
        }

        error.clear();
        return true;
    }

    void poll() override {
        if (!running_) {
            return;
        }

        GnsRuntime::instance().run_callbacks();

        std::vector<HSteamNetConnection> handles;
        handles.reserve(connections_.size());
        for (const auto& entry : connections_) {
            handles.push_back(entry.first);
        }

        for (HSteamNetConnection handle : handles) {
            const auto connection_iterator = connections_.find(handle);
            if (connection_iterator == connections_.end() || !connection_iterator->second.connected) {
                continue;
            }

            SteamNetworkingMessage_t* messages[32]{};
            const int count = sockets_->ReceiveMessagesOnConnection(handle, messages, 32);
            if (count <= 0) {
                continue;
            }

            for (int index = 0; index < count; ++index) {
                SteamNetworkingMessage_t* message = messages[index];
                if (message == nullptr) {
                    continue;
                }
                if (message->m_cbSize > 0 &&
                    static_cast<std::size_t>(message->m_cbSize) <= kMaxTransportPayloadBytes) {
                    std::vector<std::byte> payload(static_cast<std::size_t>(message->m_cbSize));
                    std::memcpy(payload.data(), message->m_pData, payload.size());
                    emit_message(connection_iterator->second.id, std::move(payload));
                }
                message->Release();
            }
        }
    }

    std::vector<TransportEvent> take_events() override {
        std::lock_guard<std::mutex> lock(events_mutex_);
        std::vector<TransportEvent> result;
        result.swap(events_);
        return result;
    }

    bool send(ConnectionId connection,
              std::span<const std::byte> payload,
              DeliveryMode mode,
              std::string& error) override {
        const auto iterator = handles_by_id_.find(connection);
        if (iterator == handles_by_id_.end()) {
            error = "unknown connection";
            return false;
        }
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            error = "payload too large";
            return false;
        }

        const EResult result = sockets_->SendMessageToConnection(
            iterator->second,
            payload.data(),
            static_cast<std::uint32_t>(payload.size()),
            delivery_flags(mode),
            nullptr);

        if (result == k_EResultOK) {
            error.clear();
            return true;
        }
        if (mode == DeliveryMode::Unreliable && result == k_EResultIgnored) {
            // The transport contract allows unreliable messages to be absent.
            error.clear();
            return true;
        }

        error = "GNS send failed with EResult ";
        error += std::to_string(static_cast<int>(result));
        return false;
    }

    bool disconnect(ConnectionId connection, std::string& error) override {
        const auto iterator = handles_by_id_.find(connection);
        if (iterator == handles_by_id_.end()) {
            error = "unknown connection";
            return false;
        }

        if (!sockets_->CloseConnection(iterator->second,
                                       k_ESteamNetConnectionEnd_App_Generic,
                                       "server requested disconnect",
                                       false)) {
            error = "GNS CloseConnection failed";
            return false;
        }
        error.clear();
        return true;
    }

    void stop() override {
        if (!running_ && !runtime_acquired_) {
            return;
        }

        if (sockets_ != nullptr) {
            for (const auto& entry : connections_) {
                sockets_->CloseConnection(entry.first,
                                          k_ESteamNetConnectionEnd_App_Generic,
                                          "server stopping",
                                          false);
            }
            connections_.clear();
            handles_by_id_.clear();

            if (listen_socket_ != k_HSteamListenSocket_Invalid) {
                CallbackRegistry::instance().remove_server(listen_socket_);
                sockets_->CloseListenSocket(listen_socket_);
                listen_socket_ = k_HSteamListenSocket_Invalid;
            }
        }

        running_ = false;
        release_runtime();
    }

    [[nodiscard]] std::uint16_t local_port() const override {
        return local_port_;
    }

    void on_status(SteamNetConnectionStatusChangedCallback_t* info) {
        if (info == nullptr || sockets_ == nullptr) {
            return;
        }

        const HSteamNetConnection handle = info->m_hConn;

        switch (info->m_info.m_eState) {
            case k_ESteamNetworkingConnectionState_Connecting: {
                const EResult result = sockets_->AcceptConnection(handle);
                if (result != k_EResultOK) {
                    sockets_->CloseConnection(handle, 0, "accept failed", false);
                }
                break;
            }

            case k_ESteamNetworkingConnectionState_Connected: {
                auto iterator = connections_.find(handle);
                if (iterator == connections_.end()) {
                    Connection connection;
                    connection.handle = handle;
                    connection.id = next_connection_id_++;
                    connection.connected = true;
                    iterator = connections_.emplace(handle, connection).first;
                    handles_by_id_[connection.id] = handle;
                    emit_connected(connection.id);
                } else {
                    iterator->second.connected = true;
                }
                break;
            }

            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
                const auto iterator = connections_.find(handle);
                if (iterator != connections_.end()) {
                    emit_disconnected(iterator->second.id, info->m_info.m_szEndDebug);
                    handles_by_id_.erase(iterator->second.id);
                    connections_.erase(iterator);
                }
                sockets_->CloseConnection(handle, 0, "connection closed", false);
                break;
            }

            case k_ESteamNetworkingConnectionState_None:
            default:
                break;
        }
    }

private:
    struct Connection {
        HSteamNetConnection handle{k_HSteamNetConnection_Invalid};
        ConnectionId id{kInvalidConnectionId};
        bool connected{false};
    };

    void emit_connected(ConnectionId id) {
        TransportEvent event;
        event.type = TransportEventType::Connected;
        event.connection = id;
        emit(std::move(event));
    }

    void emit_disconnected(ConnectionId id, const char* detail) {
        TransportEvent event;
        event.type = TransportEventType::Disconnected;
        event.connection = id;
        event.detail = detail != nullptr ? detail : "";
        emit(std::move(event));
    }

    void emit_message(ConnectionId id, std::vector<std::byte> payload) {
        TransportEvent event;
        event.type = TransportEventType::Message;
        event.connection = id;
        event.payload = std::move(payload);
        emit(std::move(event));
    }

    void emit(TransportEvent event) {
        std::lock_guard<std::mutex> lock(events_mutex_);
        events_.push_back(std::move(event));
    }

    void release_runtime() {
        if (runtime_acquired_) {
            runtime_.release();
            runtime_acquired_ = false;
        }
    }

    GnsRuntime& runtime_{GnsRuntime::instance()};
    ISteamNetworkingSockets* sockets_{nullptr};
    HSteamListenSocket listen_socket_{k_HSteamListenSocket_Invalid};
    std::uint16_t local_port_{0};
    bool running_{false};
    bool runtime_acquired_{false};
    ConnectionId next_connection_id_{1};

    std::unordered_map<HSteamNetConnection, Connection> connections_;
    std::unordered_map<ConnectionId, HSteamNetConnection> handles_by_id_;
    std::mutex events_mutex_;
    std::vector<TransportEvent> events_;
};

class GnsClient final : public TransportClient {
public:
    ~GnsClient() override {
        stop();
    }

    bool connect(const Endpoint& endpoint, std::string& error) override {
        if (connection_ != k_HSteamNetConnection_Invalid) {
            error = "client already connected";
            return false;
        }

        if (!runtime_.acquire(error)) {
            return false;
        }
        runtime_acquired_ = true;

        sockets_ = GnsRuntime::instance().sockets();
        if (sockets_ == nullptr) {
            error = "SteamNetworkingSockets() returned null";
            release_runtime();
            return false;
        }

        SteamNetworkingIPAddr address;
        if (!parse_endpoint(endpoint, address, error)) {
            release_runtime();
            return false;
        }

        SteamNetworkingConfigValue_t option;
        option.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                      reinterpret_cast<void*>(&connection_status_callback));

        connection_ = sockets_->ConnectByIPAddress(address, 1, &option);
        if (connection_ == k_HSteamNetConnection_Invalid) {
            error = "GNS ConnectByIPAddress failed";
            release_runtime();
            return false;
        }

        CallbackRegistry::instance().add_client(connection_, this);
        connected_.store(false, std::memory_order_relaxed);
        error.clear();
        return true;
    }

    void poll() override {
        if (sockets_ == nullptr) {
            return;
        }

        GnsRuntime::instance().run_callbacks();

        if (connection_ == k_HSteamNetConnection_Invalid ||
            !connected_.load(std::memory_order_relaxed)) {
            return;
        }

        SteamNetworkingMessage_t* messages[32]{};
        const int count = sockets_->ReceiveMessagesOnConnection(connection_, messages, 32);
        if (count <= 0) {
            return;
        }

        for (int index = 0; index < count; ++index) {
            SteamNetworkingMessage_t* message = messages[index];
            if (message == nullptr) {
                continue;
            }
            if (message->m_cbSize > 0 &&
                static_cast<std::size_t>(message->m_cbSize) <= kMaxTransportPayloadBytes) {
                std::vector<std::byte> payload(static_cast<std::size_t>(message->m_cbSize));
                std::memcpy(payload.data(), message->m_pData, payload.size());
                TransportEvent event;
                event.type = TransportEventType::Message;
                event.connection = 1;
                event.payload = std::move(payload);
                emit(std::move(event));
            }
            message->Release();
        }
    }

    std::vector<TransportEvent> take_events() override {
        std::lock_guard<std::mutex> lock(events_mutex_);
        std::vector<TransportEvent> result;
        result.swap(events_);
        return result;
    }

    bool send(std::span<const std::byte> payload,
              DeliveryMode mode,
              std::string& error) override {
        if (connection_ == k_HSteamNetConnection_Invalid ||
            !connected_.load(std::memory_order_relaxed)) {
            error = "client is not connected";
            return false;
        }
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            error = "payload too large";
            return false;
        }

        const EResult result = sockets_->SendMessageToConnection(
            connection_,
            payload.data(),
            static_cast<std::uint32_t>(payload.size()),
            delivery_flags(mode),
            nullptr);

        if (result == k_EResultOK) {
            error.clear();
            return true;
        }
        if (mode == DeliveryMode::Unreliable && result == k_EResultIgnored) {
            error.clear();
            return true;
        }

        error = "GNS send failed with EResult ";
        error += std::to_string(static_cast<int>(result));
        return false;
    }

    void disconnect() override {
        if (sockets_ != nullptr && connection_ != k_HSteamNetConnection_Invalid) {
            sockets_->CloseConnection(connection_,
                                      k_ESteamNetConnectionEnd_App_Generic,
                                      "client disconnect",
                                      false);
        }
        connected_.store(false, std::memory_order_relaxed);
    }

    void stop() override {
        if (sockets_ != nullptr && connection_ != k_HSteamNetConnection_Invalid) {
            CallbackRegistry::instance().remove_client(connection_);
            sockets_->CloseConnection(connection_,
                                      k_ESteamNetConnectionEnd_App_Generic,
                                      "client stopping",
                                      false);
            connection_ = k_HSteamNetConnection_Invalid;
        }
        connected_.store(false, std::memory_order_relaxed);
        release_runtime();
    }

    [[nodiscard]] bool connected() const override {
        return connected_.load(std::memory_order_relaxed);
    }

    void on_status(SteamNetConnectionStatusChangedCallback_t* info) {
        if (info == nullptr) {
            return;
        }

        switch (info->m_info.m_eState) {
            case k_ESteamNetworkingConnectionState_Connected: {
                const bool was_connected = connected_.exchange(true, std::memory_order_relaxed);
                if (!was_connected) {
                    TransportEvent event;
                    event.type = TransportEventType::Connected;
                    event.connection = 1;
                    emit(std::move(event));
                }
                break;
            }

            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
                const bool was_connected = connected_.exchange(false, std::memory_order_relaxed);
                if (was_connected) {
                    TransportEvent event;
                    event.type = TransportEventType::Disconnected;
                    event.connection = 1;
                    event.detail = info->m_info.m_szEndDebug;
                    emit(std::move(event));
                }
                break;
            }

            case k_ESteamNetworkingConnectionState_Connecting:
            case k_ESteamNetworkingConnectionState_None:
            default:
                break;
        }
    }

private:
    void emit(TransportEvent event) {
        std::lock_guard<std::mutex> lock(events_mutex_);
        events_.push_back(std::move(event));
    }

    void release_runtime() {
        if (runtime_acquired_) {
            runtime_.release();
            runtime_acquired_ = false;
        }
    }

    GnsRuntime& runtime_{GnsRuntime::instance()};
    ISteamNetworkingSockets* sockets_{nullptr};
    HSteamNetConnection connection_{k_HSteamNetConnection_Invalid};
    bool runtime_acquired_{false};
    std::atomic<bool> connected_{false};
    std::mutex events_mutex_;
    std::vector<TransportEvent> events_;
};

void connection_status_callback(SteamNetConnectionStatusChangedCallback_t* info) {
    if (info == nullptr) {
        return;
    }

    if (info->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid) {
        if (GnsServer* server = CallbackRegistry::instance().find_server(info->m_info.m_hListenSocket)) {
            server->on_status(info);
        }
        return;
    }

    if (GnsClient* client = CallbackRegistry::instance().find_client(info->m_hConn)) {
        client->on_status(info);
    }
}

} // namespace

std::unique_ptr<TransportServer> create_gns_server() {
    return std::make_unique<GnsServer>();
}

std::unique_ptr<TransportClient> create_gns_client() {
    return std::make_unique<GnsClient>();
}

} // namespace rr::net
