#include "barrier_config.h"

#include "util.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace mybarrier {
namespace {

std::string lower_copy(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool same_name(const std::string& a, const std::string& b) {
    return lower_copy(trim(a)) == lower_copy(trim(b));
}

std::string normalized_side(std::string side) {
    side = lower_copy(trim(side));
    if (side == "up") {
        return "top";
    }
    if (side == "down") {
        return "bottom";
    }
    return side;
}

std::string barrier_side(const std::string& side) {
    if (side == "top") {
        return "up";
    }
    if (side == "bottom") {
        return "down";
    }
    return side;
}

std::string trim_comment(std::string line) {
    auto hash = line.find('#');
    if (hash != std::string::npos) {
        line.erase(hash);
    }
    return trim(line);
}

bool parse_bool_option(const std::string& value, bool fallback) {
    std::string normalized = lower_copy(trim(value));
    if (normalized == "true" || normalized == "yes" || normalized == "1" || normalized == "on") {
        return true;
    }
    if (normalized == "false" || normalized == "no" || normalized == "0" || normalized == "off") {
        return false;
    }
    return fallback;
}

int parse_int_option(const std::string& value, int fallback) {
    try {
        int parsed = std::stoi(trim(value));
        if (parsed >= 0) {
            return parsed;
        }
    } catch (...) {
    }
    return fallback;
}

bool contains_screen(const std::vector<BarrierScreenConfig>& screens, const std::string& name) {
    for (const auto& screen : screens) {
        if (same_name(screen.name, name)) {
            return true;
        }
        for (const auto& alias : screen.aliases) {
            if (same_name(alias, name)) {
                return true;
            }
        }
    }
    return false;
}

void add_screen(std::vector<BarrierScreenConfig>& screens, const std::string& name, std::vector<std::string> aliases = {}) {
    std::string clean_name = trim(name);
    if (clean_name.empty() || contains_screen(screens, clean_name)) {
        return;
    }
    BarrierScreenConfig screen;
    screen.name = clean_name;
    for (const auto& alias : aliases) {
        std::string clean_alias = trim(alias);
        if (!clean_alias.empty() && !same_name(clean_alias, clean_name)) {
            screen.aliases.push_back(clean_alias);
        }
    }
    screens.push_back(screen);
}

void add_unique_link(std::vector<LayoutLink>& links, LayoutLink link) {
    link.source = trim(link.source);
    link.side = normalized_side(link.side);
    link.target = trim(link.target);
    if (link.source.empty() || link.target.empty() || !is_valid_layout_side(link.side)) {
        return;
    }
    for (auto& existing : links) {
        if (same_name(existing.source, link.source) && existing.side == link.side) {
            existing = link;
            return;
        }
    }
    links.push_back(link);
}

std::vector<std::string> parse_aliases(const std::string& value) {
    std::vector<std::string> aliases;
    for (auto part : split(value, ',')) {
        part = trim(part);
        if (!part.empty()) {
            aliases.push_back(part);
        }
    }
    return aliases;
}

}  // namespace

std::filesystem::path default_barrier_config_path(const ConfigStore& store) {
    return store.home() / "barrier.conf";
}

BarrierRuntimeConfig collect_barrier_config(ConfigStore& store) {
    BarrierRuntimeConfig config;
    DeviceConfig device = store.load_device();
    add_screen(config.screens, device.name, {device.id});

    for (const auto& peer : store.load_peers()) {
        add_screen(config.screens, peer.name.empty() ? peer.id : peer.name, {peer.id});
    }

    for (const auto& link : store.load_layout()) {
        add_screen(config.screens, link.source);
        add_screen(config.screens, link.target);
        add_unique_link(config.links, link);
    }
    return config;
}

std::string render_barrier_config(const BarrierRuntimeConfig& config) {
    std::ostringstream out;
    out << "section: screens\n";
    for (const auto& screen : config.screens) {
        out << "    " << screen.name << ":\n";
        if (!screen.aliases.empty()) {
            out << "        aliases = ";
            for (std::size_t i = 0; i < screen.aliases.size(); ++i) {
                if (i != 0) {
                    out << ",";
                }
                out << screen.aliases[i];
            }
            out << "\n";
        }
    }
    out << "end\n\n";

    out << "section: links\n";
    std::set<std::string> emitted_sources;
    for (const auto& link : config.links) {
        if (emitted_sources.insert(link.source).second) {
            out << "    " << link.source << ":\n";
            for (const auto& item : config.links) {
                if (same_name(item.source, link.source)) {
                    out << "        " << barrier_side(item.side) << " = " << item.target << "\n";
                }
            }
        }
    }
    out << "end\n\n";

    out << "section: options\n";
    out << "    clipboardSharing = " << (config.clipboard_sharing ? "true" : "false") << "\n";
    out << "    heartbeat = " << config.heartbeat_ms << "\n";
    out << "    switchDelay = " << config.switch_delay_ms << "\n";
    out << "    relativeMouseMoves = " << (config.relative_mouse_moves ? "true" : "false") << "\n";
    out << "end\n";
    return out.str();
}

void write_barrier_config_file(const std::filesystem::path& path, const BarrierRuntimeConfig& config) {
    write_text_file(path, render_barrier_config(config));
}

BarrierRuntimeConfig parse_barrier_config_text(const std::string& text) {
    BarrierRuntimeConfig config;
    std::istringstream input(text);
    std::string line;
    std::string section;
    std::string current_screen;
    std::string current_link_source;

    while (std::getline(input, line)) {
        line = trim_comment(line);
        if (line.empty()) {
            continue;
        }
        if (starts_with(line, "section:")) {
            section = lower_copy(trim(line.substr(std::string("section:").size())));
            current_screen.clear();
            current_link_source.clear();
            continue;
        }
        if (same_name(line, "end")) {
            section.clear();
            current_screen.clear();
            current_link_source.clear();
            continue;
        }

        if (section == "screens") {
            if (line.back() == ':') {
                current_screen = trim(line.substr(0, line.size() - 1));
                add_screen(config.screens, current_screen);
                continue;
            }
            auto eq = line.find('=');
            if (eq != std::string::npos && same_name(trim(line.substr(0, eq)), "aliases") && !current_screen.empty()) {
                auto aliases = parse_aliases(line.substr(eq + 1));
                for (auto& screen : config.screens) {
                    if (same_name(screen.name, current_screen)) {
                        screen.aliases = aliases;
                        break;
                    }
                }
            }
            continue;
        }

        if (section == "links") {
            if (line.back() == ':') {
                current_link_source = trim(line.substr(0, line.size() - 1));
                add_screen(config.screens, current_link_source);
                continue;
            }
            auto eq = line.find('=');
            if (eq != std::string::npos && !current_link_source.empty()) {
                LayoutLink link;
                link.source = current_link_source;
                link.side = normalized_side(line.substr(0, eq));
                link.target = trim(line.substr(eq + 1));
                add_screen(config.screens, link.target);
                add_unique_link(config.links, link);
            }
            continue;
        }

        if (section == "options") {
            auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            std::string key = lower_copy(trim(line.substr(0, eq)));
            std::string value = trim(line.substr(eq + 1));
            if (key == "clipboardsharing") {
                config.clipboard_sharing = parse_bool_option(value, config.clipboard_sharing);
            } else if (key == "relativemousemoves") {
                config.relative_mouse_moves = parse_bool_option(value, config.relative_mouse_moves);
            } else if (key == "heartbeat") {
                config.heartbeat_ms = parse_int_option(value, config.heartbeat_ms);
            } else if (key == "switchdelay") {
                config.switch_delay_ms = parse_int_option(value, config.switch_delay_ms);
            }
        }
    }
    return config;
}

void import_barrier_config(ConfigStore& store, const BarrierRuntimeConfig& config) {
    store.clear_layout();
    for (const auto& link : config.links) {
        store.save_layout_link(link);
    }
}

}  // namespace mybarrier
