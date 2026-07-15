#include "cli.h"

#include "barrier_config.h"
#include "config.h"
#include "daemon.h"
#include "net.h"
#include "platform.h"
#include "protocol.h"
#include "util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>
#include <utility>

namespace mybarrier {
namespace {

constexpr std::size_t default_file_chunk_size = 256 * 1024;

void print_usage() {
    std::cout << R"(mybarrier 0.2.0

Usage:
  mybarrier artifact-info
  mybarrier status [--name NAME] [--port PORT] [--role server|client|peer]
  mybarrier role-set server|client|peer
  mybarrier server [--name NAME] [--port PORT] [--pair-code CODE] [--write-config] [--config FILE]
  mybarrier client --host HOST [--server-port PORT] [--listen-port PORT] [--name NAME] [--code CODE]
  mybarrier serve [--name NAME] [--port PORT] [--role server|client|peer] [--pair-code CODE]
  mybarrier discover [--timeout MS] [--discovery-port PORT]
  mybarrier pair --host HOST [--port PORT] --code CODE
  mybarrier config-export [--path FILE]
  mybarrier config-import --path FILE
  mybarrier ping --peer PEER_ID_OR_NAME
  mybarrier peers
  mybarrier peer-permit --peer PEER_ID_OR_NAME [--clipboard 0|1] [--files 0|1] [--input 0|1]
  mybarrier layout-add [--from SCREEN] --side left|right|top|bottom --to SCREEN [--bidirectional]
  mybarrier layout-list
  mybarrier layout-clear
  mybarrier clipboard-set TEXT
  mybarrier clipboard-get
  mybarrier clipboard-send --peer PEER_ID_OR_NAME [--text TEXT]
  mybarrier send-file --peer PEER_ID_OR_NAME --path FILE [--chunk-size BYTES]
  mybarrier input-send --peer PEER_ID_OR_NAME --type TYPE [--payload TEXT]
  mybarrier diag

Environment:
  MYBARRIER_HOME overrides the config directory. This is useful for local two-daemon tests.
)";
}

std::string option_value(const std::vector<std::string>& args, const std::string& name, const std::string& fallback = {}) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) {
            return args[i + 1];
        }
    }
    return fallback;
}

bool has_option(const std::vector<std::string>& args, const std::string& name) {
    for (const auto& arg : args) {
        if (arg == name) {
            return true;
        }
    }
    return false;
}

bool parse_cli_bool(const std::string& value, bool fallback) {
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return fallback;
}

std::size_t parse_size_option(const std::string& value, std::size_t fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        std::size_t parsed = static_cast<std::size_t>(std::stoull(value));
        if (parsed > 0 && parsed <= max_frame_body_bytes) {
            return parsed;
        }
    } catch (...) {
    }
    return fallback;
}

std::string reverse_layout_side(const std::string& side) {
    if (side == "left") return "right";
    if (side == "right") return "left";
    if (side == "top") return "bottom";
    if (side == "bottom") return "top";
    return {};
}

void save_layout_link(ConfigStore& store, const LayoutLink& link, bool bidirectional) {
    store.save_layout_link(link);
    if (!bidirectional) {
        return;
    }
    LayoutLink reverse;
    reverse.source = link.target;
    reverse.side = reverse_layout_side(link.side);
    reverse.target = link.source;
    if (!reverse.side.empty()) {
        store.save_layout_link(reverse);
    }
}

Frame request_host(const std::string& host, uint16_t port, Frame frame) {
    SocketRuntime sockets;
    mb_socket_t socket = tcp_connect(host, port);
    if (socket == invalid_socket_value) {
        throw std::runtime_error("failed to connect to " + host + ":" + std::to_string(port));
    }
    if (!write_frame(socket, frame)) {
        close_socket(socket);
        throw std::runtime_error("failed to send request");
    }
    auto response = read_frame(socket);
    close_socket(socket);
    if (!response) {
        throw std::runtime_error("empty response from peer");
    }
    return *response;
}

Frame request_peer(const Peer& peer, Frame frame) {
    return request_host(peer.host, peer.port, std::move(frame));
}

