#include "audio/AudioSystem.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#include <miniaudio.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <utility>

namespace mycsg::audio {

namespace {

constexpr std::size_t kMaxVoices = 24;
constexpr float kPi = 3.1415926535f;

enum class Waveform {
    Sine,
    Square,
    Noise,
};

struct Voice {
    bool active = false;
    Waveform waveform = Waveform::Sine;
    float startFrequency = 440.0f;
    float endFrequency = 440.0f;
    float gain = 0.2f;
    float duration = 0.1f;
    float time = 0.0f;
    float pan = 0.0f;
    std::uint32_t noiseState = 0x12345678u;
};

float lerp(const float a, const float b, const float t) {
    return a + (b - a) * t;
}

float nextNoise(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state & 0x00ffffffu) / static_cast<float>(0x007fffffu) - 1.0f;
}

Voice makeVoice(const Waveform waveform,
                const float startFrequency,
                const float endFrequency,
                const float gain,
                const float duration,
                const float pan = 0.0f) {
    Voice voice;
    voice.active = true;
    voice.waveform = waveform;
    voice.startFrequency = startFrequency;
    voice.endFrequency = endFrequency;
    voice.gain = gain;
    voice.duration = duration;
    voice.pan = pan;
    return voice;
}

}  // namespace

struct AudioSystem::Impl {
    ma_device device{};
    std::mutex mutex;
    std::array<Voice, kMaxVoices> voices{};
    float masterVolume = 1.0f;
    float effectsVolume = 1.0f;
    bool ready = false;

