#include "util.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

namespace mybarrier {
namespace {

char hex_digit(unsigned value) {
    constexpr char digits[] = "0123456789abcdef";
    return digits[value & 0x0f];
}

std::string to_hex_u64(uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << value;
    return out.str();
}

}  // namespace

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream input(value);
    while (std::getline(input, current, delimiter)) {
        parts.push_back(current);
    }
    if (!value.empty() && value.back() == delimiter) {
        parts.emplace_back();
    }
    return parts;
}

std::string random_hex(std::size_t bytes) {
    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<unsigned> dist(0, 255);
    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        unsigned byte = dist(rng);
        out.push_back(hex_digit(byte >> 4));
        out.push_back(hex_digit(byte));
    }
    return out;
}

std::string fnv1a64_hex(const std::string& data) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : data) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return to_hex_u64(hash);
}

std::string fnv1a64_hex(const std::vector<char>& data) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : data) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return to_hex_u64(hash);
}

std::string escape_field(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string unescape_field(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out.push_back(value[i]);
            continue;
        }
        char next = value[++i];
        switch (next) {
            case 't': out.push_back('\t'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case '\\': out.push_back('\\'); break;
            default:
                out.push_back('\\');
                out.push_back(next);
                break;
        }
    }
    return out;
}

std::string sanitize_filename(const std::string& name) {
    std::filesystem::path path(name);
    std::string filename = path.filename().string();
    if (filename.empty() || filename == "." || filename == "..") {
        return "received.bin";
    }
    for (char& ch : filename) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == '/' || ch == '\\' || std::iscntrl(uch)) {
            ch = '_';
        }
    }
    return filename;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to write " + path.string());
    }
    output << data;
}

std::vector<char> read_binary_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read " + path.string());
    }
    input.seekg(0, std::ios::end);
    auto size = input.tellg();
    input.seekg(0, std::ios::beg);
    std::vector<char> data(static_cast<std::size_t>(size));
    if (!data.empty()) {
        input.read(data.data(), static_cast<std::streamsize>(data.size()));
    }
    return data;
}

void write_binary_file(const std::filesystem::path& path, const std::vector<char>& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to write " + path.string());
    }
    if (!data.empty()) {
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
}

std::string hostname() {
    char buffer[256] = {0};
#ifdef _WIN32
    if (gethostname(buffer, sizeof(buffer)) == 0) {
#else
    if (::gethostname(buffer, sizeof(buffer)) == 0) {
#endif
        return buffer;
    }
    return "mybarrier-device";
}

std::string now_iso8601() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

uint16_t parse_port(const std::string& value, uint16_t fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        int parsed = std::stoi(value);
        if (parsed > 0 && parsed <= 65535) {
            return static_cast<uint16_t>(parsed);
        }
    } catch (...) {
    }
    return fallback;
}

}  // namespace mybarrier