void print_response_or_throw(const Frame& response) {
    if (response.command == "ERR") {
        throw std::runtime_error(header_value(response, "Message", "peer returned error"));
    }
    for (const auto& [key, value] : response.headers) {
        if (key != "Content-Length") {
            std::cout << key << "=" << value << "\n";
        }
    }
}

std::vector<char> bytes_from_text(const std::string& text) {
    return std::vector<char>(text.begin(), text.end());
}

Peer pair_with_host(ConfigStore& store, const std::string& host, uint16_t port, const std::string& code) {
    if (host.empty() || code.empty()) {
        throw std::runtime_error("pair requires host and code");
    }
    auto local = store.load_device({}, 0);
    Frame request;
    request.command = "PAIR";
    request.headers = {
        {"Client-Id", local.id},
        {"Client-Name", local.name},
        {"Client-Port", std::to_string(local.port)},
        {"Client-Role", local.role},
        {"Pair-Code", code},
    };
    auto response = request_host(host, port, request);
    if (response.command == "ERR") {
        throw std::runtime_error(header_value(response, "Message", "pairing failed"));
    }
    Peer peer;
    peer.id = header_value(response, "Device-Id");
    peer.name = header_value(response, "Device-Name", peer.id);
    peer.host = host;
    peer.port = parse_port(header_value(response, "Port"), port);
    peer.secret = header_value(response, "Secret");
    peer.allow_clipboard = true;
    peer.allow_files = true;
    peer.allow_input = true;
    if (peer.id.empty() || peer.secret.empty()) {
        throw std::runtime_error("invalid pairing response");
    }
    store.save_peer(peer);
    return peer;
}

void print_artifact_info() {
    std::cout << "current_platform=" << platform_name() << "\n";
#ifdef _WIN32
    std::cout << "current_output=build/Release/mybarrier.exe or build/mybarrier.exe\n";
#else
    std::cout << "current_output=build/mybarrier\n";
#endif
    std::cout << "linux_output=build/mybarrier\n";
    std::cout << "macos_output=build/mybarrier\n";
    std::cout << "windows_output=build/Release/mybarrier.exe or build/mybarrier.exe\n";
    std::cout << "ci_workflow=.github/workflows/build.yml\n";
    std::cout << "docs=docs/build-artifacts.md\n";
}

void send_chunked_file(ConfigStore& store, const Peer& peer, const std::filesystem::path& path, std::size_t chunk_size) {
    auto data = read_binary_file(path);
    auto local = store.load_device({}, 0);
    std::string transfer_id = random_hex(8);
    std::string hash = fnv1a64_hex(data);

    Frame begin;
    begin.command = "FILE_BEGIN";
    begin.headers = {
        {"Source-Id", local.id},
        {"Secret", peer.secret},
        {"Transfer-Id", transfer_id},
        {"Filename", path.filename().string()},
        {"Size", std::to_string(data.size())},
        {"Hash", hash},
    };
    print_response_or_throw(request_peer(peer, begin));

    std::size_t offset = 0;
    while (offset < data.size()) {
        std::size_t n = std::min(chunk_size, data.size() - offset);
        Frame chunk;
        chunk.command = "FILE_CHUNK";
        chunk.headers = {
            {"Source-Id", local.id},
            {"Secret", peer.secret},
            {"Transfer-Id", transfer_id},
            {"Offset", std::to_string(offset)},
        };
        chunk.body.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                          data.begin() + static_cast<std::ptrdiff_t>(offset + n));
        request_peer(peer, chunk);
        offset += n;
    }

    Frame end;
    end.command = "FILE_END";
    end.headers = {
        {"Source-Id", local.id},
        {"Secret", peer.secret},
        {"Transfer-Id", transfer_id},
    };
    print_response_or_throw(request_peer(peer, end));
}

}  // namespace

