#include "kera/core/audio.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <algorithm>

namespace kera {

// AudioBuffer implementation
AudioBuffer::AudioBuffer()
    : frequency_(0)
    , channels_(0) {
}

AudioBuffer::~AudioBuffer() {
    // Cleanup if needed
}

bool AudioBuffer::loadFromFile(const std::string& filename) {
    // TODO: Implement audio file loading (WAV, OGG, etc.)
    // For now, just create a simple test tone
    std::cout << "Loading audio from file: " << filename << std::endl;

    // Generate a simple sine wave for testing
    frequency_ = 44100;
    channels_ = 1;
    const float duration = 1.0f; // 1 second
    const size_t sampleCount = static_cast<size_t>(frequency_ * duration);

    data_.resize(sampleCount);
    const float frequency = 440.0f; // A4 note

    for (size_t i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / frequency_;
        data_[i] = std::sin(2.0f * 3.14159f * frequency * t) * 0.1f; // Low volume
    }

    return true;
}

bool AudioBuffer::loadFromMemory(const void* data, size_t size, int frequency, int channels) {
    // TODO: Implement loading from memory buffer
    std::cout << "Loading audio from memory: " << size << " bytes, "
              << frequency << "Hz, " << channels << " channels" << std::endl;

    frequency_ = frequency;
    channels_ = channels;
    data_.assign(static_cast<const float*>(data), static_cast<const float*>(data) + size);

    return true;
}

float AudioBuffer::getDuration() const {
    if (frequency_ == 0 || channels_ == 0) {
        return 0.0f;
    }
    return static_cast<float>(data_.size()) / (frequency_ * channels_);
}

// AudioSource implementation
AudioSource::AudioSource()
    : buffer_(nullptr)
    , playing_(false)
    , paused_(false)
    , loop_(false)
    , volume_(1.0f)
    , pitch_(1.0f)
    , position_(0) {
}

AudioSource::~AudioSource() {
    stop();
}

void AudioSource::setBuffer(const AudioBuffer* buffer) {
    if (playing_) {
        stop();
    }
    buffer_ = buffer;
    position_ = 0;
}

void AudioSource::play() {
    if (buffer_ && !playing_) {
        playing_ = true;
        paused_ = false;
        std::cout << "Audio source started playing" << std::endl;
    }
}

void AudioSource::pause() {
    if (playing_ && !paused_) {
        paused_ = true;
        std::cout << "Audio source paused" << std::endl;
    }
}

void AudioSource::stop() {
    playing_ = false;
    paused_ = false;
    position_ = 0;
    std::cout << "Audio source stopped" << std::endl;
}

void AudioSource::setLoop(bool loop) {
    loop_ = loop;
}

void AudioSource::setVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void AudioSource::setPitch(float pitch) {
    pitch_ = std::max(pitch, 0.0f);
}

bool AudioSource::isPlaying() const {
    return playing_ && !paused_;
}

bool AudioSource::isPaused() const {
    return playing_ && paused_;
}

bool AudioSource::isStopped() const {
    return !playing_;
}

// Audio implementation
Audio& Audio::getInstance() {
    static Audio instance;
    return instance;
}

bool Audio::initialize() {
    if (initialized_) {
        return true;
    }

    // Initialize SDL audio
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::cerr << "Failed to initialize SDL audio: " << SDL_GetError() << std::endl;
        return false;
    }

    // TODO: Set up audio device and mixer

    initialized_ = true;
    std::cout << "Audio system initialized" << std::endl;
    return true;
}

void Audio::shutdown() {
    if (!initialized_) {
        return;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized_ = false;
    std::cout << "Audio system shutdown" << std::endl;
}

void Audio::update() {
    if (!initialized_) {
        return;
    }

    // TODO: Update audio sources, mix audio, etc.
    // This would typically be called once per frame
}

std::unique_ptr<AudioBuffer> Audio::createBuffer() {
    return std::make_unique<AudioBuffer>();
}

std::unique_ptr<AudioSource> Audio::createSource() {
    return std::make_unique<AudioSource>();
}

} // namespace kera