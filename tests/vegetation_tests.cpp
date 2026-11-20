#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "nature/render/vegetation_scene.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    const auto world = natureview::generateHeroWorld(139167);
    const natureview::VegetationScene first =
        natureview::buildVegetationScene(world);
    const natureview::VegetationScene second =
        natureview::buildVegetationScene(world);
    const natureview::VegetationScene different =
        natureview::buildVegetationScene(
            natureview::generateHeroWorld(139168));

    expect(first.fingerprint == second.fingerprint,
           "same seed must reproduce vegetation");
    expect(first.fingerprint != different.fingerprint,
           "different seed must change vegetation");
    expect(first.instances.size() > 2000U,
           "hero valley must contain a forest population");
    for (const natureview::RenderMesh& prototype : first.prototypes) {
        expect(prototype.indices.size() % 3U == 0U,
               "tree prototypes must be triangulated");
        for (const std::uint32_t index : prototype.indices) {
            expect(index < prototype.vertices.size(),
                   "tree prototype indices must stay in bounds");
        }
    }

    std::size_t conifers = 0;
    std::size_t broadleaf = 0;
    for (const natureview::TreeInstance& tree : first.instances) {
        const float riverClearance =
            std::abs(tree.position.x - world.riverXAt(tree.position.z));
        expect(riverClearance >= world.riverHalfWidthAt(tree.position.z) + 32.0F,
               "trees must clear the river channel");
        expect(tree.position.y > 8.0F && tree.position.y <= 1020.0F,
               "trees must remain below the tree line");
        expect(tree.heightMeters > 5.0F && tree.heightMeters < 15.0F,
               "tree height must stay in range");
        expect(tree.canopyRadiusMeters > 1.0F && tree.canopyRadiusMeters < 3.0F,
               "tree canopy radius must stay in range");
        expect(tree.prototypeIndex < first.prototypes.size(),
               "tree prototype index must stay in bounds");
        if (tree.prototypeIndex == 1) {
            ++broadleaf;
        } else {
            ++conifers;
        }
    }
    expect(conifers > 500U, "forest must contain conifers");
    expect(broadleaf > 100U, "low forest must contain broadleaf trees");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Vegetation tests passed: " << first.instances.size()
              << " trees\n";
    return EXIT_SUCCESS;
}
