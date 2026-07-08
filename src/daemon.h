#pragma once

#include "config.h"
#include "protocol.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mybarrier {

struct DaemonOptions {
    uint16_t port = default_port;
    uint16_t discovery_port = default_discovery_port;
    std::string name;
    std::string role;
    std::string pair_code;
};

class Daemon {
public:
    explicit Daemon(ConfigStore store);
    int run(const DaemonOptions& options);
    void stop();

private:
    struct TransferState {
        std::string source_id;
        std::string transfer_id;
        std::filesystem::path temp_path;
        std::filesystem::path final_path;
        std::string expected_hash;
        std::size_t expected_size = 0;
        std::size_t received_size = 0;
    };

    void discovery_loop(DeviceConfig device, uint16_t discovery_port);
    void handle_client(mb_socket_t client, std::string remote_ip, DeviceConfig device, std::string pair_code);
    Frame handle_frame(const Frame& frame, const std::string& remote_ip, const DeviceConfig& device, const std::string& pair_code);
    Frame handle_file_begin(const Frame& frame, const Peer& peer);
    Frame handle_file_chunk(const Frame& frame, const Peer& peer);
    Frame handle_file_end(const Frame& frame, const Peer& peer);
    bool verify_peer(const std::string& source_id, const std::string& secret, const char* capability, Peer* out_peer = nullptr);

    ConfigStore store_;
    std::atomic<bool> running_{true};
    std::mutex transfers_mutex_;
    std::unordered_map<std::string, TransferState> transfers_;
};

}  // namespace mybarrier
