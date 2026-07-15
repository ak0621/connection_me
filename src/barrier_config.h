#pragma once

#include "config.h"

#include <filesystem>
#include <string>
#include <vector>

namespace mybarrier {

struct BarrierScreenConfig {
    std::string name;
    std::vector<std::string> aliases;
};

struct BarrierRuntimeConfig {
    std::vector<BarrierScreenConfig> screens;
    std::vector<LayoutLink> links;
    bool clipboard_sharing = true;
    bool relative_mouse_moves = false;
    int heartbeat_ms = 5000;
    int switch_delay_ms = 250;
};

std::filesystem::path default_barrier_config_path(const ConfigStore& store);
BarrierRuntimeConfig collect_barrier_config(ConfigStore& store);
std::string render_barrier_config(const BarrierRuntimeConfig& config);
void write_barrier_config_file(const std::filesystem::path& path, const BarrierRuntimeConfig& config);
BarrierRuntimeConfig parse_barrier_config_text(const std::string& text);
void import_barrier_config(ConfigStore& store, const BarrierRuntimeConfig& config);

}  // namespace mybarrier
