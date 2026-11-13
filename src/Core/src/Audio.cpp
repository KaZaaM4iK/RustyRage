#include "Core/Audio.hpp"
#include "Core/Logging.hpp"
#include <SDL3/SDL.h>
#include <random>
#include <algorithm>

namespace Core {

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;

    // Fast static pseudo-random generator for audio noise
    static uint32_t g_noiseSeed = 22222;
    inline float fastRandom() {
        g_noiseSeed = (g_noiseSeed * 196314165 + 907633515);
        return (static_cast<float>(g_noiseSeed) / static_cast<float>(0xFFFFFFFF)) * 2.0f - 1.0f;
    }
}

AudioManager::AudioManager() {
    m_voices.resize(MAX_VOICES);
}

AudioManager::~AudioManager() {
    shutdown();
}

static void SDLCALL SDLAudioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    if (additional_amount <= 0 || !userdata) return;
    
    auto* manager = static_cast<AudioManager*>(userdata);
    int sampleCount = additional_amount / sizeof(float);
    int frameCount = sampleCount / 2; // Stereo
    
    std::vector<float> buffer(sampleCount);
    manager->processAudio(buffer.data(), frameCount, 2);
    
    SDL_PutAudioStreamData(stream, buffer.data(), additional_amount);
}

static SDL_AudioStream* g_sdlAudioStream = nullptr;

bool AudioManager::initialize(int sampleRate, int channels) {
    if (m_initialized.load()) return true;

    m_sampleRate = sampleRate;
    m_channels = channels;

    LOG_INFOF("Initializing AudioManager: %d Hz, %d channels...", sampleRate, channels);

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_F32;
    spec.channels = channels;
    spec.freq = sampleRate;

    g_sdlAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, SDLAudioStreamCallback, this);
    if (!g_sdlAudioStream) {
        LOG_WARNINGF("SDL_OpenAudioDeviceStream failed: %s (falling back to silent mode)", SDL_GetError());
    } else {
        SDL_ResumeAudioStreamDevice(g_sdlAudioStream);
        LOG_INFO("AudioManager initialized successfully via SDL3 Audio Stream.");
    }

    m_initialized.store(true);
    return true;
}

void AudioManager::shutdown() {
    if (!m_initialized.load()) return;
    m_initialized.store(false);

    if (g_sdlAudioStream) {
        SDL_DestroyAudioStream(g_sdlAudioStream);
        g_sdlAudioStream = nullptr;
    }
    LOG_INFO("AudioManager shutdown completed.");
}

void AudioManager::setMasterVolume(float volume) {
    m_masterVolume.store(std::clamp(volume, 0.0f, 1.0f));
}

void AudioManager::setMuted(bool muted) {
    m_isMuted.store(muted);
}

void AudioManager::updateEngine(float speedRatio, bool isAccelerating) {
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    m_engine.running = true;
    m_engine.targetFreq = 38.0f + (speedRatio * 150.0f) + (isAccelerating ? 25.0f : 0.0f);
    m_engine.filterCutoff = 180.0f + (speedRatio * 520.0f) + (isAccelerating ? 120.0f : 0.0f);
    m_engine.targetGain = 0.08f + (speedRatio * 0.12f) + (isAccelerating ? 0.06f : 0.0f);
}

void AudioManager::stopEngine() {
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    m_engine.running = false;
    m_engine.targetGain = 0.0f;
}

