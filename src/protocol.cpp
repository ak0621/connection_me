#include "protocol.h"

#include "util.h"

#include <sstream>
#include <utility>

namespace mybarrier {

bool write_frame(mb_socket_t socket, const Frame& frame) {
    std::ostringstream out;
    out << frame.command << "\n";
    bool has_length = false;
    for (const auto& [key, value] : frame.headers) {
        if (key == "Content-Length") {
            has_length = true;
        }
        out << key << ": " << value << "\n";
    }
    if (!has_length) {
        out << "Content-Length: " << frame.body.size() << "\n";
    }
    out << "\n";
    std::string header = out.str();
    if (!send_all(socket, header)) {
        return false;
    }
    if (!frame.body.empty()) {
        return send_all(socket, frame.body.data(), frame.body.size());
    }
    return true;
}

std::optional<Frame> read_frame(mb_socket_t socket) {
    auto command_line = recv_line(socket);
    if (!command_line || command_line->empty()) {
        return std::nullopt;
    }
    Frame frame;
    frame.command = trim(*command_line);
    while (true) {
        auto line = recv_line(socket);
        if (!line) {
            return std::nullopt;
        }
        if (line->empty()) {
            break;
        }
        auto pos = line->find(':');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = trim(line->substr(0, pos));
        std::string value = trim(line->substr(pos + 1));
        frame.headers[key] = value;
    }
    std::size_t length = header_size(frame, "Content-Length", 0);
    if (length > max_frame_body_bytes) {
        return std::nullopt;
    }
    if (length > 0 && !recv_exact(socket, frame.body, length)) {
        return std::nullopt;
    }
    return frame;
}

Frame ok_frame(std::map<std::string, std::string> headers, std::vector<char> body) {
    Frame frame;
    frame.command = "OK";
    frame.headers = std::move(headers);
    frame.body = std::move(body);
    return frame;
}

Frame error_frame(const std::string& message) {
    Frame frame;
    frame.command = "ERR";
    frame.headers["Message"] = message;
    return frame;
}

std::string header_value(const Frame& frame, const std::string& key, const std::string& fallback) {
    auto it = frame.headers.find(key);
    if (it == frame.headers.end()) {
        return fallback;
    }
    return it->second;
}

std::size_t header_size(const Frame& frame, const std::string& key, std::size_t fallback) {
    auto text = header_value(frame, key);
    if (text.empty()) {
        return fallback;
    }
    try {
        return static_cast<std::size_t>(std::stoull(text));
    } catch (...) {
        return fallback;
    }
}

}  // namespace mybarrier
