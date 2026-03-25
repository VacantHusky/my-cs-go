#pragma once

#include "gameplay/Simulation.h"

#include <cstdint>
#include <memory>
#include <queue>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

namespace mycsg::network {

enum class SessionType {
    Offline,
    Host,
    Client
};

struct Packet {
    std::vector<std::uint8_t> bytes;
    bool reliable = true;
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

class EnetTransport final : public INetworkTransport {
public:
    EnetTransport();
    ~EnetTransport() override;

    EnetTransport(const EnetTransport&) = delete;
    EnetTransport& operator=(const EnetTransport&) = delete;
    EnetTransport(EnetTransport&&) noexcept;
    EnetTransport& operator=(EnetTransport&&) noexcept;

    bool open(SessionType sessionType, const Endpoint& endpoint) override;
    void close() override;
    void send(const Packet& packet, const Endpoint& endpoint) override;
    std::vector<Packet> poll() override;
    void setMaxPeers(std::size_t maxPeers);
    bool clientDisconnected() const;
    void clearClientDisconnected();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct NetworkSessionConfig {
    SessionType type = SessionType::Offline;
    Endpoint endpoint;
    std::size_t maxPeers = 8;
    std::string localPlayerId = "p0";
    std::string localPlayerDisplayName = "本地玩家";
};

struct SessionSnapshot {
    float elapsedSeconds = 0.0f;
    std::vector<gameplay::PlayerState> players;
};

struct MapSyncState {
    std::uint64_t revision = 0;
    std::string sourceLabel;
    std::string mapName;
    std::string mapContent;
};

class NetworkSession {
public:
    explicit NetworkSession(NetworkSessionConfig config = {});
    ~NetworkSession();

    NetworkSession(const NetworkSession&) = delete;
    NetworkSession& operator=(const NetworkSession&) = delete;
    NetworkSession(NetworkSession&&) noexcept;
    NetworkSession& operator=(NetworkSession&&) noexcept;

    bool start();
    void stop(std::string_view reason = {});
    void setLocalPlayerState(gameplay::PlayerState player);
    void setHostMapState(const std::string& sourceLabel, const gameplay::MapData& map);
    void update(gameplay::SimulationWorld& world);
    SessionSnapshot latestSnapshot() const { return latestSnapshot_; }
    MapSyncState latestMapState() const { return latestMapState_; }
    const std::string& localPlayerId() const { return config_.localPlayerId; }
    bool remoteSessionEnded() const { return remoteSessionEnded_; }
    std::string_view remoteSessionEndReason() const { return remoteSessionEndReason_; }

private:
    NetworkSessionConfig config_;
    LoopbackTransport loopback_;
    EnetTransport enet_;
    INetworkTransport* activeTransport_ = nullptr;
    SessionSnapshot latestSnapshot_;
    MapSyncState hostMapState_;
    MapSyncState latestMapState_;
    gameplay::PlayerState localPlayerState_{};
    bool localPlayerStateValid_ = false;
    bool remoteSessionEnded_ = false;
    std::string remoteSessionEndReason_;
    float lastMapBroadcastSeconds_ = -1000.0f;
    std::uint64_t lastBroadcastMapRevision_ = 0;
    std::unordered_map<std::string, float> remotePlayerLastSeenSeconds_;
};

}  // namespace mycsg::network
