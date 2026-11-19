#include "metal_app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "metal_shaders.hpp"
#include "nature/core/math.hpp"
#include "nature/core/simulation.hpp"
#include "nature/render/preview_renderer.hpp"
#include "nature/render/render_scene.hpp"

namespace natureview::platform {
namespace {

struct alignas(16) Matrix4 {
    float values[16]{};
};

struct alignas(16) FrameUniforms {
    Matrix4 viewProjection;
    float cameraPosition[4]{};
    float sunDirection[4]{};
    float settings[4]{};
    float cameraRight[4]{};
    float cameraUp[4]{};
    float cameraForward[4]{};
    float skySettings[4]{};
    Matrix4 lightViewProjection;
    float shadowSettings[4]{};
};

struct alignas(16) TonemapUniforms {
    float exposure = 0.0F;
    float padding[3]{};
};

struct DrawRange {
    NSUInteger indexOffset = 0;
    NSUInteger indexCount = 0;
};

struct CombinedMesh {
    std::vector<RenderVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<DrawRange> terrainRanges;
    DrawRange riverRange;
};

Matrix4 multiply(const Matrix4& lhs, const Matrix4& rhs) {
    Matrix4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (int index = 0; index < 4; ++index) {
                value += lhs.values[index * 4 + row] *
                         rhs.values[column * 4 + index];
            }
            result.values[column * 4 + row] = value;
        }
    }
    return result;
}

Matrix4 perspective(
    float verticalFovRadians,
    float aspectRatio,
    float nearPlane,
    float farPlane) {
    const float focalLength = 1.0F / std::tan(verticalFovRadians * 0.5F);
    Matrix4 result;
    result.values[0] = focalLength / aspectRatio;
    result.values[5] = focalLength;
    result.values[10] = farPlane / (nearPlane - farPlane);
    result.values[11] = -1.0F;
    result.values[14] = (nearPlane * farPlane) / (nearPlane - farPlane);
    return result;
}

Matrix4 orthographic(
    float left,
    float right,
    float bottom,
    float top,
    float nearPlane,
    float farPlane) {
    Matrix4 result;
    result.values[0] = 2.0F / (right - left);
    result.values[5] = 2.0F / (top - bottom);
    result.values[10] = 1.0F / (nearPlane - farPlane);
    result.values[12] = -(right + left) / (right - left);
    result.values[13] = -(top + bottom) / (top - bottom);
    result.values[14] = nearPlane / (nearPlane - farPlane);
    result.values[15] = 1.0F;
    return result;
}

Matrix4 view(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 forward = normalize(target - eye);
    const Vec3 right = normalize(cross(up, forward));
    const Vec3 cameraUp = cross(forward, right);

    Matrix4 result;
    result.values[0] = right.x;
    result.values[4] = right.y;
    result.values[8] = right.z;
    result.values[12] = -dot(right, eye);
    result.values[1] = cameraUp.x;
    result.values[5] = cameraUp.y;
    result.values[9] = cameraUp.z;
    result.values[13] = -dot(cameraUp, eye);
    result.values[2] = -forward.x;
    result.values[6] = -forward.y;
    result.values[10] = -forward.z;
    result.values[14] = dot(forward, eye);
    result.values[15] = 1.0F;
    return result;
}

