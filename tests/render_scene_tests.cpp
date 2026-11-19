#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "nature/render/render_scene.hpp"
#include "nature/core/simulation.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

const natureview::TerrainChunk& findChunk(
    const natureview::RenderScene& scene,
    std::uint32_t column,
    std::uint32_t row) {
    return scene.terrainChunks[
        static_cast<std::size_t>(row) * scene.chunkColumns + column];
}

bool sameVertex(
    const natureview::RenderVertex& lhs,
    const natureview::RenderVertex& rhs) {
    return lhs.position.x == rhs.position.x &&
           lhs.position.y == rhs.position.y &&
           lhs.position.z == rhs.position.z &&
           lhs.normal.x == rhs.normal.x &&
           lhs.normal.y == rhs.normal.y &&
           lhs.normal.z == rhs.normal.z &&
           lhs.uv.x == rhs.uv.x &&
           lhs.uv.z == rhs.uv.z;
}

bool validIndices(const natureview::RenderMesh& mesh) {
    if (mesh.indices.size() % 3 != 0) {
        return false;
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    const auto world = natureview::generateHeroWorld(139167);
    const natureview::RenderSceneSettings settings{500.0F, 17, 25.0F};
    const natureview::RenderScene first = natureview::buildRenderScene(world, settings);
    const natureview::RenderScene second = natureview::buildRenderScene(world, settings);

    expect(first.chunkColumns == 12 && first.chunkRows == 12,
           "hero world must produce a 12 by 12 terrain grid");
    expect(first.terrainChunks.size() == 144, "all terrain chunks must be present");
    expect(first.fingerprint() == second.fingerprint(),
           "same world and settings must produce the same scene");

    for (const natureview::TerrainChunk& chunk : first.terrainChunks) {
        expect(chunk.mesh.vertices.size() == 17U * 17U,
               "terrain chunk vertex count must match settings");
        expect(chunk.mesh.indices.size() == 16U * 16U * 6U,
               "terrain chunk index count must form a full grid");
        expect(validIndices(chunk.mesh), "terrain indices must stay in bounds");
        expect(chunk.mesh.bounds.minimum.x < chunk.mesh.bounds.maximum.x &&
                   chunk.mesh.bounds.minimum.z < chunk.mesh.bounds.maximum.z,
               "terrain chunk bounds must be valid");
    }

    constexpr std::uint32_t side = 17;
    for (std::uint32_t row = 0; row < first.chunkRows; ++row) {
        for (std::uint32_t column = 0; column + 1 < first.chunkColumns; ++column) {
            const auto& left = findChunk(first, column, row);
            const auto& right = findChunk(first, column + 1, row);
            for (std::uint32_t vertexRow = 0; vertexRow < side; ++vertexRow) {
                const std::uint32_t leftIndex = vertexRow * side + (side - 1);
                const std::uint32_t rightIndex = vertexRow * side;
                expect(sameVertex(left.mesh.vertices[leftIndex],
                                  right.mesh.vertices[rightIndex]),
                       "adjacent terrain chunks must share exact edge vertices");
            }
        }
    }
    for (std::uint32_t row = 0; row + 1 < first.chunkRows; ++row) {
        for (std::uint32_t column = 0; column < first.chunkColumns; ++column) {
            const auto& south = findChunk(first, column, row);
            const auto& north = findChunk(first, column, row + 1);
            for (std::uint32_t vertexColumn = 0; vertexColumn < side; ++vertexColumn) {
                const std::uint32_t southIndex = (side - 1) * side + vertexColumn;
                const std::uint32_t northIndex = vertexColumn;
                expect(sameVertex(south.mesh.vertices[southIndex],
                                  north.mesh.vertices[northIndex]),
                       "adjacent terrain chunks must share exact north and south edges");
            }
        }
    }

    expect(first.river.vertices.size() == 482, "river mesh must cover the world");
    expect(first.river.indices.size() == 480U * 3U,
           "river mesh must be triangulated continuously");
    expect(validIndices(first.river), "river indices must stay in bounds");

    natureview::NatureSimulation simulation(139167);
    const auto visible = natureview::selectVisibleTerrainChunks(
        first, simulation.camera());
    expect(!visible.empty(), "camera selection must include terrain ahead");
    expect(visible.size() < first.terrainChunks.size(),
           "camera selection must reject some off-camera chunks");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All render scene tests passed\n";
    return EXIT_SUCCESS;
}
