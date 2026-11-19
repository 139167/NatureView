#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <string>
#include <vector>

#include "nature/core/simulation.hpp"

namespace {

struct Options {
    std::uint64_t seed = 139167;
    double seconds = 60.0;
    int framesPerSecond = 60;
};

void printUsage(std::ostream& output) {
    output << "Usage: naturecore_sim [--seed N] [--seconds N] [--fps N]\n";
}

bool parseOptions(int argc, char** argv, Options& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            printUsage(std::cout);
            return false;
        }
        if (index + 1 >= argc) {
            std::cerr << "Missing value for " << argument << '\n';
            return false;
        }

        try {
            if (argument == "--seed") {
                options.seed = std::stoull(argv[++index]);
            } else if (argument == "--seconds") {
                options.seconds = std::stod(argv[++index]);
            } else if (argument == "--fps") {
                options.framesPerSecond = std::stoi(argv[++index]);
            } else {
                std::cerr << "Unknown argument: " << argument << '\n';
                return false;
            }
        } catch (const std::exception&) {
            std::cerr << "Invalid value for " << argument << '\n';
            return false;
        }
    }

    if (!std::isfinite(options.seconds) || options.seconds < 0.0 ||
        options.seconds > 3600.0) {
        std::cerr << "--seconds must be between 0 and 3600\n";
        return false;
    }
    if (options.framesPerSecond < 20 || options.framesPerSecond > 240) {
        std::cerr << "--fps must be between 20 and 240\n";
        return false;
    }
    return true;
}

void printSnapshot(const natureview::NatureSimulation& simulation) {
    const auto& environment = simulation.environment();
    const auto& camera = simulation.camera();

    std::cout << std::fixed << std::setprecision(4)
              << "\"time_s\":" << simulation.elapsedSeconds()
              << ",\"solar_time_hour\":"
              << environment.solarTimeSeconds / 3600.0
              << ",\"cloud_cover\":" << environment.cloudCover
              << ",\"rain_intensity\":" << environment.rainIntensity
              << ",\"wind_speed_mps\":" << environment.windSpeedMetersPerSecond
              << ",\"river_discharge_m3ps\":"
              << environment.riverDischargeCubicMetersPerSecond
              << ",\"sun_elevation_deg\":" << environment.sunElevationDegrees
              << ",\"aurora_activity\":" << environment.auroraActivity
              << ",\"camera\":{\"x\":" << camera.position.x
              << ",\"y\":" << camera.position.y
              << ",\"z\":" << camera.position.z
              << ",\"view_distance_m\":" << camera.viewDistanceMeters
              << "}";
}

}  // namespace

int main(int argc, char** argv) {
    std::locale::global(std::locale::classic());

    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage(std::cerr);
        return argc > 1 && std::string(argv[1]) == "--help" ? 0 : 2;
    }

    natureview::NatureSimulation simulation(options.seed);

    std::vector<double> checkpoints;
    for (double time = 0.0; time < options.seconds; time += 15.0) {
        checkpoints.push_back(time);
    }
    if (checkpoints.empty() || checkpoints.back() < options.seconds) {
        checkpoints.push_back(options.seconds);
    }

    std::cout << "{\n"
              << "  \"seed\":" << options.seed << ",\n"
              << "  \"world_fingerprint\":\"" << std::hex
              << simulation.world().fingerprint << std::dec << "\",\n"
              << "  \"world_size_m\":" << simulation.world().sizeMeters << ",\n"
              << "  \"terrain_resolution\":" << simulation.world().resolution << ",\n"
              << "  \"simulation_hz\":" << 1.0 / natureview::NatureSimulation::kFixedStepSeconds
              << ",\n"
              << "  \"camera_speed_mps\":"
              << natureview::NatureSimulation::kCameraSpeedMetersPerSecond << ",\n"
              << "  \"samples\":[\n";

    std::size_t nextCheckpoint = 0;
    bool firstSample = true;
    const auto emitCurrentSample = [&]() {
        if (!firstSample) {
            std::cout << ",\n";
        }
        std::cout << "    {";
        printSnapshot(simulation);
        std::cout << "}";
        firstSample = false;
    };

    while (nextCheckpoint < checkpoints.size() &&
           checkpoints[nextCheckpoint] <= simulation.elapsedSeconds() + 1.0e-9) {
        emitCurrentSample();
        ++nextCheckpoint;
    }

    const int totalFrames =
        static_cast<int>(std::ceil(options.seconds * options.framesPerSecond));
    for (int frame = 0; frame < totalFrames; ++frame) {
        simulation.advance(1.0 / static_cast<double>(options.framesPerSecond));
        while (nextCheckpoint < checkpoints.size() &&
               checkpoints[nextCheckpoint] <= simulation.elapsedSeconds() + 1.0e-9) {
            emitCurrentSample();
            ++nextCheckpoint;
        }
    }

    while (nextCheckpoint < checkpoints.size()) {
        emitCurrentSample();
        ++nextCheckpoint;
    }

    std::cout << "\n  ],\n"
              << "  \"final_state_fingerprint\":\"" << std::hex
              << simulation.stateFingerprint() << std::dec << "\"\n"
              << "}\n";
    return 0;
}