FrameUniforms frameUniforms(
    const NatureSimulation& simulation,
    float aspectRatio,
    float material,
    float fogDensity) {
    const CameraState& camera = simulation.camera();
    const EnvironmentState& environment = simulation.environment();
    const float elevation = environment.sunElevationDegrees * kPi / 180.0F;
    const float horizontal = std::cos(elevation);
    const Vec3 sun = normalize({
        horizontal * std::cos(environment.sunAzimuthRadians),
        std::sin(elevation),
        horizontal * std::sin(environment.sunAzimuthRadians),
    });

    const Matrix4 projection = perspective(
        camera.verticalFovDegrees * kPi / 180.0F,
        aspectRatio,
        0.1F,
        8000.0F);
    const Matrix4 cameraView =
        view(camera.position, camera.position + camera.forward * 200.0F, {0.0F, 1.0F, 0.0F});

    FrameUniforms uniforms;
    uniforms.viewProjection = multiply(projection, cameraView);
    uniforms.cameraPosition[0] = camera.position.x;
    uniforms.cameraPosition[1] = camera.position.y;
    uniforms.cameraPosition[2] = camera.position.z;
    uniforms.cameraPosition[3] = 1.0F;
    uniforms.sunDirection[0] = sun.x;
    uniforms.sunDirection[1] = sun.y;
    uniforms.sunDirection[2] = sun.z;
    uniforms.sunDirection[3] = 0.0F;
    uniforms.settings[0] = material;
    uniforms.settings[1] = fogDensity;
    uniforms.settings[2] = static_cast<float>(simulation.elapsedSeconds());
    uniforms.settings[3] = 0.0F;

    const Vec3 worldUp = {0.0F, 1.0F, 0.0F};
    const Vec3 right = normalize(cross(worldUp, camera.forward));
    const Vec3 up = cross(camera.forward, right);
    uniforms.cameraRight[0] = right.x;
    uniforms.cameraRight[1] = right.y;
    uniforms.cameraRight[2] = right.z;
    uniforms.cameraUp[0] = up.x;
    uniforms.cameraUp[1] = up.y;
    uniforms.cameraUp[2] = up.z;
    uniforms.cameraForward[0] = camera.forward.x;
    uniforms.cameraForward[1] = camera.forward.y;
    uniforms.cameraForward[2] = camera.forward.z;
    uniforms.cameraForward[3] = 0.0F;
    uniforms.skySettings[0] =
        std::tan(camera.verticalFovDegrees * 0.5F * kPi / 180.0F);
    uniforms.skySettings[1] = aspectRatio;
    uniforms.skySettings[2] = environment.cloudCover;
    uniforms.skySettings[3] =
        smoothstep(-4.0F, 18.0F, environment.sunElevationDegrees);

    Vec3 lightFocus = camera.position + camera.forward * 700.0F;
    lightFocus.y += 250.0F;
    const Vec3 lightEye = lightFocus + sun * 3000.0F;
    const Vec3 lightUp =
        std::abs(dot(sun, {0.0F, 1.0F, 0.0F})) > 0.96F
            ? Vec3{0.0F, 0.0F, 1.0F}
            : Vec3{0.0F, 1.0F, 0.0F};
    const Matrix4 lightView = view(lightEye, lightFocus, lightUp);
    const Matrix4 lightProjection =
        orthographic(-1600.0F, 1600.0F, -1600.0F, 1600.0F, 1.0F, 7000.0F);
    uniforms.lightViewProjection = multiply(lightProjection, lightView);
    uniforms.shadowSettings[0] = 1.0F / 2048.0F;
    uniforms.shadowSettings[1] = 0.0015F;
    uniforms.shadowSettings[2] = 1.0F;
    uniforms.shadowSettings[3] = 1.0F;
    return uniforms;
}

CombinedMesh combineMesh(const RenderScene& scene) {
    CombinedMesh combined;
    std::size_t vertexCount = scene.river.vertices.size();
    std::size_t indexCount = scene.river.indices.size();
    for (const TerrainChunk& chunk : scene.terrainChunks) {
        vertexCount += chunk.mesh.vertices.size();
        indexCount += chunk.mesh.indices.size();
    }
    combined.vertices.reserve(vertexCount);
    combined.indices.reserve(indexCount);
    combined.terrainRanges.reserve(scene.terrainChunks.size());

    for (const TerrainChunk& chunk : scene.terrainChunks) {
        const auto baseVertex = static_cast<std::uint32_t>(combined.vertices.size());
        const NSUInteger firstIndex = combined.indices.size();
        combined.vertices.insert(
            combined.vertices.end(),
            chunk.mesh.vertices.begin(),
            chunk.mesh.vertices.end());
        for (const std::uint32_t index : chunk.mesh.indices) {
            combined.indices.push_back(baseVertex + index);
        }
        combined.terrainRanges.push_back({
            firstIndex,
            static_cast<NSUInteger>(chunk.mesh.indices.size()),
        });
    }

    const auto riverBase = static_cast<std::uint32_t>(combined.vertices.size());
    const NSUInteger riverFirstIndex = combined.indices.size();
    combined.vertices.insert(
        combined.vertices.end(),
        scene.river.vertices.begin(),
        scene.river.vertices.end());
    for (const std::uint32_t index : scene.river.indices) {
        combined.indices.push_back(riverBase + index);
    }
    combined.riverRange = {
        riverFirstIndex,
        static_cast<NSUInteger>(scene.river.indices.size()),
    };
    return combined;
}

