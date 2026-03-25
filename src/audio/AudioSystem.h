#pragma once

#include <memory>

namespace mycsg::audio {

enum class AudioCue {
    WeaponShot,
    HitConfirm,
    Reload,
    Jump,
    Footstep,
    Landing,
    ThrowableThrow,
    FragExplosion,
    FlashBurst,
    SmokeBurst,
    MeleeSwing,
    UiAccept,
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool initialize(float masterVolume, float effectsVolume);
    void shutdown();

    bool isReady() const;
    void setMix(float masterVolume, float effectsVolume);
    void play(AudioCue cue);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mycsg::audio
