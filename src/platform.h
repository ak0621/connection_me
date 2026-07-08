#pragma once

#include <string>
#include <vector>

namespace mybarrier {

struct PlatformCapability {
    std::string name;
    std::string status;
    std::string detail;
};

std::vector<PlatformCapability> detect_platform_capabilities();
std::string platform_name();

}  // namespace mybarrier