id<MTLLibrary> compileShaderLibrary(id<MTLDevice> device) {
    NSError* error = nil;
    NSString* source = [NSString stringWithUTF8String:kMetalShaderSource];
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if (library == nil) {
        throw std::runtime_error(
            std::string("Metal shader compilation failed: ") +
            [[error localizedDescription] UTF8String]);
    }
    return library;
}

id<MTLRenderPipelineState> createPipeline(
    id<MTLDevice> device,
    id<MTLLibrary> library,
    NSString* label,
    NSString* vertexFunction,
    NSString* fragmentFunction,
    MTLPixelFormat colorFormat,
    MTLPixelFormat depthFormat,
    MTLVertexDescriptor* vertexDescriptor) {
    NSError* error = nil;
    MTLRenderPipelineDescriptor* descriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.label = label;
    descriptor.vertexFunction = [library newFunctionWithName:vertexFunction];
    if (fragmentFunction != nil) {
        descriptor.fragmentFunction =
            [library newFunctionWithName:fragmentFunction];
    }
    descriptor.vertexDescriptor = vertexDescriptor;
    descriptor.colorAttachments[0].pixelFormat = colorFormat;
    descriptor.depthAttachmentPixelFormat = depthFormat;
    id<MTLRenderPipelineState> pipeline =
        [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (pipeline == nil) {
        throw std::runtime_error(
            std::string("Metal pipeline creation failed for ") +
            [label UTF8String] + ": " +
            [[error localizedDescription] UTF8String]);
    }
    return pipeline;
}

MTLVertexDescriptor* createTerrainVertexDescriptor() {
    MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
    vertexDescriptor.attributes[0].offset = offsetof(RenderVertex, position);
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat3;
    vertexDescriptor.attributes[1].offset = offsetof(RenderVertex, normal);
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[2].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[2].offset = offsetof(RenderVertex, uv);
    vertexDescriptor.attributes[2].bufferIndex = 0;
    vertexDescriptor.layouts[0].stride = sizeof(RenderVertex);
    return vertexDescriptor;
}

void drainEvents() {
    while (true) {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event == nil) {
            return;
        }
        [NSApp sendEvent:event];
    }
}

bool writeCapturePpm(
    const std::filesystem::path& path,
    id<MTLBuffer> captureBuffer,
    NSUInteger width,
    NSUInteger height,
    NSUInteger bytesPerRow) {
    if (captureBuffer == nil || width == 0 || height == 0) {
        return false;
    }

    PreviewImage image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.pixels.resize(static_cast<std::size_t>(width) * height);
    const auto* bytes =
        static_cast<const std::uint8_t*>(captureBuffer.contents);
    for (NSUInteger row = 0; row < height; ++row) {
        const std::uint8_t* source = bytes + row * bytesPerRow;
        for (NSUInteger column = 0; column < width; ++column) {
            const std::uint8_t* pixel = source + column * 4;
            image.pixels[static_cast<std::size_t>(row) * width + column] = {
                pixel[2],
                pixel[1],
                pixel[0],
            };
        }
    }

    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    return writePpm(path, image);
}

id<MTLTexture> createDepthTexture(
    id<MTLDevice> device,
    NSUInteger width,
    NSUInteger height) {
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                     width:width
                                    height:height
                                 mipmapped:NO];
    descriptor.usage = MTLTextureUsageRenderTarget;
    descriptor.storageMode = MTLStorageModePrivate;
    return [device newTextureWithDescriptor:descriptor];
}

id<MTLTexture> createHdrTexture(
    id<MTLDevice> device,
    NSUInteger width,
    NSUInteger height) {
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                     width:width
                                    height:height
                                 mipmapped:NO];
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModePrivate;
    return [device newTextureWithDescriptor:descriptor];
}

