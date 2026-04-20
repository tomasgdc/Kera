#pragma once

#include <memory>
#include <string>
#include <vector>

namespace kera {

class AudioBuffer {
public:
    AudioBuffer();
    ~AudioBuffer();

    bool loadFromFile(const std::string& filename);
    bool loadFromMemory(const void* data, size_t size, int frequency, int channels);

    const std::vector<float>& getData() const { return data_; }
    int getFrequency() const { return frequency_; }
    int getChannels() const { return channels_; }
    float getDuration() const;

private:
    std::vector<float> data_;
    int frequency_;
    int channels_;
};

class AudioSource {
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
    const AudioBuffer* buffer_;
    bool playing_;
    bool paused_;
    bool loop_;
    float volume_;
    float pitch_;
    size_t position_;
};

class Audio {
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

    bool initialized_;
};

} // namespace kera