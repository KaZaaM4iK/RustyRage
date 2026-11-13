#include "rr/core/Log.hpp"
#include "rr/net/Protocol.hpp"
#include "rr/net/Runtime.hpp"
#include "rr/net/SimulationAdapter.hpp"
#include "rr/net/Transport.hpp"
#include "rr/sim/SimulationClock.hpp"
#include "rr/world/MapHeader.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <thread>

#ifndef RUSTYRAGE_VERSION
#define RUSTYRAGE_VERSION "0.1.0"
#endif

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;

void handle_stop_signal(int /*signal*/) {
    g_stop_requested = 1;
}

struct ServerOptions {
    bool headless{true};
    bool show_help{false};
    bool show_version{false};
    bool net_smoke{false};
    std::uint16_t net_smoke_port{0};
    std::uint64_t ticks{0}; // 0 = run until SIGINT/SIGTERM
};

bool parse_ticks(std::string_view text, std::uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
}

bool parse_options(int argc, char** argv, ServerOptions& options, std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);

        if (argument == "--headless") {
            options.headless = true;
        } else if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else if (argument == "--version") {
            options.show_version = true;
        } else if (argument == "--ticks") {
            if (index + 1 >= argc) {
                error = "--ticks requires a value";
                return false;
            }
            if (!parse_ticks(argv[++index], options.ticks)) {
                error = "--ticks value is invalid";
                return false;
            }
        } else if (argument == "--net-smoke") {
            options.net_smoke = true;
        } else if (argument == "--net-smoke-port") {
            if (index + 1 >= argc) {
                error = "--net-smoke-port requires a value";
                return false;
            }
            std::uint64_t port = 0;
            if (!parse_ticks(argv[++index], port) || port > 65535) {
                error = "--net-smoke-port value is invalid";
                return false;
            }
            options.net_smoke_port = static_cast<std::uint16_t>(port);
        } else {
            error = "unknown argument: ";
            error += argument;
            return false;
        }
    }
    return true;
}

void print_help() {
    std::cout
        << "RustyRage dedicated server\n"
        << "Usage: rusty_server [--headless] [--ticks N] [--version] [--help]\n"
        << "  --headless   accepted for explicit dedicated-server operation (default)\n"
        << "  --ticks N    run exactly N authoritative fixed steps, then exit\n"
        << "  --net-smoke  run the Stage D loopback server + two-client smoke test\n"
        << "  --net-smoke-port N  port for --net-smoke (0 = ephemeral)\n"
        << "  --version    print version and protocol information\n"
        << "  --help       print this help\n";
}


