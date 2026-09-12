#include "../../screen/capture.h"
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>
#include <stdio.h>
#include <stdlib.h>

@interface NanoStreamDelegate : NSObject <SCStreamOutput>
@property (nonatomic, assign) nano_capture_t *captureContext;
@end

struct nano_capture_s {
    SCStream *stream;
    NanoStreamDelegate *delegate;
    dispatch_queue_t queue;
    dispatch_source_t mock_timer;
    nano_capture_callback_t callback;
    void *user_data;
    nano_frame_t frame_buffer;
    uint32_t mock_frame_count;
    bool is_running;
    bool using_mock;
};

@implementation NanoStreamDelegate
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    (void)stream;
    if (type != SCStreamOutputTypeScreen) return;
    nano_capture_t *cap = self.captureContext;
    if (!cap || !cap->is_running || !cap->callback) return;

    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixelBuffer) return;

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);

    nano_frame_t frame;
    frame.width = (uint16_t)width;
    frame.height = (uint16_t)height;
    frame.stride = (uint32_t)bytesPerRow;
    frame.size = (uint32_t)(bytesPerRow * height);
    frame.data = (uint8_t *)baseAddress;

    cap->callback(&frame, cap->user_data);

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}
@end

bool nano_capture_has_permission(void) {
    /* ScreenCaptureKit permissions can be checked via shareable content query */
    __block bool permitted = false;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
        if (!error && content.displays.count > 0) {
            permitted = true;
        }
        dispatch_semaphore_signal(sem);
    }];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC));
    return permitted;
}

nano_capture_t *nano_capture_create(void) {
    nano_capture_t *cap = (nano_capture_t *)calloc(1, sizeof(nano_capture_t));
    if (!cap) return NULL;
    cap->queue = dispatch_queue_create("com.nanoctrl.capture", DISPATCH_QUEUE_SERIAL);
    cap->delegate = [[NanoStreamDelegate alloc] init];
    cap->delegate.captureContext = cap;
    return cap;
}

static void start_mock_capture(nano_capture_t *cap) {
    cap->using_mock = true;
    fprintf(stderr, "[NANOCTRL] Note: Using synthetic screen generator (resolution 1280x720).\n");
    fprintf(stderr, "[NANOCTRL] Tip: Grant Screen Recording permissions in macOS System Settings for live display.\n");

    nano_frame_alloc(&cap->frame_buffer, 1280, 720);

    cap->mock_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, cap->queue);
    /* 30 fps = ~33.3 ms */
    dispatch_source_set_timer(cap->mock_timer, DISPATCH_TIME_NOW, 33 * NSEC_PER_MSEC, 5 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(cap->mock_timer, ^{
        if (!cap->is_running || !cap->callback) return;
        nano_frame_generate_test_pattern(&cap->frame_buffer, cap->mock_frame_count++);
        cap->callback(&cap->frame_buffer, cap->user_data);
    });
    dispatch_resume(cap->mock_timer);
}

bool nano_capture_start(nano_capture_t *cap, nano_capture_callback_t cb, void *user_data) {
    if (!cap) return false;
    cap->callback = cb;
    cap->user_data = user_data;
    cap->is_running = true;

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block bool sck_started = false;

    [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
        if (error || !content || content.displays.count == 0) {
            dispatch_semaphore_signal(sem);
            return;
        }

        SCDisplay *display = content.displays[0];
        SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
        SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
        config.width = display.width;
        config.height = display.height;
        config.pixelFormat = kCVPixelFormatType_32BGRA;
        config.minimumFrameInterval = CMTimeMake(1, 30);
        config.showsCursor = YES;

        NSError *streamErr = nil;
        cap->stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:nil];
        [cap->stream addStreamOutput:cap->delegate type:SCStreamOutputTypeScreen sampleHandlerQueue:cap->queue error:&streamErr];

        if (!streamErr) {
            [cap->stream startCaptureWithCompletionHandler:^(NSError *startErr) {
                if (!startErr) {
                    sck_started = true;
                    printf("[NANOCTRL] ScreenCaptureKit active: %lux%lu @ 30fps\n", (unsigned long)display.width, (unsigned long)display.height);
                }
                dispatch_semaphore_signal(sem);
            }];
        } else {
            dispatch_semaphore_signal(sem);
        }
    }];

    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 1000 * NSEC_PER_MSEC));

    if (!sck_started) {
        start_mock_capture(cap);
    }

    return true;
}

void nano_capture_stop(nano_capture_t *cap) {
    if (!cap || !cap->is_running) return;
    cap->is_running = false;

    if (cap->mock_timer) {
        dispatch_source_cancel(cap->mock_timer);
        cap->mock_timer = NULL;
    }
    if (cap->stream) {
        [cap->stream stopCaptureWithCompletionHandler:nil];
        cap->stream = nil;
    }
}

void nano_capture_destroy(nano_capture_t *cap) {
    if (!cap) return;
    nano_capture_stop(cap);
    nano_frame_free(&cap->frame_buffer);
    cap->delegate = nil;
    free(cap);
}
