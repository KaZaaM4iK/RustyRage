#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cmath>

namespace Core {

/**
 * Modern C++20 High-Fidelity Audio DSP & Procedural Synthesizer Engine.
 * Cross-platform: Linux, FreeBSD, OpenBSD, Windows, macOS, iOS, Android.
 * Features real-time dynamic range compression, analog biquad filtering, and zero-latency synthesis.
 */
class AudioManager {
public:
    enum class SoundEffect {
        MachineGun,
        RocketLaunch,
        Explosion,
        Crash,
        Nitro,
        PowerUp,
        CountdownTick,
        CountdownGo,
        OffTrackWarning
    };

    static AudioManager& getInstance() {
        static AudioManager instance;
        return instance;
    }

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool initialize(int sampleRate = 48000, int channels = 2);
    void shutdown();

    void setMasterVolume(float volume); // 0.0 to 1.0
    float getMasterVolume() const { return m_masterVolume.load(); }
    
    void setMuted(bool muted);
    bool isMuted() const { return m_isMuted.load(); }

    // Dynamic continuous vehicle engine audio
    void updateEngine(float speedRatio, bool isAccelerating);
    void stopEngine();

    // Trigger one-shot high quality DSP sound effects
    void playSound(SoundEffect effect);
    void playMachineGun() { playSound(SoundEffect::MachineGun); }
    void playRocketLaunch() { playSound(SoundEffect::RocketLaunch); }
    void playExplosion() { playSound(SoundEffect::Explosion); }
    void playCrash() { playSound(SoundEffect::Crash); }
    void playNitro() { playSound(SoundEffect::Nitro); }
    void playPowerUp() { playSound(SoundEffect::PowerUp); }
    void playCountdown(bool isGo) { playSound(isGo ? SoundEffect::CountdownGo : SoundEffect::CountdownTick); }
    void playOffTrackWarning() { playSound(SoundEffect::OffTrackWarning); }

    // Audio callback processor for SDL3 / miniaudio / native platform audio streams
    void processAudio(float* outputBuffer, size_t frameCount, int channels);

private:
    AudioManager();
    ~AudioManager();

    // DSP Voice structure
    struct Voice {
        bool active = false;
        SoundEffect type;
        float time = 0.0f;
        float duration = 0.0f;
        float param1 = 0.0f;
        float param2 = 0.0f;
        float phase = 0.0f;
        float filterState1 = 0.0f;
        float filterState2 = 0.0f;
    };

    // Engine synth state
    struct EngineSynth {
        float phase1 = 0.0f;
        float phase2 = 0.0f;
        float targetFreq = 45.0f;
        float currentFreq = 45.0f;
        float filterCutoff = 250.0f;
        float currentGain = 0.0f;
        float targetGain = 0.0f;
        float filterState = 0.0f;
        bool running = false;
    } m_engine;

    // Master Dynamic Limiter / Compressor state
    struct CompressorState {
        float envelope = 0.0f;
        float threshold = 0.7f; // ~ -3dB
        float ratio = 8.0f;
        float attackCoeff = 0.005f;
        float releaseCoeff = 0.0005f;
    } m_compressor;

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_isMuted{false};
    std::atomic<float> m_masterVolume{0.7f};
    int m_sampleRate = 48000;
    int m_channels = 2;

    std::mutex m_voiceMutex;
    static constexpr size_t MAX_VOICES = 32;
    std::vector<Voice> m_voices;

    // Internal DSP filters
    float applyLowpass(float input, float cutoff, float& state);
    float generateNoise();
    float applyCompressor(float input);
};

// Shorthand accessor
inline AudioManager& Audio() {
    return AudioManager::getInstance();
}

} // namespace Core
