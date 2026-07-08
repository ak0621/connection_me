#include "daemon.h"

#include "net.h"
#include "platform.h"
#include "util.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>

namespace mybarrier {
namespace {

std::string discovery_request() {
    return "MYBARRIER_DISCOVER 1";
}

std::string discovery_response(const DeviceConfig& device) {
    std::ostringstream out;
    out << "MYBARRIER_HERE 1 " << device.id << " " << device.port << " " << escape_field(device.name);
    return out.str();
}

std::string transfer_key(const std::string& source_id, const std::string& transfer_id) {
    return source_id + ":" + transfer_id;
}

std::size_t parse_size_header(const Frame& frame, const std::string& key, std::size_t fallback = 0) {
    return header_size(frame, key, fallback);
}

std::filesystem::path collision_safe_path(const std::filesystem::path& dir, const std::string& filename) {
    auto path = dir / filename;
    if (!std::filesystem::exists(path)) {
        return path;
    }
    return dir / (random_hex(4) + "-" + filename);
}

}  // namespace

Daemon::Daemon(ConfigStore store) : store_(std::move(store)) {}

void Daemon::stop() {
    running_ = false;
}

int Daemon::run(const DaemonOptions& options) {
    SocketRuntime sockets;
    DeviceConfig device = store_.load_device(options.name, options.port, options.role);
    mb_socket_t server = tcp_listen(device.port);
    std::thread discovery(&Daemon::discovery_loop, this, device, options.discovery_port);

    std::cout << "mybarrier daemon listening on tcp " << device.port
              << ", udp discovery " << options.discovery_port << "\n";
    std::cout << "device_id=" << device.id << " name=" << device.name << " role=" << device.role << "\n";
    if (!options.pair_code.empty()) {
        std::cout << "pairing enabled with code " << options.pair_code << "\n";
    } else {
        std::cout << "pairing disabled; start with --pair-code <code> to accept new peers\n";
    }

    while (running_) {
        AcceptedSocket accepted = tcp_accept(server);
        if (accepted.socket == invalid_socket_value) {
            continue;
        }
        std::thread(&Daemon::handle_client, this, accepted.socket, accepted.remote_ip, device, options.pair_code).detach();
    }

    close_socket(server);
    if (discovery.joinable()) {
        discovery.join();
    }
    return 0;
}

void Daemon::discovery_loop(DeviceConfig device, uint16_t discovery_port) {
    try {
        mb_socket_t socket = udp_bind(discovery_port, true);
        while (running_) {
            auto packet = udp_recv_from(socket, 500);
            if (!packet) {
                continue;
            }
            if (trim(packet->message) == discovery_request()) {
                udp_send_to(socket, packet->remote_ip, packet->remote_port, discovery_response(device));
            }
        }
        close_socket(socket);
    } catch (const std::exception& ex) {
        std::cerr << "discovery disabled: " << ex.what() << "\n";
    }
}

void Daemon::handle_client(mb_socket_t client, std::string remote_ip, DeviceConfig device, std::string pair_code) {
    auto frame = read_frame(client);
    if (!frame) {
        close_socket(client);
        return;
    }
    Frame response = handle_frame(*frame, remote_ip, device, pair_code);
    write_frame(client, response);
    close_socket(client);
}

Frame Daemon::handle_frame(const Frame& frame, const std::string& remote_ip, const DeviceConfig& device, const std::string& pair_code) {
    if (frame.command == "HELLO" || frame.command == "STATUS") {
        return ok_frame({
            {"Device-Id", device.id},
            {"Device-Name", device.name},
            {"Role", device.role},
            {"Port", std::to_string(device.port)},
            {"Platform", platform_name()},
            {"Version", "0.2.0"},
            {"Protocol-Major", std::to_string(app_protocol_major)},
            {"Protocol-Minor", std::to_string(app_protocol_minor)},
        });
    }

    if (frame.command == "PING") {
        Peer peer;
        if (!verify_peer(header_value(frame, "Source-Id"), header_value(frame, "Secret"), nullptr, &peer)) {
            return error_frame("unauthorized ping");
        }
        return ok_frame({
            {"Pong", "1"},
            {"From", device.id},
            {"Device-Name", device.name},
            {"Role", device.role},
            {"Protocol-Major", std::to_string(app_protocol_major)},
            {"Protocol-Minor", std::to_string(app_protocol_minor)},
        });
    }

    if (frame.command == "PAIR") {
        if (pair_code.empty()) {
            return error_frame("pairing is disabled on target daemon");
        }
        if (header_value(frame, "Pair-Code") != pair_code) {
            return error_frame("invalid pairing code");
        }
        std::string client_id = header_value(frame, "Client-Id");
        std::string client_name = header_value(frame, "Client-Name", client_id);
        uint16_t client_port = parse_port(header_value(frame, "Client-Port"), default_port);
        if (client_id.empty()) {
            return error_frame("missing Client-Id");
        }
        std::string secret = random_hex(32);
        Peer peer;
        peer.id = client_id;
        peer.name = client_name;
        peer.host = remote_ip;
        peer.port = client_port;
        peer.secret = secret;
        peer.allow_clipboard = true;
        peer.allow_files = true;
        peer.allow_input = false;
        store_.save_peer(peer);
        return ok_frame({
            {"Device-Id", device.id},
            {"Device-Name", device.name},
            {"Role", device.role},
            {"Port", std::to_string(device.port)},
            {"Secret", secret},
            {"Protocol-Major", std::to_string(app_protocol_major)},
            {"Protocol-Minor", std::to_string(app_protocol_minor)},
        });
    }

    if (frame.command == "CLIPBOARD_PUSH") {
        Peer peer;
        if (!verify_peer(header_value(frame, "Source-Id"), header_value(frame, "Secret"), "clipboard", &peer)) {
            return error_frame("unauthorized clipboard push");
        }
        std::string text(frame.body.begin(), frame.body.end());
        store_.set_local_clipboard(text);
        return ok_frame({{"Stored-Bytes", std::to_string(frame.body.size())}, {"From", peer.id}});
    }

    if (frame.command == "FILE_BEGIN") {
        Peer peer;
        if (!verify_peer(header_value(frame, "Source-Id"), header_value(frame, "Secret"), "files", &peer)) {
            return error_frame("unauthorized file begin");
        }
        return handle_file_begin(frame, peer);
    }

    if (frame.command == "FILE_CHUNK") {
        Peer peer;
        if (!verify_peer(header_value(frame, "Source-Id"), header_value(frame, "Secret"), "files", &peer)) {
            return error_frame("unauthorized file chunk");
        }
        return handle_file_chunk(frame, peer);
    }

    if (frame.command == "FILE_END") {
        Peer peer;
        if (!verify_peer(header_value(frame, "Source-Id"), header_value(frame, "Secret"), "files", &peer)) {
            return error_frame("unauthorized file end");
        }
        return handle_file_end(frame, peer);
    }

    if (frame.command == "FILE_SEND") {
        Peer peer;
        if (!verify_peer(header_value(frame, "Source-Id"), header_value(frame, "Secret"), "files", &peer)) {
            return error_frame("unauthorized file send");
        }
        std::string expected_hash = header_value(frame, "Hash");
        std::string actual_hash = fnv1a64_hex(frame.body);
        if (!expected_hash.empty() && expected_hash != actual_hash) {
            return error_frame("file hash mismatch");
        }
        std::string filename = sanitize_filename(header_value(frame, "Filename", "received.bin"));
        std::filesystem::create_directories(store_.downloads_dir());
        auto destination = collision_safe_path(store_.downloads_dir(), filename);
        write_binary_file(destination, frame.body);
        return ok_frame({
            {"Saved-As", destination.string()},
            {"Bytes", std::to_string(frame.body.size())},
            {"Hash", actual_hash},
            {"Mode", "legacy-whole-file"},
        });
    }

    if (frame.command == "INPUT_EVENT") {
        Peer peer;
        if (!verify_peer(header_value(frame, "Source-Id"), header_value(frame, "Secret"), "input", &peer)) {
            return error_frame("unauthorized input event");
        }
        std::filesystem::create_directories(store_.home());
        std::ofstream log(store_.input_log_path(), std::ios::app);
        log << now_iso8601() << " from=" << peer.id
            << " type=" << header_value(frame, "Type")
            << " payload=" << std::string(frame.body.begin(), frame.body.end()) << "\n";
        return ok_frame({{"Input-Adapter", "logged-only"}});
    }

    return error_frame("unknown command: " + frame.command);
}

Frame Daemon::handle_file_begin(const Frame& frame, const Peer& peer) {
    std::string transfer_id = header_value(frame, "Transfer-Id");
    if (transfer_id.empty()) {
        return error_frame("missing Transfer-Id");
    }
    std::string filename = sanitize_filename(header_value(frame, "Filename", "received.bin"));
    std::size_t expected_size = parse_size_header(frame, "Size", 0);
    std::string expected_hash = header_value(frame, "Hash");
    std::filesystem::create_directories(store_.downloads_dir());

    TransferState state;
    state.source_id = peer.id;
    state.transfer_id = transfer_id;
    state.expected_size = expected_size;
    state.expected_hash = expected_hash;
    state.final_path = collision_safe_path(store_.downloads_dir(), filename);
    state.temp_path = store_.downloads_dir() / (filename + "." + transfer_id + ".part");
    write_binary_file(state.temp_path, {});

    std::lock_guard<std::mutex> lock(transfers_mutex_);
    transfers_[transfer_key(peer.id, transfer_id)] = state;
    return ok_frame({{"Transfer-Id", transfer_id}, {"Ready", "1"}, {"Temp", state.temp_path.string()}});
}

Frame Daemon::handle_file_chunk(const Frame& frame, const Peer& peer) {
    std::string transfer_id = header_value(frame, "Transfer-Id");
    std::size_t offset = parse_size_header(frame, "Offset", 0);
    std::string key = transfer_key(peer.id, transfer_id);

    std::lock_guard<std::mutex> lock(transfers_mutex_);
    auto it = transfers_.find(key);
    if (it == transfers_.end()) {
        return error_frame("unknown transfer");
    }
    TransferState& state = it->second;
    if (offset != state.received_size) {
        return error_frame("unexpected chunk offset");
    }
    if (state.expected_size > 0 && state.received_size + frame.body.size() > state.expected_size) {
        return error_frame("chunk exceeds expected transfer size");
    }
    std::ofstream out(state.temp_path, std::ios::binary | std::ios::app);
    if (!out) {
        return error_frame("failed to append transfer chunk");
    }
    if (!frame.body.empty()) {
        out.write(frame.body.data(), static_cast<std::streamsize>(frame.body.size()));
    }
    state.received_size += frame.body.size();
    return ok_frame({{"Transfer-Id", transfer_id}, {"Received", std::to_string(state.received_size)}});
}

Frame Daemon::handle_file_end(const Frame& frame, const Peer& peer) {
    std::string transfer_id = header_value(frame, "Transfer-Id");
    std::string key = transfer_key(peer.id, transfer_id);
    TransferState state;
    {
        std::lock_guard<std::mutex> lock(transfers_mutex_);
        auto it = transfers_.find(key);
        if (it == transfers_.end()) {
            return error_frame("unknown transfer");
        }
        state = it->second;
        transfers_.erase(it);
    }

    if (state.expected_size != 0 && state.received_size != state.expected_size) {
        std::filesystem::remove(state.temp_path);
        return error_frame("transfer size mismatch");
    }
    auto data = read_binary_file(state.temp_path);
    std::string actual_hash = fnv1a64_hex(data);
    if (!state.expected_hash.empty() && actual_hash != state.expected_hash) {
        std::filesystem::remove(state.temp_path);
        return error_frame("transfer hash mismatch");
    }
    std::error_code ec;
    std::filesystem::rename(state.temp_path, state.final_path, ec);
    if (ec) {
        write_binary_file(state.final_path, data);
        std::filesystem::remove(state.temp_path);
    }
    return ok_frame({
        {"Transfer-Id", transfer_id},
        {"Saved-As", state.final_path.string()},
        {"Bytes", std::to_string(data.size())},
        {"Hash", actual_hash},
        {"Mode", "chunked"},
    });
}

bool Daemon::verify_peer(const std::string& source_id, const std::string& secret, const char* capability, Peer* out_peer) {
    auto peer = store_.find_peer_by_id(source_id);
    if (!peer || peer->secret != secret) {
        return false;
    }
    std::string cap = capability ? capability : "";
    if (cap == "clipboard" && !peer->allow_clipboard) {
        return false;
    }
    if (cap == "files" && !peer->allow_files) {
        return false;
    }
    if (cap == "input" && !peer->allow_input) {
        return false;
    }
    if (out_peer) {
        *out_peer = *peer;
    }
    return true;
}

}  // namespace mybarrier
