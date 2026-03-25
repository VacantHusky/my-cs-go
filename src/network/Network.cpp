#include "network/Network.h"

#include "gameplay/MapData.h"

#include <spdlog/spdlog.h>

#include <enet/enet.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace mycsg::network {

namespace {

constexpr std::size_t kEnetChannelCount = 2;
constexpr std::uint32_t kSnapshotMagic = 0x4D59434Eu;
constexpr std::uint16_t kSnapshotVersion = 1;
constexpr float kRemotePlayerTimeoutSeconds = 3.0f;
constexpr float kMapBroadcastIntervalSeconds = 1.0f;

enum class PacketKind : std::uint8_t {
    Snapshot = 1,
    PlayerState = 2,
    MapState = 3,
    SessionEnded = 4,
};

int gEnetRuntimeRefs = 0;

const char* sessionTypeLabel(const SessionType type) {
    switch (type) {
        case SessionType::Offline:
            return "Offline";
        case SessionType::Host:
            return "Host";
        case SessionType::Client:
            return "Client";
    }
    return "Unknown";
}

bool acquireEnetRuntime() {
    if (gEnetRuntimeRefs == 0 && enet_initialize() != 0) {
        spdlog::error("[Network] ENet runtime 初始化失败。");
        return false;
    }
    ++gEnetRuntimeRefs;
    return true;
}

void releaseEnetRuntime() {
    if (gEnetRuntimeRefs <= 0) {
        return;
    }

    --gEnetRuntimeRefs;
    if (gEnetRuntimeRefs == 0) {
        enet_deinitialize();
    }
}

template <typename T>
void appendValue(std::vector<std::uint8_t>& bytes, const T value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

template <typename T>
bool readValue(const std::vector<std::uint8_t>& bytes, std::size_t& offset, T& outValue) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (offset + sizeof(T) > bytes.size()) {
        return false;
    }

    std::memcpy(&outValue, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

void appendString(std::vector<std::uint8_t>& bytes, const std::string& value) {
    const std::uint16_t length = static_cast<std::uint16_t>(
        std::min<std::size_t>(value.size(), std::numeric_limits<std::uint16_t>::max()));
    appendValue(bytes, length);
    bytes.insert(bytes.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(length));
}

bool readString(const std::vector<std::uint8_t>& bytes, std::size_t& offset, std::string& outValue) {
    std::uint16_t length = 0;
    if (!readValue(bytes, offset, length) || offset + length > bytes.size()) {
        return false;
    }

    outValue.assign(reinterpret_cast<const char*>(bytes.data() + offset), length);
    offset += length;
    return true;
}

void appendLargeString(std::vector<std::uint8_t>& bytes, const std::string& value) {
    const std::uint32_t length = static_cast<std::uint32_t>(
        std::min<std::size_t>(value.size(), std::numeric_limits<std::uint32_t>::max()));
    appendValue(bytes, length);
    bytes.insert(bytes.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(length));
}

bool readLargeString(const std::vector<std::uint8_t>& bytes, std::size_t& offset, std::string& outValue) {
    std::uint32_t length = 0;
    if (!readValue(bytes, offset, length) || offset + length > bytes.size()) {
        return false;
    }

    outValue.assign(reinterpret_cast<const char*>(bytes.data() + offset), length);
    offset += length;
    return true;
}

void appendPlayerState(std::vector<std::uint8_t>& bytes, const gameplay::PlayerState& player) {
    appendString(bytes, player.id);
    appendString(bytes, player.displayName);
    appendValue(bytes, static_cast<std::uint8_t>(player.team));
    appendValue(bytes, player.position.x);
    appendValue(bytes, player.position.y);
    appendValue(bytes, player.position.z);
    appendValue(bytes, player.velocity.x);
    appendValue(bytes, player.velocity.y);
    appendValue(bytes, player.velocity.z);
    appendValue(bytes, player.health);
    appendValue(bytes, static_cast<std::uint8_t>(player.botControlled ? 1 : 0));
    appendString(bytes, player.loadout.primaryWeaponId);
    appendString(bytes, player.loadout.secondaryWeaponId);
    appendString(bytes, player.loadout.tacticalGrenadeId);
    appendString(bytes, player.loadout.lethalGrenadeId);
    appendValue(bytes, static_cast<std::uint8_t>(player.loadout.optic));
}

bool readPlayerState(const std::vector<std::uint8_t>& bytes, std::size_t& offset, gameplay::PlayerState& player) {
    std::uint8_t team = 0;
    std::uint8_t botControlled = 0;
    std::uint8_t optic = 0;
    if (!readString(bytes, offset, player.id) ||
        !readString(bytes, offset, player.displayName) ||
        !readValue(bytes, offset, team) ||
        !readValue(bytes, offset, player.position.x) ||
        !readValue(bytes, offset, player.position.y) ||
        !readValue(bytes, offset, player.position.z) ||
        !readValue(bytes, offset, player.velocity.x) ||
        !readValue(bytes, offset, player.velocity.y) ||
        !readValue(bytes, offset, player.velocity.z) ||
        !readValue(bytes, offset, player.health) ||
        !readValue(bytes, offset, botControlled) ||
        !readString(bytes, offset, player.loadout.primaryWeaponId) ||
        !readString(bytes, offset, player.loadout.secondaryWeaponId) ||
        !readString(bytes, offset, player.loadout.tacticalGrenadeId) ||
        !readString(bytes, offset, player.loadout.lethalGrenadeId) ||
        !readValue(bytes, offset, optic)) {
        return false;
    }

    player.team = static_cast<gameplay::Team>(team);
    player.botControlled = botControlled != 0;
    player.loadout.optic = static_cast<content::OpticType>(optic);
    return true;
}

Packet encodeSnapshotPacket(const SessionSnapshot& snapshot) {
    Packet packet;
    packet.reliable = false;
    packet.bytes.reserve(256 + snapshot.players.size() * 128);

    appendValue(packet.bytes, kSnapshotMagic);
    appendValue(packet.bytes, kSnapshotVersion);
    appendValue(packet.bytes, static_cast<std::uint8_t>(PacketKind::Snapshot));
    appendValue(packet.bytes, static_cast<std::uint16_t>(
        std::min<std::size_t>(snapshot.players.size(), std::numeric_limits<std::uint16_t>::max())));
    appendValue(packet.bytes, snapshot.elapsedSeconds);

    for (const gameplay::PlayerState& player : snapshot.players) {
        appendPlayerState(packet.bytes, player);
    }

    return packet;
}

Packet encodePlayerStatePacket(const gameplay::PlayerState& player) {
    Packet packet;
    packet.reliable = false;
    packet.bytes.reserve(192);
    appendValue(packet.bytes, kSnapshotMagic);
    appendValue(packet.bytes, kSnapshotVersion);
    appendValue(packet.bytes, static_cast<std::uint8_t>(PacketKind::PlayerState));
    appendPlayerState(packet.bytes, player);
    return packet;
}

Packet encodeMapStatePacket(const MapSyncState& mapState) {
    Packet packet;
    packet.reliable = true;
    packet.bytes.reserve(96 + mapState.mapContent.size());
    appendValue(packet.bytes, kSnapshotMagic);
    appendValue(packet.bytes, kSnapshotVersion);
    appendValue(packet.bytes, static_cast<std::uint8_t>(PacketKind::MapState));
    appendValue(packet.bytes, mapState.revision);
    appendString(packet.bytes, mapState.sourceLabel);
    appendString(packet.bytes, mapState.mapName);
    appendLargeString(packet.bytes, mapState.mapContent);
    return packet;
}

Packet encodeSessionEndedPacket(const std::string_view reason) {
    Packet packet;
    packet.reliable = true;
    packet.bytes.reserve(64 + reason.size());
    appendValue(packet.bytes, kSnapshotMagic);
    appendValue(packet.bytes, kSnapshotVersion);
    appendValue(packet.bytes, static_cast<std::uint8_t>(PacketKind::SessionEnded));
    appendString(packet.bytes, std::string(reason));
    return packet;
}

bool decodePacketHeader(const Packet& packet,
                        PacketKind* outKind,
                        std::size_t& offset) {
    offset = 0;
    std::uint32_t magic = 0;
    std::uint16_t version = 0;
    std::uint8_t kind = 0;
    if (!readValue(packet.bytes, offset, magic) ||
        !readValue(packet.bytes, offset, version) ||
        !readValue(packet.bytes, offset, kind)) {
        return false;
    }
    if (magic != kSnapshotMagic || version != kSnapshotVersion ||
        kind < static_cast<std::uint8_t>(PacketKind::Snapshot) ||
        kind > static_cast<std::uint8_t>(PacketKind::SessionEnded)) {
        return false;
    }
    *outKind = static_cast<PacketKind>(kind);
    return true;
}

bool decodePacketHeader(const Packet& packet,
                        PacketKind expectedKind,
                        std::size_t& offset) {
    PacketKind decodedKind = PacketKind::Snapshot;
    if (!decodePacketHeader(packet, &decodedKind, offset)) {
        return false;
    }
    if (decodedKind != expectedKind) {
        return false;
    }
    return true;
}

bool decodeSnapshotPacket(const Packet& packet, SessionSnapshot& outSnapshot) {
    std::size_t offset = 0;
    std::uint16_t playerCount = 0;
    if (!decodePacketHeader(packet, PacketKind::Snapshot, offset) ||
        !readValue(packet.bytes, offset, playerCount) ||
        !readValue(packet.bytes, offset, outSnapshot.elapsedSeconds)) {
        return false;
    }

    outSnapshot.players.clear();
    outSnapshot.players.reserve(playerCount);
    for (std::uint16_t index = 0; index < playerCount; ++index) {
        gameplay::PlayerState player;
        if (!readPlayerState(packet.bytes, offset, player)) {
            return false;
        }
        outSnapshot.players.push_back(std::move(player));
    }

    return true;
}

bool decodePlayerStatePacket(const Packet& packet, gameplay::PlayerState& outPlayer) {
    std::size_t offset = 0;
    return decodePacketHeader(packet, PacketKind::PlayerState, offset) &&
           readPlayerState(packet.bytes, offset, outPlayer);
}

bool decodeMapStatePacket(const Packet& packet, MapSyncState& outMapState) {
    std::size_t offset = 0;
    return decodePacketHeader(packet, PacketKind::MapState, offset) &&
           readValue(packet.bytes, offset, outMapState.revision) &&
           readString(packet.bytes, offset, outMapState.sourceLabel) &&
           readString(packet.bytes, offset, outMapState.mapName) &&
           readLargeString(packet.bytes, offset, outMapState.mapContent);
}

bool decodeSessionEndedPacket(const Packet& packet, std::string& outReason) {
    std::size_t offset = 0;
    return decodePacketHeader(packet, PacketKind::SessionEnded, offset) &&
           readString(packet.bytes, offset, outReason);
}

std::optional<PacketKind> packetKind(const Packet& packet) {
    std::size_t offset = 0;
    PacketKind kind = PacketKind::Snapshot;
    if (!decodePacketHeader(packet, &kind, offset)) {
        return std::nullopt;
    }
    return kind;
}

void erasePeer(std::vector<ENetPeer*>& peers, ENetPeer* peer) {
    peers.erase(std::remove(peers.begin(), peers.end(), peer), peers.end());
}

}  // namespace

struct EnetTransport::Impl {
    SessionType sessionType = SessionType::Offline;
    Endpoint endpoint{};
    ENetHost* host = nullptr;
    ENetPeer* serverPeer = nullptr;
    std::vector<ENetPeer*> connectedPeers;
    std::size_t maxPeers = 8;
    bool runtimeAcquired = false;
    bool clientDisconnected = false;
};

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

EnetTransport::EnetTransport() : impl_(std::make_unique<Impl>()) {}

EnetTransport::~EnetTransport() {
    close();
}

EnetTransport::EnetTransport(EnetTransport&&) noexcept = default;
EnetTransport& EnetTransport::operator=(EnetTransport&&) noexcept = default;

void EnetTransport::setMaxPeers(const std::size_t maxPeers) {
    impl_->maxPeers = std::max<std::size_t>(1, maxPeers);
}

bool EnetTransport::open(const SessionType sessionType, const Endpoint& endpoint) {
    close();

    if (!acquireEnetRuntime()) {
        return false;
    }
    impl_->runtimeAcquired = true;
    impl_->sessionType = sessionType;
    impl_->endpoint = endpoint;
    impl_->clientDisconnected = false;

    constexpr std::size_t kDefaultBandwidth = 0;
    if (sessionType == SessionType::Host) {
        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = endpoint.port;
        impl_->host = enet_host_create(&address,
                                       impl_->maxPeers,
                                       kEnetChannelCount,
                                       kDefaultBandwidth,
                                       kDefaultBandwidth);
        if (impl_->host == nullptr) {
            spdlog::error("[Network] ENet Host 创建失败: port={} maxPeers={}", endpoint.port, impl_->maxPeers);
            close();
            return false;
        }

        spdlog::info("[Network] ENet Host ready on 0.0.0.0:{} (channels={} maxPeers={})",
                     endpoint.port,
                     kEnetChannelCount,
                     impl_->maxPeers);
        return true;
    }

    impl_->host = enet_host_create(nullptr, 1, kEnetChannelCount, kDefaultBandwidth, kDefaultBandwidth);
    if (impl_->host == nullptr) {
        spdlog::error("[Network] ENet Client host 创建失败。");
        close();
        return false;
    }

    ENetAddress address{};
    address.port = endpoint.port;
    if (enet_address_set_host(&address, endpoint.host.c_str()) != 0) {
        spdlog::error("[Network] ENet 无法解析目标地址: {}:{}", endpoint.host, endpoint.port);
        close();
        return false;
    }

    impl_->serverPeer = enet_host_connect(impl_->host, &address, kEnetChannelCount, 0);
    if (impl_->serverPeer == nullptr) {
        spdlog::error("[Network] ENet 连接创建失败: {}:{}", endpoint.host, endpoint.port);
        close();
        return false;
    }

    spdlog::info("[Network] ENet {} transport 已启动: {}:{}",
                 sessionTypeLabel(sessionType),
                 endpoint.host,
                 endpoint.port);
    return true;
}

void EnetTransport::close() {
    if (!impl_) {
        return;
    }

    if (impl_->host != nullptr) {
        if (impl_->serverPeer != nullptr) {
            enet_peer_disconnect(impl_->serverPeer, 0);
        }
        for (ENetPeer* peer : impl_->connectedPeers) {
            if (peer != nullptr) {
                enet_peer_disconnect(peer, 0);
            }
        }

        enet_host_flush(impl_->host);

        ENetEvent event{};
        for (int attempts = 0; attempts < 4; ++attempts) {
            while (enet_host_service(impl_->host, &event, 50) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_RECEIVE:
                        enet_packet_destroy(event.packet);
                        break;
                    case ENET_EVENT_TYPE_DISCONNECT:
                        erasePeer(impl_->connectedPeers, event.peer);
                        if (impl_->serverPeer == event.peer) {
                            impl_->serverPeer = nullptr;
                            impl_->clientDisconnected = true;
                        }
                        break;
                    case ENET_EVENT_TYPE_CONNECT:
                    case ENET_EVENT_TYPE_NONE:
                        break;
                }
            }
            if (impl_->serverPeer == nullptr && impl_->connectedPeers.empty()) {
                break;
            }
        }

        if (impl_->serverPeer != nullptr) {
            enet_peer_reset(impl_->serverPeer);
            impl_->serverPeer = nullptr;
        }
        for (ENetPeer* peer : impl_->connectedPeers) {
            if (peer != nullptr) {
                enet_peer_reset(peer);
            }
        }
        impl_->connectedPeers.clear();
        enet_host_destroy(impl_->host);
        impl_->host = nullptr;
    }
    impl_->clientDisconnected = false;

    if (impl_->runtimeAcquired) {
        releaseEnetRuntime();
        impl_->runtimeAcquired = false;
    }
}

void EnetTransport::send(const Packet& packet, const Endpoint&) {
    if (!impl_ || impl_->host == nullptr || packet.bytes.empty()) {
        return;
    }

    ENetPacket* enetPacket = enet_packet_create(packet.bytes.data(),
                                                packet.bytes.size(),
                                                packet.reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (enetPacket == nullptr) {
        return;
    }

    bool queued = false;
    if (impl_->sessionType == SessionType::Host) {
        if (!impl_->connectedPeers.empty()) {
            enet_host_broadcast(impl_->host, 0, enetPacket);
            queued = true;
        }
    } else if (impl_->serverPeer != nullptr && impl_->serverPeer->state == ENET_PEER_STATE_CONNECTED) {
        queued = enet_peer_send(impl_->serverPeer, 0, enetPacket) == 0;
    }

    if (!queued) {
        enet_packet_destroy(enetPacket);
        return;
    }

    enet_host_flush(impl_->host);
}

std::vector<Packet> EnetTransport::poll() {
    std::vector<Packet> packets;
    if (!impl_ || impl_->host == nullptr) {
        return packets;
    }

    ENetEvent event{};
    while (enet_host_service(impl_->host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                if (impl_->sessionType == SessionType::Host) {
                    impl_->connectedPeers.push_back(event.peer);
                } else {
                    impl_->serverPeer = event.peer;
                }
                spdlog::info("[Network] ENet peer connected | type={} peerCount={}",
                             sessionTypeLabel(impl_->sessionType),
                             impl_->sessionType == SessionType::Host ? impl_->connectedPeers.size() : 1);
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                Packet packet;
                packet.bytes.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                packet.reliable = (event.packet->flags & ENET_PACKET_FLAG_RELIABLE) != 0;
                packets.push_back(std::move(packet));
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                erasePeer(impl_->connectedPeers, event.peer);
                if (impl_->serverPeer == event.peer) {
                    impl_->serverPeer = nullptr;
                    impl_->clientDisconnected = true;
                }
                spdlog::warn("[Network] ENet peer disconnected | type={} remainingPeers={}",
                             sessionTypeLabel(impl_->sessionType),
                             impl_->connectedPeers.size());
                break;
            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }

    return packets;
}

bool EnetTransport::clientDisconnected() const {
    return impl_ && impl_->clientDisconnected;
}

void EnetTransport::clearClientDisconnected() {
    if (impl_) {
        impl_->clientDisconnected = false;
    }
}

NetworkSession::NetworkSession(NetworkSessionConfig config) : config_(std::move(config)) {}

NetworkSession::~NetworkSession() {
    stop();
}

NetworkSession::NetworkSession(NetworkSession&&) noexcept = default;
NetworkSession& NetworkSession::operator=(NetworkSession&&) noexcept = default;

bool NetworkSession::start() {
    remotePlayerLastSeenSeconds_.clear();
    latestSnapshot_ = {};
    latestMapState_ = {};
    localPlayerStateValid_ = false;
    remoteSessionEnded_ = false;
    remoteSessionEndReason_.clear();
    lastMapBroadcastSeconds_ = -1000.0f;
    lastBroadcastMapRevision_ = 0;
    if (config_.type == SessionType::Offline) {
        activeTransport_ = &loopback_;
    } else {
        enet_.setMaxPeers(config_.maxPeers);
        activeTransport_ = &enet_;
    }
    return activeTransport_->open(config_.type, config_.endpoint);
}

void NetworkSession::stop(const std::string_view reason) {
    if (activeTransport_ != nullptr && config_.type == SessionType::Host) {
        const std::string closeReason = reason.empty() ? "主机已关闭房间" : std::string(reason);
        activeTransport_->send(encodeSessionEndedPacket(closeReason), config_.endpoint);
    }
    if (activeTransport_ != nullptr) {
        activeTransport_->close();
    }
    activeTransport_ = nullptr;
    remotePlayerLastSeenSeconds_.clear();
    latestSnapshot_ = {};
    latestMapState_ = {};
    localPlayerStateValid_ = false;
    remoteSessionEnded_ = false;
    remoteSessionEndReason_.clear();
    lastMapBroadcastSeconds_ = -1000.0f;
    lastBroadcastMapRevision_ = 0;
}

void NetworkSession::setLocalPlayerState(gameplay::PlayerState player) {
    localPlayerState_ = std::move(player);
    localPlayerStateValid_ = !localPlayerState_.id.empty();
}

void NetworkSession::setHostMapState(const std::string& sourceLabel, const gameplay::MapData& map) {
    const std::string serialized = gameplay::MapSerializer::serialize(map);
    if (serialized.empty()) {
        return;
    }

    const bool changed = hostMapState_.sourceLabel != sourceLabel ||
        hostMapState_.mapName != map.name ||
        hostMapState_.mapContent != serialized;
    if (!changed) {
        return;
    }

    hostMapState_.revision += 1;
    hostMapState_.sourceLabel = sourceLabel;
    hostMapState_.mapName = map.name;
    hostMapState_.mapContent = serialized;
    if (config_.type != SessionType::Client) {
        latestMapState_ = hostMapState_;
    }
    lastMapBroadcastSeconds_ = -1000.0f;
    lastBroadcastMapRevision_ = 0;
}

void NetworkSession::update(gameplay::SimulationWorld& world) {
    if (activeTransport_ == nullptr) {
        return;
    }

    for (const Packet& packet : activeTransport_->poll()) {
        if (config_.type == SessionType::Client) {
            const auto kind = packetKind(packet);
            if (!kind.has_value()) {
                continue;
            }

            if (*kind == PacketKind::Snapshot) {
                SessionSnapshot snapshot;
                if (decodeSnapshotPacket(packet, snapshot)) {
                    latestSnapshot_ = std::move(snapshot);
                }
            } else if (*kind == PacketKind::MapState) {
                MapSyncState mapState;
                if (decodeMapStatePacket(packet, mapState) && mapState.revision >= latestMapState_.revision) {
                    latestMapState_ = std::move(mapState);
                }
            } else if (*kind == PacketKind::SessionEnded) {
                std::string reason;
                if (decodeSessionEndedPacket(packet, reason)) {
                    remoteSessionEnded_ = true;
                    remoteSessionEndReason_ = reason.empty() ? "主机已关闭房间" : std::move(reason);
                }
            }
            continue;
        }

        const auto kind = packetKind(packet);
        if (!kind.has_value() || *kind != PacketKind::PlayerState) {
            continue;
        }

        gameplay::PlayerState remotePlayer;
        if (!decodePlayerStatePacket(packet, remotePlayer) ||
            remotePlayer.id.empty() ||
            remotePlayer.id == config_.localPlayerId) {
            continue;
        }

        world.upsertPlayer(remotePlayer);
        remotePlayerLastSeenSeconds_[remotePlayer.id] = world.elapsedSeconds();
    }

    if (config_.type != SessionType::Client) {
        std::vector<std::string> expiredRemoteIds;
        expiredRemoteIds.reserve(remotePlayerLastSeenSeconds_.size());
        for (const auto& [playerId, lastSeenSeconds] : remotePlayerLastSeenSeconds_) {
            if (world.elapsedSeconds() - lastSeenSeconds > kRemotePlayerTimeoutSeconds) {
                expiredRemoteIds.push_back(playerId);
            }
        }
        for (const std::string& playerId : expiredRemoteIds) {
            world.removePlayerById(playerId);
            remotePlayerLastSeenSeconds_.erase(playerId);
        }
    }

    if (config_.type == SessionType::Client && enet_.clientDisconnected()) {
        remoteSessionEnded_ = true;
        if (remoteSessionEndReason_.empty()) {
            remoteSessionEndReason_ = "与主机的连接已断开";
        }
        enet_.clearClientDisconnected();
    }

    if (config_.type == SessionType::Client) {
        if (localPlayerStateValid_) {
            activeTransport_->send(encodePlayerStatePacket(localPlayerState_), config_.endpoint);
        }
        return;
    }

    const SessionSnapshot localSnapshot{
        .elapsedSeconds = world.elapsedSeconds(),
        .players = world.players(),
    };
    latestSnapshot_ = localSnapshot;
    activeTransport_->send(encodeSnapshotPacket(localSnapshot), config_.endpoint);

    if (config_.type == SessionType::Host &&
        hostMapState_.revision != 0 &&
        (hostMapState_.revision != lastBroadcastMapRevision_ ||
         world.elapsedSeconds() - lastMapBroadcastSeconds_ >= kMapBroadcastIntervalSeconds)) {
        activeTransport_->send(encodeMapStatePacket(hostMapState_), config_.endpoint);
        lastBroadcastMapRevision_ = hostMapState_.revision;
        lastMapBroadcastSeconds_ = world.elapsedSeconds();
    }
}

}  // namespace mycsg::network
