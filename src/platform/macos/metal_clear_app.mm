#include "metal_clear_app.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace natureview::platform {
namespace {

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

}  // namespace

int runMetalClearApp(const MetalClearOptions& options) {
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
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = YES;
        metalLayer.maximumDrawableCount = 2;

        NSView* contentView = [window contentView];
        contentView.wantsLayer = YES;
        contentView.layer = metalLayer;

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        dispatch_semaphore_t inFlight = dispatch_semaphore_create(2);
        id<MTLCommandBuffer> lastCommandBuffer = nil;
        std::uint32_t renderedFrames = 0;
        bool failed = false;

        while (options.frameLimit == 0 || renderedFrames < options.frameLimit) {
            @autoreleasepool {
                drainEvents();
                if (![window isVisible]) {
                    break;
                }

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

                MTLRenderPassDescriptor* pass =
                    [MTLRenderPassDescriptor renderPassDescriptor];
                MTLRenderPassColorAttachmentDescriptor* colorAttachment =
                    pass.colorAttachments[0];
                colorAttachment.texture = drawable.texture;
                colorAttachment.loadAction = MTLLoadActionClear;
                colorAttachment.storeAction = MTLStoreActionStore;
                colorAttachment.clearColor =
                    MTLClearColorMake(0.012, 0.035, 0.052, 1.0);

                id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
                id<MTLRenderCommandEncoder> encoder =
                    [commandBuffer renderCommandEncoderWithDescriptor:pass];
                [encoder endEncoding];
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

        std::cout << "metal_device=" << [[device name] UTF8String] << '\n'
                  << "rendered_frames=" << renderedFrames << '\n';
        [window orderOut:nil];

        if (failed || renderedFrames == 0) {
            return 1;
        }
    }

    return 0;
}

}  // namespace natureview::platform
