#include "net.h"

#include <cstring>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mybarrier {
namespace {

void set_reuse_addr(mb_socket_t socket) {
    int yes = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
}

void set_broadcast(mb_socket_t socket) {
    int yes = 1;
    setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof(yes));
}

}  // namespace

SocketRuntime::SocketRuntime() {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif
}

SocketRuntime::~SocketRuntime() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void close_socket(mb_socket_t socket) {
    if (socket == invalid_socket_value) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

bool send_all(mb_socket_t socket, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        int rc = ::send(socket, data + sent, static_cast<int>(size - sent), 0);
#else
        ssize_t rc = ::send(socket, data + sent, size - sent, 0);
#endif
        if (rc <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(rc);
    }
    return true;
}

bool send_all(mb_socket_t socket, const std::string& data) {
    return send_all(socket, data.data(), data.size());
}

std::optional<std::string> recv_line(mb_socket_t socket) {
    std::string line;
    char ch = 0;
    while (true) {
#ifdef _WIN32
        int rc = ::recv(socket, &ch, 1, 0);
#else
        ssize_t rc = ::recv(socket, &ch, 1, 0);
#endif
        if (rc <= 0) {
            return std::nullopt;
        }
        if (ch == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }
        line.push_back(ch);
        if (line.size() > 64 * 1024) {
            return std::nullopt;
        }
    }
}

bool recv_exact(mb_socket_t socket, std::vector<char>& data, std::size_t size) {
    data.assign(size, 0);
    std::size_t received = 0;
    while (received < size) {
#ifdef _WIN32
        int rc = ::recv(socket, data.data() + received, static_cast<int>(size - received), 0);
#else
        ssize_t rc = ::recv(socket, data.data() + received, size - received, 0);
#endif
        if (rc <= 0) {
            return false;
        }
        received += static_cast<std::size_t>(rc);
    }
    return true;
}

mb_socket_t tcp_listen(uint16_t port, int backlog) {
    mb_socket_t server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server == invalid_socket_value) {
        throw std::runtime_error("socket failed: " + last_socket_error());
    }
    set_reuse_addr(server);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::string error = last_socket_error();
        close_socket(server);
        throw std::runtime_error("bind failed on tcp port " + std::to_string(port) + ": " + error);
    }
    if (::listen(server, backlog) != 0) {
        std::string error = last_socket_error();
        close_socket(server);
        throw std::runtime_error("listen failed: " + error);
    }
    return server;
}

AcceptedSocket tcp_accept(mb_socket_t server_socket) {
    sockaddr_in addr{};
#ifdef _WIN32
    int len = sizeof(addr);
#else
    socklen_t len = sizeof(addr);
#endif
    mb_socket_t client = ::accept(server_socket, reinterpret_cast<sockaddr*>(&addr), &len);
    if (client == invalid_socket_value) {
        return {};
    }
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return {client, ip};
}

mb_socket_t tcp_connect(const std::string& host, uint16_t port, int /*timeout_ms*/) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;
    std::string port_text = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), port_text.c_str(), &hints, &results);
    if (rc != 0 || results == nullptr) {
        return invalid_socket_value;
    }

    mb_socket_t socket = invalid_socket_value;
    for (addrinfo* item = results; item != nullptr; item = item->ai_next) {
        socket = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (socket == invalid_socket_value) {
            continue;
        }
        if (::connect(socket, item->ai_addr, static_cast<int>(item->ai_addrlen)) == 0) {
            break;
        }
        close_socket(socket);
        socket = invalid_socket_value;
    }
    freeaddrinfo(results);
    return socket;
}

mb_socket_t udp_bind(uint16_t port, bool broadcast) {
    mb_socket_t socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket == invalid_socket_value) {
        throw std::runtime_error("udp socket failed: " + last_socket_error());
    }
    set_reuse_addr(socket);
    if (broadcast) {
        set_broadcast(socket);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::string error = last_socket_error();
        close_socket(socket);
        throw std::runtime_error("bind failed on udp port " + std::to_string(port) + ": " + error);
    }
    return socket;
}

bool udp_send_to(mb_socket_t socket, const std::string& host, uint16_t port, const std::string& data) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        return false;
    }
    int rc = sendto(socket, data.data(), static_cast<int>(data.size()), 0,
                    reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return rc == static_cast<int>(data.size());
}

std::optional<UdpPacket> udp_recv_from(mb_socket_t socket, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(socket, &fds);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int rc = select(static_cast<int>(socket + 1), &fds, nullptr, nullptr, &tv);
    if (rc <= 0) {
        return std::nullopt;
    }

    char buffer[2048] = {0};
    sockaddr_in addr{};
#ifdef _WIN32
    int len = sizeof(addr);
#else
    socklen_t len = sizeof(addr);
#endif
    int n = recvfrom(socket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&addr), &len);
    if (n <= 0) {
        return std::nullopt;
    }
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    UdpPacket packet;
    packet.remote_ip = ip;
    packet.remote_port = ntohs(addr.sin_port);
    packet.message = std::string(buffer, static_cast<std::size_t>(n));
    return packet;
}

std::string last_socket_error() {
#ifdef _WIN32
    return std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

std::string local_ip_hint() {
    mb_socket_t socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket == invalid_socket_value) {
        return "127.0.0.1";
    }
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    connect(socket, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));
    sockaddr_in local{};
#ifdef _WIN32
    int len = sizeof(local);
#else
    socklen_t len = sizeof(local);
#endif
    std::string result = "127.0.0.1";
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
        result = ip;
    }
    close_socket(socket);
    return result;
}

}  // namespace mybarrier
