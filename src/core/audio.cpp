// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/core/audio.h"

#include "kera/utilities/logger.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace kera
{

    // AudioBuffer implementation
    AudioBuffer::AudioBuffer() : m_frequency(0), m_channels(0) {}

    AudioBuffer::~AudioBuffer()
    {
        // Cleanup if needed
    }

    bool AudioBuffer::loadFromFile(const std::string& filename)
    {
        // TODO: Implement audio file loading (WAV, OGG, etc.)
        // For now, just create a simple test tone
        Logger::getInstance().info("Loading audio from file: " + filename);

        // Generate a simple sine wave for testing
        m_frequency = 44100;
        m_channels = 1;
        const float duration = 1.0f;  // 1 second
        const size_t sample_count = static_cast<size_t>(m_frequency * duration);

        m_data.resize(sample_count);
        const float frequency = 440.0f;  // A4 note

        for (size_t i = 0; i < sample_count; ++i)
        {
            float t = static_cast<float>(i) / m_frequency;
            m_data[i] = static_cast<float>(std::sin(2.0f * 3.14159f * frequency * t) * 0.1f);  // Low volume
        }

        return true;
    }

    bool AudioBuffer::loadFromMemory(const void* data, size_t size, int frequency, int channels)
    {
        // TODO: Implement loading from memory buffer
        Logger::getInstance().info("Loading audio from memory: " + std::to_string(size) + " bytes, " +
                                   std::to_string(frequency) + "Hz, " + std::to_string(channels) + " channels");

        m_frequency = frequency;
        m_channels = channels;
        m_data.assign(static_cast<const float*>(data), static_cast<const float*>(data) + size);

        return true;
    }

    float AudioBuffer::getDuration() const
    {
        if (m_frequency == 0 || m_channels == 0)
        {
            return 0.0f;
        }
        return static_cast<float>(m_data.size()) / (m_frequency * m_channels);
    }

    // AudioSource implementation
    AudioSource::AudioSource()
        : m_buffer(nullptr)
        , m_playing(false)
        , m_paused(false)
        , m_loop(false)
        , m_volume(1.0f)
        , m_pitch(1.0f)
        , m_position(0)
    {
    }

    AudioSource::~AudioSource()
    {
        stop();
    }

    void AudioSource::setBuffer(const AudioBuffer* buffer)
    {
        if (m_playing)
        {
            stop();
        }
        m_buffer = buffer;
        m_position = 0;
    }

    void AudioSource::play()
    {
        if (m_buffer && !m_playing)
        {
            m_playing = true;
            m_paused = false;
            Logger::getInstance().info("Audio source started playing");
        }
    }

    void AudioSource::pause()
    {
        if (m_playing && !m_paused)
        {
            m_paused = true;
            Logger::getInstance().info("Audio source paused");
        }
    }

    void AudioSource::stop()
    {
        m_playing = false;
        m_paused = false;
        m_position = 0;
        Logger::getInstance().info("Audio source stopped");
    }

    void AudioSource::setLoop(bool loop)
    {
        m_loop = loop;
    }

    void AudioSource::setVolume(float volume)
    {
        m_volume = std::clamp(volume, 0.0f, 1.0f);
    }

    void AudioSource::setPitch(float pitch)
    {
        m_pitch = std::max(pitch, 0.0f);
    }

    bool AudioSource::isPlaying() const
    {
        return m_playing && !m_paused;
    }

    bool AudioSource::isPaused() const
    {
        return m_playing && m_paused;
    }

    bool AudioSource::isStopped() const
    {
        return !m_playing;
    }

    // Audio implementation
    Audio& Audio::getInstance()
    {
        static Audio instance;
        return instance;
    }

    bool Audio::initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        // Initialize SDL audio
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            Logger::getInstance().error("Failed to initialize SDL audio: " + std::string(SDL_GetError()));
            return false;
        }

        // TODO: Set up audio device and mixer

        m_initialized = true;
        Logger::getInstance().info("Audio system initialized");
        return true;
    }

    void Audio::shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_initialized = false;
        Logger::getInstance().info("Audio system shutdown");
    }

    void Audio::update()
    {
        if (!m_initialized)
        {
            return;
        }

        // TODO: Update audio sources, mix audio, etc.
        // This would typically be called once per frame
    }

    std::unique_ptr<AudioBuffer> Audio::createBuffer()
    {
        return std::make_unique<AudioBuffer>();
    }

    std::unique_ptr<AudioSource> Audio::createSource()
    {
        return std::make_unique<AudioSource>();
    }

}  // namespace kera
