#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "nature/audio/soundscape.hpp"
#include "nature/core/simulation.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

float rms(const std::vector<float>& samples) {
    float sum = 0.0F;
    for (const float sample : samples) {
        sum += sample * sample;
    }
    return std::sqrt(sum / static_cast<float>(samples.size()));
}

}  // namespace

int main() {
    natureview::NatureSimulation simulation(139167);
    for (int tick = 0; tick < 1200; ++tick) {
        simulation.advance(natureview::NatureSimulation::kFixedStepSeconds);
    }

    const natureview::SoundscapeDirector director;
    const auto& world = simulation.world();
    const auto baseEnvironment = simulation.environment();
    const auto baseCamera = simulation.camera();
    const natureview::SoundscapeParameters base =
        director.evaluate(baseEnvironment, baseCamera, world);

    expect(base.windGain >= 0.0F && base.windGain <= 1.0F,
           "wind gain must stay normalized");
    expect(base.waterGain >= 0.0F && base.waterGain <= 1.0F,
           "water gain must stay normalized");
    expect(base.waterPan >= -1.0F && base.waterPan <= 1.0F,
           "water pan must stay normalized");
    expect(base.rainGain >= 0.0F && base.rainGain <= 1.0F,
           "rain gain must stay normalized");

    auto windyEnvironment = baseEnvironment;
    windyEnvironment.windSpeedMetersPerSecond += 5.0F;
    const natureview::SoundscapeParameters windy =
        director.evaluate(windyEnvironment, baseCamera, world);
    expect(windy.windGain > base.windGain, "stronger wind must raise wind gain");

    auto rainyEnvironment = baseEnvironment;
    rainyEnvironment.rainIntensity = 0.9F;
    const natureview::SoundscapeParameters rainy =
        director.evaluate(rainyEnvironment, baseCamera, world);
    expect(rainy.rainGain > base.rainGain, "stronger rain must raise rain gain");

    auto farCamera = baseCamera;
    farCamera.position.x = world.riverXAt(farCamera.position.z) + 1200.0F;
    const natureview::SoundscapeParameters distant =
        director.evaluate(baseEnvironment, farCamera, world);
    expect(base.waterGain > distant.waterGain,
           "camera closer to river must raise water gain");

    natureview::OfflineSoundscapeRenderer renderer;
    const std::vector<float> first = renderer.render(base, {48000, 0.25, 77});
    const std::vector<float> second = renderer.render(base, {48000, 0.25, 77});
    expect(first == second, "same soundscape input must render identically");
    expect(first.size() == 12000U * 2U, "soundscape must be stereo at 48 kHz");
    expect(rms(first) > 0.001F, "soundscape must contain audible energy");

    const bool finite = std::all_of(
        first.begin(), first.end(), [](float sample) {
            return std::isfinite(sample) && sample >= -1.0F && sample <= 1.0F;
        });
    expect(finite, "soundscape samples must be finite and normalized");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All audio tests passed\n";
    return EXIT_SUCCESS;
}
