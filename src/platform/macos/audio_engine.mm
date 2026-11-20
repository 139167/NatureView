#include "audio_engine.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

#import <AVFoundation/AVFoundation.h>

namespace natureview::platform {
namespace {

struct AtomicSoundscapeParameters {
    std::atomic<float> windGain{0.0F};
    std::atomic<float> windTurbulence{0.0F};
    std::atomic<float> foliageGain{0.0F};
    std::atomic<float> waterGain{0.0F};
    std::atomic<float> waterFlow{0.0F};
    std::atomic<float> waterPan{0.0F};
    std::atomic<float> rainGain{0.0F};

    void store(const SoundscapeParameters& parameters) {
        windGain.store(parameters.windGain, std::memory_order_relaxed);
        windTurbulence.store(parameters.windTurbulence, std::memory_order_relaxed);
        foliageGain.store(parameters.foliageGain, std::memory_order_relaxed);
        waterGain.store(parameters.waterGain, std::memory_order_relaxed);
        waterFlow.store(parameters.waterFlow, std::memory_order_relaxed);
        waterPan.store(parameters.waterPan, std::memory_order_relaxed);
        rainGain.store(parameters.rainGain, std::memory_order_relaxed);
    }

    [[nodiscard]] SoundscapeParameters load() const {
        return {
            windGain.load(std::memory_order_relaxed),
            windTurbulence.load(std::memory_order_relaxed),
            foliageGain.load(std::memory_order_relaxed),
            waterGain.load(std::memory_order_relaxed),
            waterFlow.load(std::memory_order_relaxed),
            waterPan.load(std::memory_order_relaxed),
            rainGain.load(std::memory_order_relaxed),
        };
    }
};

}  // namespace

struct MacAudioEngine::Impl {
    AVAudioEngine* engine = nil;
    AVAudioSourceNode* sourceNode = nil;
    AVAudioFormat* format = nil;
    std::shared_ptr<AtomicSoundscapeParameters> parameters =
        std::make_shared<AtomicSoundscapeParameters>();
    float sampleRate = 48000.0F;
    bool running = false;
};

MacAudioEngine::MacAudioEngine() : impl_(std::make_unique<Impl>()) {}

MacAudioEngine::~MacAudioEngine() {
    stop();
}

bool MacAudioEngine::start(
    const SoundscapeParameters& initialParameters,
    std::uint64_t seed) {
    if (impl_->running) {
        return true;
    }

    @autoreleasepool {
        impl_->parameters->store(initialParameters);
        impl_->format = [[AVAudioFormat alloc]
            initWithCommonFormat:AVAudioPCMFormatFloat32
                      sampleRate:48000.0
                        channels:2
                     interleaved:NO];
        if (impl_->format == nil) {
            return false;
        }

        impl_->engine = [[AVAudioEngine alloc] init];
        auto sourceParameters = impl_->parameters;
        const std::uint32_t sourceSampleRate =
            static_cast<std::uint32_t>(impl_->format.sampleRate);
        auto synthesizer =
            std::make_shared<SoundscapeSynthesizer>(seed, sourceSampleRate);
        synthesizer->setParameters(initialParameters);

        impl_->sourceNode = [[AVAudioSourceNode alloc]
            initWithFormat:impl_->format
               renderBlock:^OSStatus(
                   BOOL* isSilence,
                   const AudioTimeStamp* timestamp,
                   AVAudioFrameCount frameCount,
                   AudioBufferList* outputData) {
            (void)timestamp;
            if (isSilence != nullptr) {
                *isSilence = NO;
            }

            synthesizer->setParameters(sourceParameters->load());

            if (outputData->mNumberBuffers >= 2 &&
                outputData->mBuffers[0].mData != nullptr &&
                outputData->mBuffers[1].mData != nullptr) {
                auto* left = static_cast<float*>(outputData->mBuffers[0].mData);
                auto* right = static_cast<float*>(outputData->mBuffers[1].mData);
                for (AVAudioFrameCount frame = 0; frame < frameCount; ++frame) {
                    synthesizer->nextSample(left[frame], right[frame]);
                }
                return noErr;
            }

            if (outputData->mNumberBuffers >= 1 &&
                outputData->mBuffers[0].mData != nullptr &&
                outputData->mBuffers[0].mNumberChannels >= 2) {
                auto* interleaved =
                    static_cast<float*>(outputData->mBuffers[0].mData);
                for (AVAudioFrameCount frame = 0; frame < frameCount; ++frame) {
                    synthesizer->nextSample(
                        interleaved[frame * 2],
                        interleaved[frame * 2 + 1]);
                }
            }
            return noErr;
        }];

        if (impl_->sourceNode == nil) {
            return false;
        }

        [impl_->engine attachNode:impl_->sourceNode];
        [impl_->engine connect:impl_->sourceNode
                             to:impl_->engine.mainMixerNode
                         format:impl_->format];
        impl_->engine.mainMixerNode.outputVolume = 0.55F;
        [impl_->engine prepare];

        NSError* error = nil;
        if (![impl_->engine startAndReturnError:&error]) {
            [impl_->engine detachNode:impl_->sourceNode];
            impl_->sourceNode = nil;
            impl_->engine = nil;
            return false;
        }

        impl_->sampleRate = static_cast<float>(impl_->format.sampleRate);
        impl_->running = true;
        return true;
    }
}

void MacAudioEngine::update(const SoundscapeParameters& parameters) {
    impl_->parameters->store(parameters);
}

void MacAudioEngine::stop() {
    if (!impl_->running) {
        return;
    }

    @autoreleasepool {
        [impl_->engine stop];
        if (impl_->sourceNode != nil) {
            [impl_->engine detachNode:impl_->sourceNode];
        }
        impl_->sourceNode = nil;
        impl_->engine = nil;
        impl_->format = nil;
        impl_->running = false;
    }
}

bool MacAudioEngine::isRunning() const {
    return impl_->running;
}

float MacAudioEngine::sampleRate() const {
    return impl_->sampleRate;
}

}  // namespace natureview::platform
