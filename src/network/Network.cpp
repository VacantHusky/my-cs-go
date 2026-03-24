#include "network/Network.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mycsg::network {

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr std::intptr_t kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr std::intptr_t kInvalidSocket = -1;
#endif

NativeSocket nativeSocket(const std::intptr_t socketHandle) {
    return static_cast<NativeSocket>(socketHandle);
}

void closeSocket(const std::intptr_t socketHandle) {
#ifdef _WIN32
    closesocket(nativeSocket(socketHandle));
#else
    close(nativeSocket(socketHandle));
#endif
}

}  // namespace

bool LoopbackTransport::open(SessionType, const Endpoint&) {
    isOpen_ = true;
    return true;
}

void LoopbackTransport::close() {
    isOpen_ = false;
    inbox_ = {};
}

void LoopbackTransport::send(const Packet& packet, const Endpoint&) {
    if (isOpen_) {
        inbox_.push(packet);
    }
}

std::vector<Packet> LoopbackTransport::poll() {
    std::vector<Packet> packets;
    while (!inbox_.empty()) {
        packets.push_back(std::move(inbox_.front()));
        inbox_.pop();
    }
    return packets;
}

UdpTransport::UdpTransport() = default;

UdpTransport::~UdpTransport() {
    close();
}

bool UdpTransport::open(SessionType, const Endpoint& endpoint) {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return false;
    }
#endif

    socket_ = static_cast<std::intptr_t>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (socket_ == kInvalidSocket) {
        close();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(nativeSocket(socket_), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close();
        return false;
    }

#ifdef _WIN32
    u_long nonBlocking = 1;
    ioctlsocket(nativeSocket(socket_), FIONBIO, &nonBlocking);
#else
    const int flags = fcntl(nativeSocket(socket_), F_GETFL, 0);
    fcntl(nativeSocket(socket_), F_SETFL, flags | O_NONBLOCK);
#endif
    return true;
}

void UdpTransport::close() {
    if (socket_ != kInvalidSocket) {
        closeSocket(socket_);
        socket_ = kInvalidSocket;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void UdpTransport::send(const Packet& packet, const Endpoint& endpoint) {
    if (socket_ == kInvalidSocket) {
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    inet_pton(AF_INET, endpoint.host.c_str(), &address.sin_addr);
    ::sendto(nativeSocket(socket_),
             reinterpret_cast<const char*>(packet.bytes.data()),
             static_cast<int>(packet.bytes.size()),
             0,
             reinterpret_cast<sockaddr*>(&address),
             sizeof(address));
}

std::vector<Packet> UdpTransport::poll() {
    std::vector<Packet> packets;
    if (socket_ == kInvalidSocket) {
        return packets;
    }

    for (;;) {
        std::uint8_t buffer[1400];
        sockaddr_in from{};
#ifdef _WIN32
        int fromLength = sizeof(from);
#else
        socklen_t fromLength = sizeof(from);
#endif
        const int received = static_cast<int>(::recvfrom(nativeSocket(socket_),
            reinterpret_cast<char*>(buffer),
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&from),
            &fromLength));

        if (received <= 0) {
            break;
        }

        Packet packet;
        packet.bytes.assign(buffer, buffer + received);
        packets.push_back(std::move(packet));
    }
    return packets;
}

NetworkSession::NetworkSession(NetworkSessionConfig config) : config_(std::move(config)) {}

bool NetworkSession::start() {
    if (config_.type == SessionType::Offline) {
        activeTransport_ = &loopback_;
    } else {
        activeTransport_ = &udp_;
    }
    return activeTransport_->open(config_.type, config_.endpoint);
}

void NetworkSession::stop() {
    if (activeTransport_ != nullptr) {
        activeTransport_->close();
    }
    activeTransport_ = nullptr;
}

void NetworkSession::update(const gameplay::SimulationWorld& world) {
    latestSnapshot_.elapsedSeconds = world.elapsedSeconds();
    latestSnapshot_.players = world.players();

    if (activeTransport_ == nullptr) {
        return;
    }

    Packet packet;
    packet.bytes.resize(sizeof(float));
    std::memcpy(packet.bytes.data(), &latestSnapshot_.elapsedSeconds, sizeof(float));
    activeTransport_->send(packet, config_.endpoint);
    activeTransport_->poll();
}

}  // namespace mycsg::network