void AudioManager::playSound(SoundEffect effect) {
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    
    // Find free voice slot or overwrite the oldest
    Voice* targetVoice = nullptr;
    float maxTime = -1.0f;
    
    for (auto& voice : m_voices) {
        if (!voice.active) {
            targetVoice = &voice;
            break;
        }
        if (voice.time > maxTime) {
            maxTime = voice.time;
            targetVoice = &voice;
        }
    }

    if (!targetVoice) return;

    targetVoice->active = true;
    targetVoice->type = effect;
    targetVoice->time = 0.0f;
    targetVoice->phase = 0.0f;
    targetVoice->filterState1 = 0.0f;
    targetVoice->filterState2 = 0.0f;

    switch (effect) {
        case SoundEffect::MachineGun:
            targetVoice->duration = 0.12f;
            targetVoice->param1 = 130.0f; // start frequency
            break;
        case SoundEffect::RocketLaunch:
            targetVoice->duration = 0.55f;
            targetVoice->param1 = 140.0f;
            break;
        case SoundEffect::Explosion:
            targetVoice->duration = 0.85f;
            targetVoice->param1 = 90.0f;
            break;
        case SoundEffect::Crash:
            targetVoice->duration = 0.35f;
            targetVoice->param1 = 140.0f;
            break;
        case SoundEffect::Nitro:
            targetVoice->duration = 0.45f;
            targetVoice->param1 = 260.0f;
            break;
        case SoundEffect::PowerUp:
            targetVoice->duration = 0.40f;
            targetVoice->param1 = 587.33f; // D5 note
            break;
        case SoundEffect::CountdownTick:
            targetVoice->duration = 0.15f;
            targetVoice->param1 = 440.0f; // A4
            break;
        case SoundEffect::CountdownGo:
            targetVoice->duration = 0.45f;
            targetVoice->param1 = 880.0f; // A5
            break;
        case SoundEffect::OffTrackWarning:
            targetVoice->duration = 0.25f;
            targetVoice->param1 = 320.0f;
            break;
    }
}

float AudioManager::applyLowpass(float input, float cutoff, float& state) {
    float dt = 1.0f / static_cast<float>(m_sampleRate);
    float rc = 1.0f / (TWO_PI * cutoff);
    float alpha = dt / (rc + dt);
    state += alpha * (input - state);
    return state;
}

float AudioManager::generateNoise() {
    return fastRandom();
}

float AudioManager::applyCompressor(float input) {
    float absVal = std::abs(input);
    if (absVal > m_compressor.envelope) {
        m_compressor.envelope += m_compressor.attackCoeff * (absVal - m_compressor.envelope);
    } else {
        m_compressor.envelope += m_compressor.releaseCoeff * (absVal - m_compressor.envelope);
    }

    float gain = 1.0f;
    if (m_compressor.envelope > m_compressor.threshold) {
        float overDb = 20.0f * std::log10(m_compressor.envelope / m_compressor.threshold);
        float compDb = overDb * (1.0f - 1.0f / m_compressor.ratio);
        gain = std::pow(10.0f, -compDb / 20.0f);
    }

    return input * gain;
}

