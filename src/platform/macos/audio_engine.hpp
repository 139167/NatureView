#pragma once

#include <cstdint>
#include <memory>

#include "nature/audio/soundscape.hpp"

namespace natureview::platform {

class MacAudioEngine {
public:
    MacAudioEngine();
    ~MacAudioEngine();

    MacAudioEngine(const MacAudioEngine&) = delete;
    MacAudioEngine& operator=(const MacAudioEngine&) = delete;

    [[nodiscard]] bool start(
        const SoundscapeParameters& initialParameters,
        std::uint64_t seed);
    void update(const SoundscapeParameters& parameters);
    void stop();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] float sampleRate() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace natureview::platform