id<MTLDepthStencilState> createDepthState(id<MTLDevice> device, bool writeDepth) {
    MTLDepthStencilDescriptor* descriptor = [[MTLDepthStencilDescriptor alloc] init];
    descriptor.depthCompareFunction = writeDepth ? MTLCompareFunctionLess
                                                 : MTLCompareFunctionAlways;
    descriptor.depthWriteEnabled = writeDepth;
    return [device newDepthStencilStateWithDescriptor:descriptor];
}

id<MTLTexture> createShadowTexture(id<MTLDevice> device) {
    constexpr NSUInteger shadowSize = 2048;
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                     width:shadowSize
                                    height:shadowSize
                                 mipmapped:NO];
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModePrivate;
    return [device newTextureWithDescriptor:descriptor];
}

}  // namespace

int runMetalApp(const MetalAppOptions& options) {
    if (options.width == 0 || options.height == 0) {
        std::cerr << "Metal window dimensions must be positive\n";
        return 2;
    }

    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [NSApp finishLaunching];

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            std::cerr << "No Metal device is available\n";
            return 1;
        }

        id<MTLCommandQueue> commandQueue = [device newCommandQueue];
        if (commandQueue == nil) {
            std::cerr << "Failed to create a Metal command queue\n";
            return 1;
        }

        const MTLPixelFormat colorFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        const MTLPixelFormat hdrFormat = MTLPixelFormatRGBA16Float;
        const MTLPixelFormat depthFormat = MTLPixelFormatDepth32Float;
        id<MTLLibrary> shaderLibrary = compileShaderLibrary(device);
        MTLVertexDescriptor* terrainVertexDescriptor =
            createTerrainVertexDescriptor();
        id<MTLRenderPipelineState> terrainPipeline = createPipeline(
            device,
            shaderLibrary,
            @"NatureView terrain",
            @"terrainVertex",
            @"terrainFragment",
            hdrFormat,
            depthFormat,
            terrainVertexDescriptor);
        id<MTLRenderPipelineState> shadowPipeline = createPipeline(
            device,
            shaderLibrary,
            @"NatureView shadow",
            @"shadowVertex",
            nil,
            MTLPixelFormatInvalid,
            depthFormat,
            terrainVertexDescriptor);
        id<MTLRenderPipelineState> skyPipeline = createPipeline(
            device,
            shaderLibrary,
            @"NatureView sky",
            @"skyVertex",
            @"skyFragment",
            hdrFormat,
            depthFormat,
            nil);
        id<MTLRenderPipelineState> tonemapPipeline = createPipeline(
            device,
            shaderLibrary,
            @"NatureView tonemap",
            @"tonemapVertex",
            @"tonemapFragment",
            colorFormat,
            MTLPixelFormatInvalid,
            nil);
        id<MTLDepthStencilState> skyDepthState =
            createDepthState(device, false);
        id<MTLDepthStencilState> terrainDepthState =
            createDepthState(device, true);
        id<MTLTexture> shadowTexture = createShadowTexture(device);
        if (shadowTexture == nil) {
            std::cerr << "Failed to create the shadow map\n";
            return 1;
        }

        NatureSimulation simulation(139167);
        constexpr double simulationFramesPerSecond = 60.0;
        const int prewarmFrames = static_cast<int>(
            std::ceil(options.prewarmSeconds * simulationFramesPerSecond));
        for (int frame = 0; frame < prewarmFrames; ++frame) {
            simulation.advance(1.0 / simulationFramesPerSecond);
        }
        const RenderScene scene = buildRenderScene(simulation.world());
        const CombinedMesh mesh = combineMesh(scene);
        id<MTLBuffer> vertexBuffer = [device
            newBufferWithBytes:mesh.vertices.data()
                       length:mesh.vertices.size() * sizeof(RenderVertex)
                      options:MTLResourceStorageModeShared];
        id<MTLBuffer> indexBuffer = [device
            newBufferWithBytes:mesh.indices.data()
                       length:mesh.indices.size() * sizeof(std::uint32_t)
                      options:MTLResourceStorageModeShared];
        if (vertexBuffer == nil || indexBuffer == nil) {
            std::cerr << "Failed to upload render mesh\n";
            return 1;
        }

        const NSWindowStyleMask styleMask =
            NSWindowStyleMaskTitled |
            NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable |
            NSWindowStyleMaskResizable;
        NSWindow* window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(
                                    0.0,
                                    0.0,
                                    static_cast<CGFloat>(options.width),
                                    static_cast<CGFloat>(options.height))
                      styleMask:styleMask
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [window setTitle:@"NatureView"];
        [window center];

        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        metalLayer.device = device;
        metalLayer.pixelFormat = colorFormat;
        metalLayer.framebufferOnly = options.capturePath.empty() ? YES : NO;
        metalLayer.maximumDrawableCount = 2;

        NSView* contentView = [window contentView];
        contentView.wantsLayer = YES;
        contentView.layer = metalLayer;

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        dispatch_semaphore_t inFlight = dispatch_semaphore_create(2);
        id<MTLTexture> depthTexture = nil;
        id<MTLTexture> hdrTexture = nil;
        id<MTLCommandBuffer> lastCommandBuffer = nil;
        id<MTLBuffer> captureBuffer = nil;
        NSUInteger capturedBytesPerRow = 0;
        NSUInteger capturedWidth = 0;
        NSUInteger capturedHeight = 0;
        std::uint32_t renderedFrames = 0;
        bool failed = false;
        auto previousFrameTime = std::chrono::steady_clock::now();

        while (options.frameLimit == 0 || renderedFrames < options.frameLimit) {
            @autoreleasepool {
                drainEvents();
                if (![window isVisible]) {
                    break;
                }

                const auto currentFrameTime = std::chrono::steady_clock::now();
                const double deltaSeconds = options.frameLimit == 0
                    ? std::min(
                          std::chrono::duration<double>(
                              currentFrameTime - previousFrameTime).count(),
                          0.25)
                    : (renderedFrames == 0 ? 0.0 : 1.0 / 60.0);
                previousFrameTime = currentFrameTime;
                simulation.advance(deltaSeconds);

                const NSRect backingBounds = [contentView convertRectToBacking:contentView.bounds];
                metalLayer.drawableSize = backingBounds.size;

                const dispatch_time_t waitTime =
                    dispatch_time(DISPATCH_TIME_NOW, 1'000'000'000LL);
                if (dispatch_semaphore_wait(inFlight, waitTime) != 0) {
                    std::cerr << "Timed out waiting for a Metal frame\n";
                    failed = true;
                    break;
                }

                id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
                if (drawable == nil) {
                    dispatch_semaphore_signal(inFlight);
                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                    continue;
                }

                const NSUInteger drawableWidth = drawable.texture.width;
                const NSUInteger drawableHeight = drawable.texture.height;
                if (depthTexture == nil ||
                    depthTexture.width != drawableWidth ||
                    depthTexture.height != drawableHeight) {
                    depthTexture =
                        createDepthTexture(device, drawableWidth, drawableHeight);
                }
                if (hdrTexture == nil ||
                    hdrTexture.width != drawableWidth ||
                    hdrTexture.height != drawableHeight) {
                    hdrTexture =
                        createHdrTexture(device, drawableWidth, drawableHeight);
                }

                const float aspectRatio =
                    static_cast<float>(drawableWidth) / static_cast<float>(drawableHeight);
                FrameUniforms terrainUniforms = frameUniforms(
                    simulation,
                    aspectRatio,
                    0.0F,
                    simulation.environment().fogDensityPerMeter);

                id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
                MTLRenderPassDescriptor* shadowPass =
                    [MTLRenderPassDescriptor renderPassDescriptor];
                shadowPass.depthAttachment.texture = shadowTexture;
                shadowPass.depthAttachment.loadAction = MTLLoadActionClear;
                shadowPass.depthAttachment.storeAction = MTLStoreActionStore;
                shadowPass.depthAttachment.clearDepth = 1.0;

                id<MTLRenderCommandEncoder> shadowEncoder =
                    [commandBuffer renderCommandEncoderWithDescriptor:shadowPass];
                const MTLViewport shadowViewport{
                    0.0,
                    0.0,
                    2048.0,
                    2048.0,
                    0.0,
                    1.0,
                };
                [shadowEncoder setViewport:shadowViewport];
                [shadowEncoder setRenderPipelineState:shadowPipeline];
                [shadowEncoder setDepthStencilState:terrainDepthState];
                [shadowEncoder setCullMode:MTLCullModeNone];
                [shadowEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
                [shadowEncoder setVertexBytes:&terrainUniforms
                                       length:sizeof(terrainUniforms)
                                      atIndex:1];
                for (std::size_t chunkIndex = 0;
                     chunkIndex < mesh.terrainRanges.size();
                     ++chunkIndex) {
                    const DrawRange& range = mesh.terrainRanges[chunkIndex];
                    [shadowEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                              indexCount:range.indexCount
                                               indexType:MTLIndexTypeUInt32
                                             indexBuffer:indexBuffer
                                       indexBufferOffset:range.indexOffset *
                                                         sizeof(std::uint32_t)];
                }
                [shadowEncoder endEncoding];

                MTLRenderPassDescriptor* pass =
                    [MTLRenderPassDescriptor renderPassDescriptor];
                MTLRenderPassColorAttachmentDescriptor* colorAttachment =
                    pass.colorAttachments[0];
                colorAttachment.texture = hdrTexture;
                colorAttachment.loadAction = MTLLoadActionDontCare;
                colorAttachment.storeAction = MTLStoreActionStore;
                pass.depthAttachment.texture = depthTexture;
                pass.depthAttachment.loadAction = MTLLoadActionClear;
                pass.depthAttachment.storeAction = MTLStoreActionDontCare;
                pass.depthAttachment.clearDepth = 1.0;

                id<MTLRenderCommandEncoder> encoder =
                    [commandBuffer renderCommandEncoderWithDescriptor:pass];
                const MTLViewport viewport{
                    0.0,
                    0.0,
                    static_cast<double>(drawableWidth),
                    static_cast<double>(drawableHeight),
                    0.0,
                    1.0,
                };
                [encoder setViewport:viewport];

                [encoder setRenderPipelineState:skyPipeline];
                [encoder setDepthStencilState:skyDepthState];
                [encoder setFragmentBytes:&terrainUniforms
                                   length:sizeof(terrainUniforms)
                                  atIndex:1];
                [encoder setFragmentTexture:shadowTexture atIndex:1];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:0
                            vertexCount:3];

                [encoder setRenderPipelineState:terrainPipeline];
                [encoder setDepthStencilState:terrainDepthState];
                [encoder setCullMode:MTLCullModeNone];
                [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
                [encoder setVertexBytes:&terrainUniforms
                                 length:sizeof(terrainUniforms)
                                atIndex:1];
                [encoder setFragmentBytes:&terrainUniforms
                                   length:sizeof(terrainUniforms)
                                  atIndex:1];

                const std::vector<std::uint32_t> visibleChunks =
                    selectVisibleTerrainChunks(
                        scene,
                        simulation.camera(),
                        aspectRatio);
                for (const std::uint32_t chunkIndex : visibleChunks) {
                    const DrawRange& range = mesh.terrainRanges[chunkIndex];
                    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                       indexCount:range.indexCount
                                        indexType:MTLIndexTypeUInt32
                                      indexBuffer:indexBuffer
                                indexBufferOffset:range.indexOffset * sizeof(std::uint32_t)];
                }

                FrameUniforms riverUniforms = frameUniforms(
                    simulation,
                    aspectRatio,
                    1.0F,
                    simulation.environment().fogDensityPerMeter);
                [encoder setVertexBytes:&riverUniforms
                                 length:sizeof(riverUniforms)
                                atIndex:1];
                [encoder setFragmentBytes:&riverUniforms
                                   length:sizeof(riverUniforms)
                                  atIndex:1];
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:mesh.riverRange.indexCount
                                     indexType:MTLIndexTypeUInt32
                                   indexBuffer:indexBuffer
                             indexBufferOffset:mesh.riverRange.indexOffset *
                                               sizeof(std::uint32_t)];
                [encoder endEncoding];

                MTLRenderPassDescriptor* compositePass =
                    [MTLRenderPassDescriptor renderPassDescriptor];
                compositePass.colorAttachments[0].texture = drawable.texture;
                compositePass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
                compositePass.colorAttachments[0].storeAction = MTLStoreActionStore;

                id<MTLRenderCommandEncoder> compositeEncoder =
                    [commandBuffer renderCommandEncoderWithDescriptor:compositePass];
                [compositeEncoder setViewport:viewport];
                [compositeEncoder setRenderPipelineState:tonemapPipeline];
                [compositeEncoder setFragmentTexture:hdrTexture atIndex:0];
                const float exposure =
                    -0.15F +
                    (1.0F - simulation.environment().cloudCover) * 0.08F +
                    simulation.environment().rainIntensity * 0.05F +
                    clamp(
                        (12.0F - simulation.environment().sunElevationDegrees) / 80.0F,
                        0.0F,
                        0.12F);
                TonemapUniforms tonemapUniforms;
                tonemapUniforms.exposure = exposure;
                [compositeEncoder setFragmentBytes:&tonemapUniforms
                                            length:sizeof(tonemapUniforms)
                                           atIndex:0];
                [compositeEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                                     vertexStart:0
                                     vertexCount:3];
                [compositeEncoder endEncoding];

                if (!options.capturePath.empty() &&
                    renderedFrames + 1 == options.frameLimit) {
                    capturedWidth = drawableWidth;
                    capturedHeight = drawableHeight;
                    capturedBytesPerRow = drawableWidth * 4;
                    capturedBytesPerRow =
                        (capturedBytesPerRow + 255U) & ~NSUInteger{255U};
                    captureBuffer = [device
                        newBufferWithLength:capturedBytesPerRow * drawableHeight
                                   options:MTLResourceStorageModeShared];
                    id<MTLBlitCommandEncoder> blit =
                        [commandBuffer blitCommandEncoder];
                    [blit copyFromTexture:drawable.texture
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(0, 0, 0)
                              sourceSize:MTLSizeMake(
                                             drawableWidth,
                                             drawableHeight,
                                             1)
                                toBuffer:captureBuffer
                       destinationOffset:0
                  destinationBytesPerRow:capturedBytesPerRow
                destinationBytesPerImage:capturedBytesPerRow * drawableHeight];
                    [blit endEncoding];
                }

                [commandBuffer presentDrawable:drawable];
                [commandBuffer addCompletedHandler:^(
                    __unused id<MTLCommandBuffer> completedBuffer) {
                    dispatch_semaphore_signal(inFlight);
                }];
                [commandBuffer commit];

                lastCommandBuffer = commandBuffer;
                ++renderedFrames;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        if (lastCommandBuffer != nil) {
            [lastCommandBuffer waitUntilCompleted];
            if (lastCommandBuffer.status == MTLCommandBufferStatusError) {
                std::cerr << "Metal command buffer failed\n";
                failed = true;
            }
        }

        bool captureSucceeded = true;
        if (!options.capturePath.empty()) {
            captureSucceeded = writeCapturePpm(
                options.capturePath,
                captureBuffer,
                capturedWidth,
                capturedHeight,
                capturedBytesPerRow);
            if (!captureSucceeded) {
                std::cerr << "Failed to capture Metal frame\n";
                failed = true;
            }
        }

        std::cout << "metal_device=" << [[device name] UTF8String] << '\n'
                  << "rendered_frames=" << renderedFrames << '\n'
                  << "simulation_seconds=" << simulation.elapsedSeconds() << '\n'
                  << "terrain_chunks=" << scene.terrainChunks.size() << '\n'
                  << "combined_vertices=" << mesh.vertices.size() << '\n'
                  << "combined_indices=" << mesh.indices.size() << '\n';
        if (captureSucceeded && !options.capturePath.empty()) {
            std::cout << "capture=" << options.capturePath << '\n';
        }
        [window orderOut:nil];

        if (failed || renderedFrames == 0 || !captureSucceeded) {
            return 1;
        }
    }

    return 0;
}

}  // namespace natureview::platform