void AudioManager::processAudio(float* outputBuffer, size_t frameCount, int channels) {
    if (m_isMuted.load()) {
        std::fill(outputBuffer, outputBuffer + frameCount * channels, 0.0f);
        return;
    }

    const float masterVol = m_masterVolume.load();
    const float dt = 1.0f / static_cast<float>(m_sampleRate);

    std::lock_guard<std::mutex> lock(m_voiceMutex);

    // Smooth engine interpolation
    m_engine.currentFreq += (m_engine.targetFreq - m_engine.currentFreq) * 0.05f;
    m_engine.currentGain += (m_engine.targetGain - m_engine.currentGain) * 0.05f;

    for (size_t i = 0; i < frameCount; ++i) {
        float mixedSample = 0.0f;

        // 1. Engine Synth DSP
        if (m_engine.currentGain > 0.001f) {
            m_engine.phase1 += m_engine.currentFreq * dt;
            if (m_engine.phase1 >= 1.0f) m_engine.phase1 -= 1.0f;

            m_engine.phase2 += (m_engine.currentFreq * 1.5f) * dt;
            if (m_engine.phase2 >= 1.0f) m_engine.phase2 -= 1.0f;

            // Warm triangle + rounded saw waveform
            float tri = (std::abs(m_engine.phase1 * 2.0f - 1.0f) * 2.0f - 1.0f);
            float saw = (m_engine.phase2 * 2.0f - 1.0f) * 0.4f;
            float rawEngine = tri + saw;

            float filteredEngine = applyLowpass(rawEngine, m_engine.filterCutoff, m_engine.filterState);
            mixedSample += filteredEngine * m_engine.currentGain;
        }

        // 2. Active Voices DSP
        for (auto& voice : m_voices) {
            if (!voice.active) continue;

            float sample = 0.0f;
            float progress = voice.time / voice.duration;
            float env = 1.0f - progress; // Linear decay

            switch (voice.type) {
                case SoundEffect::MachineGun: {
                    float currentFreq = voice.param1 * (1.0f - progress * 0.7f);
                    voice.phase += currentFreq * dt;
                    float punch = std::sin(voice.phase * TWO_PI) * 0.6f;
                    float noise = generateNoise() * 0.4f;
                    float filtered = applyLowpass(punch + noise, 900.0f, voice.filterState1);
                    sample = filtered * (env * env) * 0.35f;
                    break;
                }
                case SoundEffect::RocketLaunch: {
                    float freq = voice.param1 + (progress * 480.0f);
                    voice.phase += freq * dt;
                    float whistle = std::sin(voice.phase * TWO_PI) * 0.4f;
                    float roar = generateNoise() * 0.6f;
                    float filtered = applyLowpass(roar, 450.0f + progress * 800.0f, voice.filterState1);
                    sample = (whistle + filtered) * std::sin(progress * PI) * 0.30f;
                    break;
                }
                case SoundEffect::Explosion: {
                    float subFreq = voice.param1 * (1.0f - progress * 0.75f);
                    voice.phase += subFreq * dt;
                    float subBass = std::sin(voice.phase * TWO_PI) * 0.6f;
                    float shockwave = generateNoise() * 0.5f;
                    float filtered = applyLowpass(subBass + shockwave, 380.0f * (1.0f - progress * 0.6f), voice.filterState1);
                    sample = filtered * (env * env) * 0.45f;
                    break;
                }
                case SoundEffect::Crash: {
                    float slideFreq = voice.param1 * (1.0f - progress * 0.7f);
                    voice.phase += slideFreq * dt;
                    float body = (std::abs((voice.phase - std::floor(voice.phase)) * 2.0f - 1.0f) * 2.0f - 1.0f);
                    float crunch = generateNoise() * 0.4f;
                    float filtered = applyLowpass(body + crunch, 500.0f, voice.filterState1);
                    sample = filtered * env * 0.35f;
                    break;
                }
                case SoundEffect::Nitro: {
                    float freq = voice.param1 + (progress * 420.0f);
                    voice.phase += freq * dt;
                    float cleanTone = std::sin(voice.phase * TWO_PI);
                    sample = cleanTone * std::sin(progress * PI) * 0.28f;
                    break;
                }
                case SoundEffect::PowerUp: {
                    // 4-note crystal arpeggio
                    int noteIndex = static_cast<int>(progress * 4.0f);
                    float noteFreqs[] = { 587.33f, 739.99f, 880.00f, 1174.66f };
                    float freq = noteFreqs[std::clamp(noteIndex, 0, 3)];
                    voice.phase += freq * dt;
                    float crystal = std::sin(voice.phase * TWO_PI);
                    sample = crystal * env * 0.30f;
                    break;
                }
                case SoundEffect::CountdownTick: {
                    voice.phase += voice.param1 * dt;
                    float bell = std::sin(voice.phase * TWO_PI);
                    sample = bell * (env * env) * 0.30f;
                    break;
                }
                case SoundEffect::CountdownGo: {
                    voice.phase += voice.param1 * dt;
                    float goTone = std::sin(voice.phase * TWO_PI);
                    sample = goTone * env * 0.40f;
                    break;
                }
                case SoundEffect::OffTrackWarning: {
                    float sweep = voice.param1 - (progress * 100.0f);
                    voice.phase += sweep * dt;
                    float warn = (std::abs((voice.phase - std::floor(voice.phase)) * 2.0f - 1.0f) * 2.0f - 1.0f);
                    sample = warn * env * 0.25f;
                    break;
                }
            }

            mixedSample += sample;
            voice.time += dt;
            if (voice.time >= voice.duration) {
                voice.active = false;
            }
        }

        // 3. Dynamic Range Compression & Limiting
        float compressed = applyCompressor(mixedSample * masterVol);
        float finalSample = std::clamp(compressed, -0.98f, 0.98f);

        // Write stereo channels
        for (int ch = 0; ch < channels; ++ch) {
            outputBuffer[i * channels + ch] = finalSample;
        }
    }
}

} // namespace Core
