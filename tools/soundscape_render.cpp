#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>

#include "nature/audio/soundscape.hpp"
#include "nature/core/simulation.hpp"

namespace {

void printUsage(std::ostream& output) {
    output << "Usage: soundscape_render [--seed N] [--seconds N] "
              "[--duration N] [--output FILE.wav]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::locale::global(std::locale::classic());
    std::uint64_t seed = 139167;
    double prewarmSeconds = 60.0;
    double durationSeconds = 5.0;
    std::string outputPath = "build/nature-soundscape.wav";

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
            } else if (argument == "--duration") {
                durationSeconds = std::stod(argv[++index]);
            } else if (argument == "--output") {
                outputPath = argv[++index];
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
        !std::isfinite(durationSeconds) || durationSeconds <= 0.0 ||
        durationSeconds > 60.0) {
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
    const natureview::SoundscapeParameters parameters =
        director.evaluate(
            simulation.environment(),
            simulation.camera(),
            simulation.world());
    const natureview::OfflineSoundscapeRenderer renderer;
    const std::vector<float> samples = renderer.render(
        parameters,
        {48000, durationSeconds, seed});
    if (!natureview::writeStereoWav(outputPath, samples, 48000)) {
        std::cerr << "Failed to write soundscape WAV\n";
        return EXIT_FAILURE;
    }

    std::cout << std::fixed << std::setprecision(4)
              << "output=" << outputPath << '\n'
              << "seed=" << seed << '\n'
              << "simulation_seconds=" << simulation.elapsedSeconds() << '\n'
              << "wind_gain=" << parameters.windGain << '\n'
              << "water_gain=" << parameters.waterGain << '\n'
              << "water_pan=" << parameters.waterPan << '\n'
              << "rain_gain=" << parameters.rainGain << '\n'
              << "duration_seconds=" << durationSeconds << '\n';
    return EXIT_SUCCESS;
}
