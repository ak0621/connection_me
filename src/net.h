#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
using mb_socket_t = SOCKET;
#else
using mb_socket_t = int;
#endif

namespace mybarrier {

constexpr uint16_t default_port = 37373;
constexpr uint16_t default_discovery_port = 37374;
constexpr mb_socket_t invalid_socket_value = static_cast<mb_socket_t>(-1);

struct AcceptedSocket {
    mb_socket_t socket = invalid_socket_value;
    std::string remote_ip;
};

struct UdpPacket {
    std::string remote_ip;
    uint16_t remote_port = 0;
    std::string message;
};

class SocketRuntime {
public:
    SocketRuntime();
    ~SocketRuntime();
};

void close_socket(mb_socket_t socket);
bool send_all(mb_socket_t socket, const char* data, std::size_t size);
bool send_all(mb_socket_t socket, const std::string& data);
std::optional<std::string> recv_line(mb_socket_t socket);
bool recv_exact(mb_socket_t socket, std::vector<char>& data, std::size_t size);

mb_socket_t tcp_listen(uint16_t port, int backlog = 32);
AcceptedSocket tcp_accept(mb_socket_t server_socket);
mb_socket_t tcp_connect(const std::string& host, uint16_t port, int timeout_ms = 3000);

mb_socket_t udp_bind(uint16_t port, bool broadcast = false);
bool udp_send_to(mb_socket_t socket, const std::string& host, uint16_t port, const std::string& data);
std::optional<UdpPacket> udp_recv_from(mb_socket_t socket, int timeout_ms);

std::string last_socket_error();
std::string local_ip_hint();

}  // namespace mybarrier
