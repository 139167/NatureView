#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>

#include "nature/render/preview_renderer.hpp"
#include "nature/render/render_scene.hpp"

namespace {

struct Options {
    std::uint64_t seed = 139167;
    double seconds = 60.0;
    int width = 960;
    int height = 540;
    std::filesystem::path output = "nature-preview.ppm";
};

void printUsage(std::ostream& output) {
    output << "Usage: nature_preview_cli [--seed N] [--seconds N] "
              "[--width N] [--height N] [--output FILE.ppm]\n";
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
            } else if (argument == "--width") {
                options.width = std::stoi(argv[++index]);
            } else if (argument == "--height") {
                options.height = std::stoi(argv[++index]);
            } else if (argument == "--output") {
                options.output = argv[++index];
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
    if (options.width < 64 || options.width > 3840 ||
        options.height < 64 || options.height > 2160) {
        std::cerr << "Image dimensions must be between 64 and 3840 by 64 and 2160\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::locale::global(std::locale::classic());

    Options options;
    const bool validOptions = parseOptions(argc, argv, options);
    if (!validOptions) {
        if (argc > 1 && std::string(argv[1]) == "--help") {
            return 0;
        }
        printUsage(std::cerr);
        return 2;
    }

    natureview::NatureSimulation simulation(options.seed);
    constexpr int simulationFramesPerSecond = 60;
    const int totalFrames =
        static_cast<int>(std::ceil(options.seconds * simulationFramesPerSecond));
    for (int frame = 0; frame < totalFrames; ++frame) {
        simulation.advance(1.0 / simulationFramesPerSecond);
    }

    const natureview::RenderScene renderScene =
        natureview::buildRenderScene(simulation.world());
    const std::vector<std::uint32_t> visibleChunks =
        natureview::selectVisibleTerrainChunks(
            renderScene,
            simulation.camera(),
            static_cast<float>(options.width) / static_cast<float>(options.height));
    std::size_t terrainVertices = 0;
    std::size_t terrainTriangles = 0;
    for (const natureview::TerrainChunk& chunk : renderScene.terrainChunks) {
        terrainVertices += chunk.mesh.vertices.size();
        terrainTriangles += chunk.mesh.indices.size() / 3;
    }

    const auto renderStart = std::chrono::steady_clock::now();
    natureview::SoftwarePreviewRenderer renderer;
    const natureview::PreviewImage image = renderer.render(
        simulation, {options.width, options.height, 4500.0F});
    const auto renderEnd = std::chrono::steady_clock::now();

    if (!options.output.parent_path().empty()) {
        std::filesystem::create_directories(options.output.parent_path());
    }
    if (!natureview::writePpm(options.output, image)) {
        std::cerr << "Failed to write " << options.output << '\n';
        return EXIT_FAILURE;
    }

    const double elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(renderEnd - renderStart).count();
    std::cout << std::fixed << std::setprecision(3)
              << "output=" << options.output << '\n'
              << "seed=" << options.seed << '\n'
              << "simulation_seconds=" << simulation.elapsedSeconds() << '\n'
              << "image=" << image.width << 'x' << image.height << '\n'
              << "image_fingerprint=" << std::hex << image.fingerprint() << std::dec << '\n'
              << "terrain_chunks=" << renderScene.terrainChunks.size() << '\n'
              << "visible_terrain_chunks=" << visibleChunks.size() << '\n'
              << "terrain_vertices=" << terrainVertices << '\n'
              << "terrain_triangles=" << terrainTriangles << '\n'
              << "river_vertices=" << renderScene.river.vertices.size() << '\n'
              << "river_triangles=" << renderScene.river.indices.size() / 3 << '\n'
              << "scene_fingerprint=" << std::hex << renderScene.fingerprint()
              << std::dec << '\n'
              << "render_ms=" << elapsedMilliseconds << '\n';
    return EXIT_SUCCESS;
}
