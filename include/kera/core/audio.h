// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace kera
{

    class AudioBuffer
    {
    public:
        AudioBuffer();
        ~AudioBuffer();

        bool loadFromFile(const std::string& filename);
        bool loadFromMemory(const void* data, size_t size, int frequency, int channels);

        const std::vector<float>& getData() const
        {
            return m_data;
        }
        int getFrequency() const
        {
            return m_frequency;
        }
        int getChannels() const
        {
            return m_channels;
        }
        float getDuration() const;

    private:
        std::vector<float> m_data;
        int m_frequency;
        int m_channels;
    };

    class AudioSource
    {
    public:
        AudioSource();
        ~AudioSource();

        void setBuffer(const AudioBuffer* buffer);
        void play();
        void pause();
        void stop();
        void setLoop(bool loop);
        void setVolume(float volume);
        void setPitch(float pitch);

        bool isPlaying() const;
        bool isPaused() const;
        bool isStopped() const;

    private:
        const AudioBuffer* m_buffer;
        bool m_playing;
        bool m_paused;
        bool m_loop;
        float m_volume;
        float m_pitch;
        size_t m_position;
    };

    class Audio
    {
    public:
        static Audio& getInstance();

        bool initialize();
        void shutdown();

        void update();

        // Factory methods
        std::unique_ptr<AudioBuffer> createBuffer();
        std::unique_ptr<AudioSource> createSource();

    private:
        Audio() = default;
        ~Audio() = default;

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        bool m_initialized;
    };

}  // namespace kera
