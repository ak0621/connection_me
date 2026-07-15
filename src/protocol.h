#pragma once

#include "net.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mybarrier {

constexpr int app_protocol_major = 0;
constexpr int app_protocol_minor = 3;
constexpr std::size_t max_frame_body_bytes = 8 * 1024 * 1024;

struct Frame {
    std::string command;
    std::map<std::string, std::string> headers;
    std::vector<char> body;
};

bool write_frame(mb_socket_t socket, const Frame& frame);
std::optional<Frame> read_frame(mb_socket_t socket);
Frame ok_frame(std::map<std::string, std::string> headers = {}, std::vector<char> body = {});
Frame error_frame(const std::string& message);
std::string header_value(const Frame& frame, const std::string& key, const std::string& fallback = {});
std::size_t header_size(const Frame& frame, const std::string& key, std::size_t fallback = 0);

}  // namespace mybarrier