    static void dataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount) {
        auto* self = static_cast<Impl*>(device->pUserData);
        auto* samples = static_cast<float*>(output);
        if (self == nullptr) {
            std::fill(samples, samples + frameCount * 2, 0.0f);
            return;
        }

        std::scoped_lock lock(self->mutex);
        std::fill(samples, samples + frameCount * 2, 0.0f);

        for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
            float left = 0.0f;
            float right = 0.0f;

            for (auto& voice : self->voices) {
                if (!voice.active) {
                    continue;
                }

                const float t = std::clamp(voice.time / std::max(0.0001f, voice.duration), 0.0f, 1.0f);
                const float envelope = (1.0f - t) * (1.0f - t);
                if (envelope <= 0.0001f) {
                    voice.active = false;
                    continue;
                }

                const float frequency = lerp(voice.startFrequency, voice.endFrequency, t);
                float sample = 0.0f;
                switch (voice.waveform) {
                    case Waveform::Sine:
                        sample = std::sin(2.0f * kPi * frequency * voice.time);
                        break;
                    case Waveform::Square:
                        sample = std::sin(2.0f * kPi * frequency * voice.time) >= 0.0f ? 1.0f : -1.0f;
                        break;
                    case Waveform::Noise:
                        sample = nextNoise(voice.noiseState);
                        break;
                }

                sample *= voice.gain * envelope * self->masterVolume * self->effectsVolume;
                const float panL = 0.5f * (1.0f - voice.pan);
                const float panR = 0.5f * (1.0f + voice.pan);
                left += sample * panL;
                right += sample * panR;

                voice.time += 1.0f / static_cast<float>(device->sampleRate);
                if (voice.time >= voice.duration) {
                    voice.active = false;
                }
            }

            samples[frame * 2] = std::clamp(left, -1.0f, 1.0f);
            samples[frame * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
        }
    }

    bool initialize(const float master, const float effects) {
        shutdown();

        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 2;
        config.sampleRate = 48000;
        config.dataCallback = dataCallback;
        config.pUserData = this;

        if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
            spdlog::warn("[Audio] Failed to initialize miniaudio playback device. Continuing without sound.");
            return false;
        }
        if (ma_device_start(&device) != MA_SUCCESS) {
            spdlog::warn("[Audio] Failed to start miniaudio playback device. Continuing without sound.");
            ma_device_uninit(&device);
            return false;
        }

        masterVolume = master;
        effectsVolume = effects;
        ready = true;
        spdlog::info("[Audio] miniaudio playback ready at {} Hz.", device.sampleRate);
        return true;
    }

    void shutdown() {
        if (!ready) {
            return;
        }
        ma_device_uninit(&device);
        ready = false;
    }

    void pushVoice(const Voice& voice) {
        if (!ready) {
            return;
        }
        std::scoped_lock lock(mutex);
        for (auto& slot : voices) {
            if (!slot.active) {
                slot = voice;
                return;
            }
        }
        voices.front() = voice;
    }

    void play(const AudioCue cue) {
        switch (cue) {
            case AudioCue::WeaponShot:
                pushVoice(makeVoice(Waveform::Square, 150.0f, 70.0f, 0.18f, 0.10f));
                pushVoice(makeVoice(Waveform::Noise, 0.0f, 0.0f, 0.08f, 0.05f, -0.1f));
                break;
            case AudioCue::HitConfirm:
                pushVoice(makeVoice(Waveform::Sine, 1200.0f, 760.0f, 0.12f, 0.08f));
                break;
            case AudioCue::Reload:
                pushVoice(makeVoice(Waveform::Square, 420.0f, 240.0f, 0.08f, 0.06f, -0.2f));
                pushVoice(makeVoice(Waveform::Square, 280.0f, 180.0f, 0.08f, 0.05f, 0.2f));
                break;
            case AudioCue::Jump:
                pushVoice(makeVoice(Waveform::Sine, 220.0f, 160.0f, 0.10f, 0.08f));
                break;
            case AudioCue::Footstep:
                pushVoice(makeVoice(Waveform::Noise, 0.0f, 0.0f, 0.06f, 0.035f, -0.1f));
                pushVoice(makeVoice(Waveform::Square, 180.0f, 110.0f, 0.04f, 0.045f, 0.1f));
                break;
            case AudioCue::Landing:
                pushVoice(makeVoice(Waveform::Noise, 0.0f, 0.0f, 0.10f, 0.06f));
                pushVoice(makeVoice(Waveform::Sine, 140.0f, 70.0f, 0.06f, 0.10f));
                break;
            case AudioCue::ThrowableThrow:
                pushVoice(makeVoice(Waveform::Noise, 0.0f, 0.0f, 0.06f, 0.05f));
                pushVoice(makeVoice(Waveform::Sine, 520.0f, 260.0f, 0.05f, 0.07f));
                break;
            case AudioCue::FragExplosion:
                pushVoice(makeVoice(Waveform::Noise, 0.0f, 0.0f, 0.28f, 0.36f));
                pushVoice(makeVoice(Waveform::Sine, 120.0f, 40.0f, 0.12f, 0.22f));
                break;
            case AudioCue::FlashBurst:
                pushVoice(makeVoice(Waveform::Sine, 2200.0f, 900.0f, 0.14f, 0.20f));
                break;
            case AudioCue::SmokeBurst:
                pushVoice(makeVoice(Waveform::Noise, 0.0f, 0.0f, 0.11f, 0.28f));
                break;
            case AudioCue::MeleeSwing:
                pushVoice(makeVoice(Waveform::Sine, 480.0f, 160.0f, 0.10f, 0.09f));
                break;
            case AudioCue::UiAccept:
                pushVoice(makeVoice(Waveform::Sine, 880.0f, 1320.0f, 0.07f, 0.05f));
                break;
        }
    }
};

AudioSystem::AudioSystem() : impl_(std::make_unique<Impl>()) {}
AudioSystem::~AudioSystem() = default;

bool AudioSystem::initialize(const float masterVolume, const float effectsVolume) {
    return impl_->initialize(masterVolume, effectsVolume);
}

void AudioSystem::shutdown() {
    impl_->shutdown();
}

bool AudioSystem::isReady() const {
    return impl_->ready;
}

void AudioSystem::setMix(const float masterVolume, const float effectsVolume) {
    impl_->masterVolume = std::clamp(masterVolume, 0.0f, 1.0f);
    impl_->effectsVolume = std::clamp(effectsVolume, 0.0f, 1.0f);
}

void AudioSystem::play(const AudioCue cue) {
    impl_->play(cue);
}

}  // namespace mycsg::audio
