#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "nature/core/simulation.hpp"
#include "nature/core/world.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(float lhs, float rhs, float tolerance) {
    return std::abs(lhs - rhs) <= tolerance;
}

void testWorldReproducibility() {
    const auto first = natureview::generateHeroWorld(139167);
    const auto second = natureview::generateHeroWorld(139167);
    const auto different = natureview::generateHeroWorld(139168);

    expect(first.fingerprint == second.fingerprint, "same seed must reproduce world");
    expect(first.fingerprint != different.fingerprint, "different seed must change world");
    expect(first.heights.size() == 257U * 257U, "terrain resolution must match");
    expect(first.river.size() == 513U, "river sampling must match");
    expect(first.riverHeightAt(0.0F) > 0.0F, "river must have a valid height");
    expect(std::isfinite(first.sampleHeight(120.0F, -300.0F)),
           "terrain sampling must be finite");
}

void testSimulationReproducibility() {
    natureview::NatureSimulation first(139167);
    natureview::NatureSimulation second(139167);

    for (int step = 0; step < 1200; ++step) {
        first.advance(natureview::NatureSimulation::kFixedStepSeconds);
        second.advance(natureview::NatureSimulation::kFixedStepSeconds);
    }

    expect(first.tickCount() == 1200, "60 seconds must produce 1200 fixed ticks");
    expect(near(static_cast<float>(first.elapsedSeconds()), 60.0F, 1.0e-4F),
           "fixed ticks must advance elapsed time");
    expect(first.stateFingerprint() == second.stateFingerprint(),
           "same seed and cadence must reproduce final state");
}

void testFrameCadenceIndependence() {
    natureview::NatureSimulation renderCadence(42);
    natureview::NatureSimulation fixedCadence(42);

    for (int frame = 0; frame < 3600; ++frame) {
        renderCadence.advance(1.0 / 60.0);
    }
    for (int tick = 0; tick < 1200; ++tick) {
        fixedCadence.advance(natureview::NatureSimulation::kFixedStepSeconds);
    }

    expect(renderCadence.tickCount() == fixedCadence.tickCount(),
           "render cadence must not change fixed simulation ticks");
    expect(renderCadence.stateFingerprint() == fixedCadence.stateFingerprint(),
           "render cadence must not change deterministic state");
}

void testEnvironmentRangesAndContinuity() {
    natureview::NatureSimulation simulation(7331);
    natureview::CameraState previousCamera = simulation.camera();

    float maximumCameraStep = 0.0F;
    for (int tick = 0; tick < 12000; ++tick) {
        simulation.advance(natureview::NatureSimulation::kFixedStepSeconds);
        const auto& environment = simulation.environment();
        const auto& camera = simulation.camera();

        expect(environment.cloudCover >= 0.0F && environment.cloudCover <= 1.0F,
               "cloud cover must stay normalized");
        expect(environment.rainIntensity >= 0.0F && environment.rainIntensity <= 1.0F,
               "rain intensity must stay normalized");
        expect(environment.humidity >= 0.0F && environment.humidity <= 1.0F,
               "humidity must stay normalized");
        expect(environment.fogDensityPerMeter > 0.0F, "fog density must stay positive");
        expect(environment.riverDischargeCubicMetersPerSecond > 0.0F,
               "river discharge must stay positive");
        expect(std::isfinite(environment.sunAzimuthRadians), "sun azimuth must be finite");
        expect(environment.auroraActivity >= 0.0F && environment.auroraActivity <= 1.0F,
               "aurora activity must stay normalized");

        maximumCameraStep =
            std::max(maximumCameraStep, natureview::length(camera.position - previousCamera.position));
        previousCamera = camera;
    }

    expect(maximumCameraStep < 0.06F, "camera must not jump between fixed ticks");
}

}  // namespace

int main() {
    testWorldReproducibility();
    testSimulationReproducibility();
    testFrameCadenceIndependence();
    testEnvironmentRangesAndContinuity();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All NatureCore tests passed\n";
    return EXIT_SUCCESS;
}
