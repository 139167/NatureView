#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>
#include <thread>

#include "audio_engine.hpp"
#include "nature/audio/soundscape.hpp"
#include "nature/core/simulation.hpp"

namespace {

void printUsage(std::ostream& output) {
    output << "Usage: natureview_audio_demo [--seed N] [--seconds N] "
              "[--play-seconds N]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::locale::global(std::locale::classic());
    std::uint64_t seed = 139167;
    double prewarmSeconds = 60.0;
    double playSeconds = 3.0;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            printUsage(std::cout);
            return EXIT_SUCCESS;
        }
        if (index + 1 >= argc) {
            std::cerr << "Missing value for " << argument << '\n';
            return 2;
        }
        try {
            if (argument == "--seed") {
                seed = std::stoull(argv[++index]);
            } else if (argument == "--seconds") {
                prewarmSeconds = std::stod(argv[++index]);
            } else if (argument == "--play-seconds") {
                playSeconds = std::stod(argv[++index]);
            } else {
                std::cerr << "Unknown argument: " << argument << '\n';
                return 2;
            }
        } catch (const std::exception&) {
            std::cerr << "Invalid value for " << argument << '\n';
            return 2;
        }
    }

    if (!std::isfinite(prewarmSeconds) || prewarmSeconds < 0.0 ||
        prewarmSeconds > 3600.0 ||
        !std::isfinite(playSeconds) || playSeconds <= 0.0 ||
        playSeconds > 3600.0) {
        std::cerr << "Invalid time range\n";
        return 2;
    }

    natureview::NatureSimulation simulation(seed);
    constexpr double simulationRate = 60.0;
    const int prewarmFrames =
        static_cast<int>(std::ceil(prewarmSeconds * simulationRate));
    for (int frame = 0; frame < prewarmFrames; ++frame) {
        simulation.advance(1.0 / simulationRate);
    }

    const natureview::SoundscapeDirector director;
    natureview::platform::MacAudioEngine audio;
    natureview::SoundscapeParameters parameters =
        director.evaluate(
            simulation.environment(),
            simulation.camera(),
            simulation.world());
    if (!audio.start(parameters, seed)) {
        std::cerr << "Failed to start macOS audio output\n";
        return EXIT_FAILURE;
    }
    const bool audioStarted = audio.isRunning();

    auto previous = std::chrono::steady_clock::now();
    const auto start = previous;
    const auto finish =
        start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(playSeconds));
    while (std::chrono::steady_clock::now() < finish) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto current = std::chrono::steady_clock::now();
        const double delta = std::min(
            std::chrono::duration<double>(current - previous).count(),
            0.25);
        previous = current;
        simulation.advance(delta);
        parameters = director.evaluate(
            simulation.environment(),
            simulation.camera(),
            simulation.world());
        audio.update(parameters);
    }
    audio.stop();

    std::cout << std::fixed << std::setprecision(4)
              << "audio_started=" << (audioStarted ? "true" : "false") << '\n'
              << "sample_rate=" << audio.sampleRate() << '\n'
              << "seed=" << seed << '\n'
              << "simulation_seconds=" << simulation.elapsedSeconds() << '\n'
              << "wind_gain=" << parameters.windGain << '\n'
              << "water_gain=" << parameters.waterGain << '\n'
              << "water_pan=" << parameters.waterPan << '\n'
              << "rain_gain=" << parameters.rainGain << '\n';
    return EXIT_SUCCESS;
}
