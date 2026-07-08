#include "platform.h"

#include <cstdlib>
#include <filesystem>

namespace mybarrier {

std::string platform_name() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::vector<PlatformCapability> detect_platform_capabilities() {
    std::vector<PlatformCapability> caps;
#ifdef _WIN32
    caps.push_back({"input_capture", "planned", "Use low-level keyboard/mouse hooks."});
    caps.push_back({"input_injection", "planned", "Use SendInput."});
    caps.push_back({"clipboard", "planned", "Use Win32 Clipboard API."});
#elif defined(__APPLE__)
    caps.push_back({"input_capture", "planned", "Use CGEventTap with Accessibility/Input Monitoring permissions."});
    caps.push_back({"input_injection", "planned", "Use CGEventPost with Accessibility permission."});
    caps.push_back({"clipboard", "planned", "Use NSPasteboard."});
#elif defined(__linux__)
    const char* session = std::getenv("XDG_SESSION_TYPE");
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    bool has_uinput = std::filesystem::exists("/dev/uinput");
    caps.push_back({"session", session ? session : "unknown", "DISPLAY=" + std::string(display ? display : "") + " WAYLAND_DISPLAY=" + std::string(wayland ? wayland : "")});
    caps.push_back({"input_capture", display ? "possible-x11" : "unavailable-here", display ? "X11 backend can be added with XInput/XRecord." : "No graphical DISPLAY in this session."});
    caps.push_back({"input_injection", has_uinput ? "needs-permission" : "missing-uinput", has_uinput ? "/dev/uinput exists but may require udev/group/root access." : "/dev/uinput was not found."});
    caps.push_back({"clipboard", (display || wayland) ? "possible" : "file-adapter-only", "MVP uses a file-backed clipboard adapter until native GUI clipboard adapters are added."});
#else
    caps.push_back({"platform", "unknown", "No native adapters defined."});
#endif
    return caps;
}

}  // namespace mybarrier