int run_network_smoke(std::uint16_t requested_port) {
    using namespace rr::net;

    auto server = std::make_unique<ServerRuntime>(create_gns_server());
    std::string error;

    // GNS rejects an explicit UDP port 0 as a listen address. Choose an
    // application-level ephemeral port when the caller asks for 0, retrying a
    // small bounded set of ports to avoid a single occupied port.
    std::uint16_t selected_port = requested_port;
    bool started = false;
    std::mt19937 random_engine(std::random_device{}());
    std::uniform_int_distribution<int> port_distribution(30000, 55000);
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (selected_port == 0) {
            selected_port = static_cast<std::uint16_t>(port_distribution(random_engine));
        }
        if (server->start(Endpoint{"0.0.0.0", selected_port}, error)) {
            started = true;
            break;
        }
        if (requested_port != 0) {
            break;
        }
        selected_port = 0;
    }

    if (!started) {
        std::cerr << "network_smoke: server start failed: " << error << '\n';
        return 1;
    }

    const std::uint16_t port = server->local_port();
    if (port == 0) {
        std::cerr << "network_smoke: server did not report a bound port\n";
        server->stop();
        return 1;
    }
    std::cout << "network_smoke: server listening on 127.0.0.1:" << port << '\n';

    std::array<std::unique_ptr<ClientRuntime>, 2> clients;
    clients[0] = std::make_unique<ClientRuntime>(create_gns_client(), "smoke_one", 0xA1100001u);
    clients[1] = std::make_unique<ClientRuntime>(create_gns_client(), "smoke_two", 0xA1100002u);

    for (std::size_t index = 0; index < clients.size(); ++index) {
        if (!clients[index]->connect(Endpoint{"127.0.0.1", port}, error)) {
            std::cerr << "network_smoke: client " << index << " connect failed: " << error << '\n';
            clients[index]->stop();
            server->stop();
            return 1;
        }
    }

    NetSimAdapter adapter(1.0 / 60.0, 8);
    std::uint32_t received_inputs = 0;
    std::uint32_t sent_inputs = 0;
    bool inputs_sent = false;
    bool ticks_sent = false;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        server->poll();
        for (const auto& client : clients) {
            client->poll();
        }

        for (const ReceivedInput& input : server->consume_inputs()) {
            if (adapter.submit_input(input.client, input.input)) {
                ++received_inputs;
            }
        }

        const auto server_clients = server->clients();
        const bool both_welcomed = clients[0]->welcomed() && clients[1]->welcomed();

        if (both_welcomed && server_clients.size() == 2 && !inputs_sent) {
            for (std::size_t client_index = 0; client_index < clients.size(); ++client_index) {
                for (std::uint32_t tick = 1; tick <= 10; ++tick) {
                    ClientInput input;
                    input.tick = tick;
                    input.throttle = 0.5f;
                    input.steering = client_index == 0 ? 0.1f : -0.1f;
                    if (!clients[client_index]->send_input(input, DeliveryMode::Unreliable, error)) {
                        std::cerr << "network_smoke: unreliable send failed: " << error << '\n';
                    } else {
                        ++sent_inputs;
                    }
                }
            }
            inputs_sent = true;
        }

        if (both_welcomed && received_inputs >= 2 && !ticks_sent) {
            const ServerTick tick = adapter.make_server_tick();
            for (const ServerClientInfo& client : server_clients) {
                if (!server->send_tick(client.id, tick, error)) {
                    std::cerr << "network_smoke: reliable ServerTick send failed: " << error << '\n';
                }
            }
            ticks_sent = true;
        }

        if (ticks_sent && clients[0]->last_server_tick().has_value() &&
            clients[1]->last_server_tick().has_value()) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const bool handshake_ok = clients[0]->welcomed() && clients[1]->welcomed() &&
                              server->clients().size() == 2;
    const bool reliable_ok = clients[0]->last_server_tick().has_value() &&
                             clients[1]->last_server_tick().has_value();
    const bool unreliable_ok = inputs_sent && received_inputs > 0 && received_inputs <= sent_inputs;

    if (!handshake_ok || !reliable_ok || !unreliable_ok) {
        std::cerr << "network_smoke: handshake/reliable/unreliable precondition failed"
                  << " handshake=" << (handshake_ok ? "ok" : "fail")
                  << " reliable=" << (reliable_ok ? "ok" : "fail")
                  << " unreliable_received=" << received_inputs
                  << " unreliable_sent=" << sent_inputs << '\n';
        for (const auto& client : clients) {
            client->disconnect();
            client->stop();
        }
        server->stop();
        return 1;
    }

    // Send the protocol Disconnect reliably, allow GNS to flush it, then close
    // the transport. The server must observe Disconnect messages and remove
    // both sessions.
    for (const auto& client : clients) {
        if (!client->send_disconnect(error)) {
            std::cerr << "network_smoke: reliable Disconnect send failed: " << error << '\n';
        }
    }

    const auto flush_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (std::chrono::steady_clock::now() < flush_deadline) {
        server->poll();
        for (const auto& client : clients) {
            client->poll();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (const auto& client : clients) {
        client->disconnect();
    }

    const auto close_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < close_deadline) {
        server->poll();
        for (const auto& client : clients) {
            client->poll();
        }
        if (server->clients().empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const bool disconnect_ok = server->clients().empty() && server->disconnect_messages() >= 2;

    for (const auto& client : clients) {
        client->stop();
    }
    server->stop();

    std::cout << "network_smoke: clients=2 input_sent=" << sent_inputs
              << " input_received=" << received_inputs
              << " reliable_ticks=" << (reliable_ok ? 2 : 0)
              << " disconnect_messages=" << server->disconnect_messages()
              << " remaining_clients=" << server->clients().size() << '\n';

    if (!disconnect_ok) {
        std::cerr << "network_smoke: clean disconnect check failed\n";
        return 1;
    }

    std::cout << "network_smoke: PASS clients=2 reliable=ok unreliable=ok disconnect=clean"
              << " transport=GameNetworkingSockets\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    ServerOptions options;
    std::string parse_error;
    if (!parse_options(argc, argv, options, parse_error)) {
        std::cerr << "rusty_server: " << parse_error << '\n';
        return 2;
    }

    if (options.show_help) {
        print_help();
        return 0;
    }
    if (options.show_version) {
        std::cout << "RustyRage server " RUSTYRAGE_VERSION
                  << " protocol " << rr::net::kProtocolVersion << '\n';
        return 0;
    }

    if (options.net_smoke) {
        return run_network_smoke(options.net_smoke_port);
    }

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    rr::core::Logger logger(std::cerr);
    logger.set_min_level(rr::core::LogLevel::Info);

    rr::world::MapHeader map_header;
    map_header.map_id = "foundation_smoke";
    map_header.max_players = rr::world::kMaxPlayersPerMatch;
    std::string map_error;
    if (!map_header.validate(map_error)) {
        logger.write(rr::core::LogLevel::Critical, "invalid default map header: " + map_error);
        return 1;
    }

    logger.write(rr::core::LogLevel::Info,
                 "RustyRage dedicated server " RUSTYRAGE_VERSION
                 " (headless, protocol " + std::to_string(rr::net::kProtocolVersion) + ")");

    rr::sim::FixedStepClock clock(1.0 / 60.0, 8);
    const auto tick_budget = options.ticks;
    std::uint64_t last_logged_tick = 0;
    auto last_wall_time = std::chrono::steady_clock::now();

    while (g_stop_requested == 0 && (tick_budget == 0 || clock.tick() < tick_budget)) {
        std::uint32_t steps = 0;
        if (tick_budget != 0) {
            // Deterministic bounded run used by CTest and smoke harnesses.
            steps = clock.advance(clock.step_seconds());
        } else {
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - last_wall_time).count();
            last_wall_time = now;
            steps = clock.advance(elapsed);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (steps == 0) {
            continue;
        }

        // Stage A establishes the authoritative fixed-step seam. Physics,
        // networking and match rules are added by their own gated stages.
        if (clock.tick() - last_logged_tick >= 60 || clock.tick() == tick_budget) {
            logger.write(rr::core::LogLevel::Debug,
                         "authoritative tick " + std::to_string(clock.tick()) +
                         ", map " + map_header.map_id);
            last_logged_tick = clock.tick();
        }
    }

    if (clock.dropped_steps() != 0) {
        logger.write(rr::core::LogLevel::Warning,
                     "fixed-step scheduler dropped " +
                     std::to_string(clock.dropped_steps()) + " overdue steps");
    }

    logger.write(rr::core::LogLevel::Info,
                 "server stopped cleanly at tick " + std::to_string(clock.tick()));
    return 0;
}
