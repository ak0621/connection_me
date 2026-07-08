#include "config.h"

#include "util.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace mybarrier {
namespace {

std::map<std::string, std::string> read_kv_file(const std::filesystem::path& path) {
    std::map<std::string, std::string> values;
    std::istringstream input(read_text_file(path));
    std::string line;
    while (std::getline(input, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        values[line.substr(0, pos)] = line.substr(pos + 1);
    }
    return values;
}

std::string bool_to_string(bool value) {
    return value ? "1" : "0";
}

bool parse_bool(const std::string& value, bool fallback) {
    if (value == "1" || value == "true" || value == "yes") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no") {
        return false;
    }
    return fallback;
}

}  // namespace

std::string normalize_role(const std::string& role) {
    std::string value = trim(role);
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (value == "server" || value == "client" || value == "peer") {
        return value;
    }
    if (value.empty()) {
        return "peer";
    }
    throw std::runtime_error("invalid role: " + role + " (expected server, client, or peer)");
}

bool is_valid_layout_side(const std::string& side) {
    return side == "left" || side == "right" || side == "top" || side == "bottom";
}

std::filesystem::path default_config_home() {
    if (const char* env = std::getenv("MYBARRIER_HOME")) {
        if (*env) {
            return std::filesystem::path(env);
        }
    }
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::filesystem::path(appdata) / "MyBarrier";
    }
    if (const char* profile = std::getenv("USERPROFILE")) {
        return std::filesystem::path(profile) / "AppData" / "Roaming" / "MyBarrier";
    }
    return std::filesystem::current_path() / ".mybarrier";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "MyBarrier";
    }
    return std::filesystem::current_path() / ".mybarrier";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        return std::filesystem::path(xdg) / "mybarrier";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".config" / "mybarrier";
    }
    return std::filesystem::current_path() / ".mybarrier";
#endif
}

ConfigStore::ConfigStore(std::filesystem::path home)
    : home_(home.empty() ? default_config_home() : std::move(home)) {
    std::filesystem::create_directories(home_);
}

const std::filesystem::path& ConfigStore::home() const {
    return home_;
}

std::filesystem::path ConfigStore::downloads_dir() const {
    return home_ / "received";
}

std::filesystem::path ConfigStore::input_log_path() const {
    return home_ / "input-events.log";
}

DeviceConfig ConfigStore::load_device(const std::string& name_override, uint16_t port_override, const std::string& role_override) {
    auto path = home_ / "device.conf";
    auto values = read_kv_file(path);
    DeviceConfig device;
    device.id = values["id"];
    device.key = values["key"];
    device.name = values["name"];
    device.role = normalize_role(values["role"]);
    device.port = parse_port(values["port"], 37373);

    bool changed = false;
    if (device.id.empty()) {
        device.id = random_hex(16);
        changed = true;
    }
    if (device.key.empty()) {
        device.key = random_hex(32);
        changed = true;
    }
    if (device.name.empty()) {
        device.name = hostname();
        changed = true;
    }
    if (!name_override.empty() && name_override != device.name) {
        device.name = name_override;
        changed = true;
    }
    if (!role_override.empty()) {
        std::string role = normalize_role(role_override);
        if (role != device.role) {
            device.role = role;
            changed = true;
        }
    }
    if (port_override != 0 && port_override != device.port) {
        device.port = port_override;
        changed = true;
    }
    if (changed || !std::filesystem::exists(path)) {
        save_device(device);
    }
    return device;
}

void ConfigStore::save_device(const DeviceConfig& device) {
    std::ostringstream out;
    out << "id=" << device.id << "\n";
    out << "key=" << device.key << "\n";
    out << "name=" << device.name << "\n";
    out << "role=" << normalize_role(device.role) << "\n";
    out << "port=" << device.port << "\n";
    write_text_file(home_ / "device.conf", out.str());
}

std::vector<Peer> ConfigStore::load_peers() const {
    std::vector<Peer> peers;
    std::istringstream input(read_text_file(home_ / "peers.tsv"));
    std::string line;
    while (std::getline(input, line)) {
        if (trim(line).empty()) {
            continue;
        }
        auto parts = split(line, '\t');
        if (parts.size() < 8) {
            continue;
        }
        Peer peer;
        peer.id = unescape_field(parts[0]);
        peer.name = unescape_field(parts[1]);
        peer.host = unescape_field(parts[2]);
        peer.port = parse_port(parts[3], 37373);
        peer.secret = unescape_field(parts[4]);
        peer.allow_clipboard = parse_bool(parts[5], true);
        peer.allow_files = parse_bool(parts[6], true);
        peer.allow_input = parse_bool(parts[7], false);
        if (!peer.id.empty() && !peer.secret.empty()) {
            peers.push_back(peer);
        }
    }
    return peers;
}

std::optional<Peer> ConfigStore::find_peer(const std::string& id_or_name) const {
    for (const auto& peer : load_peers()) {
        if (peer.id == id_or_name || peer.name == id_or_name) {
            return peer;
        }
    }
    return std::nullopt;
}

std::optional<Peer> ConfigStore::find_peer_by_id(const std::string& id) const {
    for (const auto& peer : load_peers()) {
        if (peer.id == id) {
            return peer;
        }
    }
    return std::nullopt;
}

void ConfigStore::save_peer(const Peer& peer) {
    auto peers = load_peers();
    bool replaced = false;
    for (auto& existing : peers) {
        if (existing.id == peer.id) {
            existing = peer;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        peers.push_back(peer);
    }

    std::ostringstream out;
    for (const auto& item : peers) {
        out << escape_field(item.id) << '\t'
            << escape_field(item.name) << '\t'
            << escape_field(item.host) << '\t'
            << item.port << '\t'
            << escape_field(item.secret) << '\t'
            << bool_to_string(item.allow_clipboard) << '\t'
            << bool_to_string(item.allow_files) << '\t'
            << bool_to_string(item.allow_input) << '\n';
    }
    write_text_file(home_ / "peers.tsv", out.str());
}

std::vector<LayoutLink> ConfigStore::load_layout() const {
    std::vector<LayoutLink> links;
    std::istringstream input(read_text_file(home_ / "layout.tsv"));
    std::string line;
    while (std::getline(input, line)) {
        if (trim(line).empty()) {
            continue;
        }
        auto parts = split(line, '\t');
        if (parts.size() < 3) {
            continue;
        }
        LayoutLink link{unescape_field(parts[0]), unescape_field(parts[1]), unescape_field(parts[2])};
        if (!link.source.empty() && is_valid_layout_side(link.side) && !link.target.empty()) {
            links.push_back(link);
        }
    }
    return links;
}

void ConfigStore::save_layout_link(const LayoutLink& link) {
    if (link.source.empty() || !is_valid_layout_side(link.side) || link.target.empty()) {
        throw std::runtime_error("invalid layout link");
    }
    auto links = load_layout();
    bool replaced = false;
    for (auto& existing : links) {
        if (existing.source == link.source && existing.side == link.side) {
            existing = link;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        links.push_back(link);
    }
    std::ostringstream out;
    for (const auto& item : links) {
        out << escape_field(item.source) << '\t' << escape_field(item.side) << '\t' << escape_field(item.target) << '\n';
    }
    write_text_file(home_ / "layout.tsv", out.str());
}

void ConfigStore::clear_layout() {
    write_text_file(home_ / "layout.tsv", "");
}

std::string ConfigStore::local_clipboard() const {
    return read_text_file(home_ / "clipboard.txt");
}

void ConfigStore::set_local_clipboard(const std::string& text) const {
    write_text_file(home_ / "clipboard.txt", text);
}

}  // namespace mybarrier