int run_cli(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
        print_usage();
        return 0;
    }

    ConfigStore store;
    const std::string command = args[0];

    if (command == "artifact-info") {
        print_artifact_info();
        return 0;
    }

    if (command == "status") {
        uint16_t port = parse_port(option_value(args, "--port"), 0);
        auto device = store.load_device(option_value(args, "--name"), port, option_value(args, "--role"));
        std::cout << "config_home=" << store.home().string() << "\n";
        std::cout << "device_id=" << device.id << "\n";
        std::cout << "device_name=" << device.name << "\n";
        std::cout << "role=" << device.role << "\n";
        std::cout << "port=" << device.port << "\n";
        std::cout << "platform=" << platform_name() << "\n";
        std::cout << "protocol=" << app_protocol_major << "." << app_protocol_minor << "\n";
        std::cout << "peers=" << store.load_peers().size() << "\n";
        std::cout << "layout_links=" << store.load_layout().size() << "\n";
        return 0;
    }

    if (command == "role-set") {
        std::string role = args.size() >= 2 ? args[1] : option_value(args, "--role");
        auto device = store.load_device({}, 0, role);
        std::cout << "role=" << device.role << "\n";
        return 0;
    }

    if (command == "server") {
        DaemonOptions options;
        options.port = parse_port(option_value(args, "--port"), default_port);
        options.discovery_port = parse_port(option_value(args, "--discovery-port"), default_discovery_port);
        options.name = option_value(args, "--name");
        options.role = "server";
        options.pair_code = option_value(args, "--pair-code");
        store.load_device(options.name, options.port, options.role);
        if (has_option(args, "--write-config") || has_option(args, "--config")) {
            std::filesystem::path path = option_value(args, "--config");
            if (path.empty()) {
                path = default_barrier_config_path(store);
            }
            write_barrier_config_file(path, collect_barrier_config(store));
            std::cout << "config=" << path.string() << "\n";
        }
        Daemon daemon(store);
        return daemon.run(options);
    }

    if (command == "client") {
        std::string host = option_value(args, "--host");
        std::string code = option_value(args, "--code");
        uint16_t server_port = parse_port(option_value(args, "--server-port", option_value(args, "--port")), default_port);
        uint16_t listen_port = parse_port(option_value(args, "--listen-port"), default_port);
        store.load_device(option_value(args, "--name"), listen_port, "client");
        if (!host.empty() && !code.empty()) {
            auto peer = pair_with_host(store, host, server_port, code);
            std::cout << "paired server id=" << peer.id << " name=" << peer.name << " host=" << peer.host << " port=" << peer.port << "\n";
        } else if (host.empty()) {
            throw std::runtime_error("client requires --host");
        }
        DaemonOptions options;
        options.port = listen_port;
        options.discovery_port = parse_port(option_value(args, "--discovery-port"), default_discovery_port);
        options.name = option_value(args, "--name");
        options.role = "client";
        Daemon daemon(store);
        return daemon.run(options);
    }

    if (command == "config-export") {
        BarrierRuntimeConfig config = collect_barrier_config(store);
        std::filesystem::path path = option_value(args, "--path");
        if (path.empty()) {
            std::cout << render_barrier_config(config);
        } else {
            write_barrier_config_file(path, config);
            std::cout << "config=" << path.string() << "\n";
        }
        return 0;
    }

    if (command == "config-import") {
        std::filesystem::path path = option_value(args, "--path");
        if (path.empty()) {
            throw std::runtime_error("config-import requires --path");
        }
        auto config = parse_barrier_config_text(read_text_file(path));
        import_barrier_config(store, config);
        std::cout << "imported screens=" << config.screens.size() << " links=" << config.links.size() << "\n";
        return 0;
    }

    if (command == "serve") {
        DaemonOptions options;
        options.port = parse_port(option_value(args, "--port"), default_port);
        options.discovery_port = parse_port(option_value(args, "--discovery-port"), default_discovery_port);
        options.name = option_value(args, "--name");
        options.role = option_value(args, "--role");
        options.pair_code = option_value(args, "--pair-code");
        Daemon daemon(store);
        return daemon.run(options);
    }

    if (command == "discover") {
        SocketRuntime sockets;
        uint16_t discovery_port = parse_port(option_value(args, "--discovery-port"), default_discovery_port);
        int timeout_ms = static_cast<int>(parse_port(option_value(args, "--timeout"), 1500));
        mb_socket_t socket = udp_bind(0, true);
        udp_send_to(socket, "255.255.255.255", discovery_port, "MYBARRIER_DISCOVER 1");
        udp_send_to(socket, "127.0.0.1", discovery_port, "MYBARRIER_DISCOVER 1");
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        bool found = false;
        while (std::chrono::steady_clock::now() < deadline) {
            auto packet = udp_recv_from(socket, 200);
            if (!packet) {
                continue;
            }
            auto parts = split(trim(packet->message), ' ');
            if (parts.size() >= 5 && parts[0] == "MYBARRIER_HERE") {
                std::string name = parts[4];
                for (std::size_t i = 5; i < parts.size(); ++i) {
                    name += " " + parts[i];
                }
                std::cout << "host=" << packet->remote_ip
                          << " id=" << parts[2]
                          << " port=" << parts[3]
                          << " name=" << unescape_field(name) << "\n";
                found = true;
            }
        }
        close_socket(socket);
        return found ? 0 : 2;
    }

    if (command == "pair") {
        std::string host = option_value(args, "--host");
        std::string code = option_value(args, "--code");
        uint16_t port = parse_port(option_value(args, "--port"), default_port);
        if (host.empty() || code.empty()) {
            throw std::runtime_error("pair requires --host and --code");
        }
        Peer peer = pair_with_host(store, host, port, code);
        std::cout << "paired id=" << peer.id << " name=" << peer.name << " host=" << peer.host << " port=" << peer.port << "\n";
        return 0;
    }

    if (command == "ping") {
        std::string peer_key = option_value(args, "--peer");
        if (peer_key.empty()) {
            throw std::runtime_error("ping requires --peer");
        }
        auto peer = store.find_peer(peer_key);
        if (!peer) {
            throw std::runtime_error("unknown peer: " + peer_key);
        }
        auto local = store.load_device({}, 0);
        Frame request;
        request.command = "PING";
        request.headers = {{"Source-Id", local.id}, {"Secret", peer->secret}};
        print_response_or_throw(request_peer(*peer, request));
        return 0;
    }

    if (command == "peers") {
        auto peers = store.load_peers();
        if (peers.empty()) {
            std::cout << "no peers\n";
            return 0;
        }
        for (const auto& peer : peers) {
            std::cout << "id=" << peer.id
                      << " name=" << peer.name
                      << " host=" << peer.host
                      << " port=" << peer.port
                      << " clipboard=" << (peer.allow_clipboard ? "1" : "0")
                      << " files=" << (peer.allow_files ? "1" : "0")
                      << " input=" << (peer.allow_input ? "1" : "0") << "\n";
        }
        return 0;
    }

    if (command == "peer-permit") {
        std::string peer_key = option_value(args, "--peer");
        if (peer_key.empty()) {
            throw std::runtime_error("peer-permit requires --peer");
        }
        auto peer = store.find_peer(peer_key);
        if (!peer) {
            throw std::runtime_error("unknown peer: " + peer_key);
        }
        bool changed = false;
        if (has_option(args, "--clipboard")) {
            peer->allow_clipboard = parse_cli_bool(option_value(args, "--clipboard"), peer->allow_clipboard);
            changed = true;
        }
        if (has_option(args, "--files")) {
            peer->allow_files = parse_cli_bool(option_value(args, "--files"), peer->allow_files);
            changed = true;
        }
        if (has_option(args, "--input")) {
            peer->allow_input = parse_cli_bool(option_value(args, "--input"), peer->allow_input);
            changed = true;
        }
        if (!changed) {
            throw std::runtime_error("peer-permit requires at least one permission flag");
        }
        store.save_peer(*peer);
        std::cout << "updated id=" << peer->id
                  << " clipboard=" << (peer->allow_clipboard ? "1" : "0")
                  << " files=" << (peer->allow_files ? "1" : "0")
                  << " input=" << (peer->allow_input ? "1" : "0") << "\n";
        return 0;
    }

    if (command == "layout-add") {
        auto device = store.load_device({}, 0);
        LayoutLink link;
        link.source = option_value(args, "--from", device.name);
        link.side = option_value(args, "--side");
        link.target = option_value(args, "--to");
        if (link.source.empty() || !is_valid_layout_side(link.side) || link.target.empty()) {
            throw std::runtime_error("layout-add requires --side left|right|top|bottom and --to SCREEN");
        }
        bool bidirectional = has_option(args, "--bidirectional");
        save_layout_link(store, link, bidirectional);
        std::cout << "layout " << link.source << "." << link.side << " -> " << link.target;
        if (bidirectional) {
            std::cout << " bidirectional=1";
        }
        std::cout << "\n";
        return 0;
    }

    if (command == "layout-list") {
        auto links = store.load_layout();
        if (links.empty()) {
            std::cout << "no layout links\n";
            return 0;
        }
        for (const auto& link : links) {
            std::cout << "from=" << link.source << " side=" << link.side << " to=" << link.target << "\n";
        }
        return 0;
    }

    if (command == "layout-clear") {
        store.clear_layout();
        std::cout << "layout cleared\n";
        return 0;
    }

    if (command == "clipboard-set") {
        if (args.size() < 2) {
            throw std::runtime_error("clipboard-set requires text");
        }
        store.set_local_clipboard(args[1]);
        std::cout << "stored " << args[1].size() << " bytes in file-backed clipboard\n";
        return 0;
    }

    if (command == "clipboard-get") {
        std::cout << store.local_clipboard();
        return 0;
    }

    if (command == "clipboard-send") {
        std::string peer_key = option_value(args, "--peer");
        if (peer_key.empty()) {
            throw std::runtime_error("clipboard-send requires --peer");
        }
        auto peer = store.find_peer(peer_key);
        if (!peer) {
            throw std::runtime_error("unknown peer: " + peer_key);
        }
        auto local = store.load_device({}, 0);
        std::string text = has_option(args, "--text") ? option_value(args, "--text") : store.local_clipboard();
        Frame request;
        request.command = "CLIPBOARD_PUSH";
        request.headers = {{"Source-Id", local.id}, {"Secret", peer->secret}};
        request.body = bytes_from_text(text);
        auto response = request_peer(*peer, request);
        print_response_or_throw(response);
        return 0;
    }

    if (command == "send-file") {
        std::string peer_key = option_value(args, "--peer");
        std::string path_text = option_value(args, "--path");
        if (peer_key.empty() || path_text.empty()) {
            throw std::runtime_error("send-file requires --peer and --path");
        }
        auto peer = store.find_peer(peer_key);
        if (!peer) {
            throw std::runtime_error("unknown peer: " + peer_key);
        }
        std::size_t chunk_size = parse_size_option(option_value(args, "--chunk-size"), default_file_chunk_size);
        send_chunked_file(store, *peer, std::filesystem::path(path_text), chunk_size);
        return 0;
    }

    if (command == "input-send") {
        std::string peer_key = option_value(args, "--peer");
        std::string type = option_value(args, "--type");
        std::string payload = option_value(args, "--payload");
        if (peer_key.empty() || type.empty()) {
            throw std::runtime_error("input-send requires --peer and --type");
        }
        auto peer = store.find_peer(peer_key);
        if (!peer) {
            throw std::runtime_error("unknown peer: " + peer_key);
        }
        auto local = store.load_device({}, 0);
        Frame request;
        request.command = "INPUT_EVENT";
        request.headers = {{"Source-Id", local.id}, {"Secret", peer->secret}, {"Type", type}};
        request.body = bytes_from_text(payload);
        auto response = request_peer(*peer, request);
        print_response_or_throw(response);
        return 0;
    }

    if (command == "diag") {
        std::cout << "platform=" << platform_name() << "\n";
        std::cout << "config_home=" << store.home().string() << "\n";
        for (const auto& cap : detect_platform_capabilities()) {
            std::cout << cap.name << "=" << cap.status << " detail=" << cap.detail << "\n";
        }
        return 0;
    }

    throw std::runtime_error("unknown command: " + command);
}

}  // namespace mybarrier
