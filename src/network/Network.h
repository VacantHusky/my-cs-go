#pragma once

#include "gameplay/Simulation.h"

#include <cstdint>
#include <queue>
#include <string>
#include <vector>

namespace mycsg::network {

enum class SessionType {
    Offline,
    Host,
    Client
};

struct Packet {
    std::vector<std::uint8_t> bytes;
};

struct Endpoint {
    std::string host = "127.0.0.1";
    std::uint16_t port = 37015;
};

class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;
    virtual bool open(SessionType sessionType, const Endpoint& endpoint) = 0;
    virtual void close() = 0;
    virtual void send(const Packet& packet, const Endpoint& endpoint) = 0;
    virtual std::vector<Packet> poll() = 0;
};

class LoopbackTransport final : public INetworkTransport {
public:
    bool open(SessionType sessionType, const Endpoint& endpoint) override;
    void close() override;
    void send(const Packet& packet, const Endpoint& endpoint) override;
    std::vector<Packet> poll() override;

private:
    bool isOpen_ = false;
    std::queue<Packet> inbox_;
};

class UdpTransport final : public INetworkTransport {
public:
    UdpTransport();
    ~UdpTransport() override;

    bool open(SessionType sessionType, const Endpoint& endpoint) override;
    void close() override;
    void send(const Packet& packet, const Endpoint& endpoint) override;
    std::vector<Packet> poll() override;

private:
    std::intptr_t socket_ = -1;
};

struct NetworkSessionConfig {
    SessionType type = SessionType::Offline;
    Endpoint endpoint;
};

struct SessionSnapshot {
    float elapsedSeconds = 0.0f;
    std::vector<gameplay::PlayerState> players;
};

class NetworkSession {
public:
    explicit NetworkSession(NetworkSessionConfig config = {});

    bool start();
    void stop();
    void update(const gameplay::SimulationWorld& world);
    SessionSnapshot latestSnapshot() const { return latestSnapshot_; }

private:
    NetworkSessionConfig config_;
    LoopbackTransport loopback_;
    UdpTransport udp_;
    INetworkTransport* activeTransport_ = nullptr;
    SessionSnapshot latestSnapshot_;
};

}  // namespace mycsg::network
