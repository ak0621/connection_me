#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mybarrier {

struct DeviceConfig {
    std::string id;
    std::string key;
    std::string name;
    std::string role = "peer";
    uint16_t port = 37373;
};

struct Peer {
    std::string id;
    std::string name;
    std::string host;
    uint16_t port = 37373;
    std::string secret;
    bool allow_clipboard = true;
    bool allow_files = true;
    bool allow_input = false;
};

struct LayoutLink {
    std::string source;
    std::string side;
    std::string target;
};

class ConfigStore {
public:
    explicit ConfigStore(std::filesystem::path home = {});

    const std::filesystem::path& home() const;
    std::filesystem::path downloads_dir() const;
    std::filesystem::path input_log_path() const;

    DeviceConfig load_device(const std::string& name_override = {}, uint16_t port_override = 0, const std::string& role_override = {});
    void save_device(const DeviceConfig& device);

    std::vector<Peer> load_peers() const;
    std::optional<Peer> find_peer(const std::string& id_or_name) const;
    std::optional<Peer> find_peer_by_id(const std::string& id) const;
    void save_peer(const Peer& peer);

    std::vector<LayoutLink> load_layout() const;
    void save_layout_link(const LayoutLink& link);
    void clear_layout();

    std::string local_clipboard() const;
    void set_local_clipboard(const std::string& text) const;

private:
    std::filesystem::path home_;
};

std::filesystem::path default_config_home();
std::string normalize_role(const std::string& role);
bool is_valid_layout_side(const std::string& side);

}  // namespace mybarrier
