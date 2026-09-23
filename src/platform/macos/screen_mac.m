#include "../../screen/capture.h"
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>
#include <stdio.h>
#include <stdlib.h>

@interface NanoCaptureEngine : NSObject <SCStreamOutput> {
@public
    SCStream *_stream;
    dispatch_queue_t _queue;
    dispatch_source_t _mock_timer;
    nano_capture_callback_t _callback;
    void *_user_data;
    nano_frame_t _frame_buffer;
    uint32_t _mock_frame_count;
    BOOL _is_running;
    BOOL _using_mock;
    NSLock *_lock;
}
@end

@implementation NanoCaptureEngine

- (instancetype)init {
    self = [super init];
    if (self) {
        _queue = dispatch_queue_create("com.nanoctrl.capture", DISPATCH_QUEUE_SERIAL);
        _lock = [[NSLock alloc] init];
        _is_running = NO;
        _using_mock = NO;
        _mock_frame_count = 0;
        memset(&_frame_buffer, 0, sizeof(_frame_buffer));
    }
    return self;
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    (void)stream;
    if (type != SCStreamOutputTypeScreen) return;

    [_lock lock];
    if (!_is_running || !_callback) {
        [_lock unlock];
        return;
    }
    nano_capture_callback_t cb = _callback;
    void *ud = _user_data;
    [_lock unlock];

    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixelBuffer) return;

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);

    if (baseAddress && width > 0 && height > 0) {
        nano_frame_t frame;
        frame.width = (uint16_t)width;
        frame.height = (uint16_t)height;
        frame.stride = (uint32_t)bytesPerRow;
        frame.size = (uint32_t)(bytesPerRow * height);
        frame.data = (uint8_t *)baseAddress;

        cb(&frame, ud);
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}

- (void)startMockCapture {
    [_lock lock];
    _using_mock = YES;
    fprintf(stderr, "[NANOCTRL] Note: Using synthetic screen generator (resolution 1280x720).\n");
    fprintf(stderr, "[NANOCTRL] Tip: Grant Screen Recording permissions in macOS System Settings for live display.\n");

    nano_frame_free(&_frame_buffer);
    nano_frame_alloc(&_frame_buffer, 1280, 720);

    _mock_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    /* 30 fps = ~33.3 ms */
    dispatch_source_set_timer(_mock_timer, DISPATCH_TIME_NOW, 33 * NSEC_PER_MSEC, 5 * NSEC_PER_MSEC);

    __weak typeof(self) weakSelf = self;
    dispatch_source_set_event_handler(_mock_timer, ^{
        typeof(self) strongSelf = weakSelf;
        if (!strongSelf) return;

        [strongSelf->_lock lock];
        if (!strongSelf->_is_running || !strongSelf->_callback || !strongSelf->_frame_buffer.data) {
            [strongSelf->_lock unlock];
            return;
        }

        nano_frame_generate_test_pattern(&strongSelf->_frame_buffer, strongSelf->_mock_frame_count++);
        nano_capture_callback_t cb = strongSelf->_callback;
        void *ud = strongSelf->_user_data;
        nano_frame_t f = strongSelf->_frame_buffer;
        [strongSelf->_lock unlock];

        cb(&f, ud);
    });

    dispatch_resume(_mock_timer);
    [_lock unlock];
}

- (void)stop {
    [_lock lock];
    _is_running = NO;

    if (_mock_timer) {
        dispatch_source_cancel(_mock_timer);
        _mock_timer = nil;
    }

    SCStream *s = _stream;
    _stream = nil;
    [_lock unlock];

    if (s) {
        [s stopCaptureWithCompletionHandler:nil];
    }

    /* Drain serial queue to guarantee no callback block is in-flight */
    if (_queue) {
        dispatch_sync(_queue, ^{});
    }
}

- (void)cleanup {
    [self stop];

    [_lock lock];
    _callback = NULL;
    _user_data = NULL;
    nano_frame_free(&_frame_buffer);
    [_lock unlock];
}

@end

struct nano_capture_s {
    NanoCaptureEngine *engine;
};

bool nano_capture_has_permission(void) {
    __block bool permitted = false;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
        if (!error && content && content.displays.count > 0) {
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
    cap->engine = [[NanoCaptureEngine alloc] init];
    return cap;
}

bool nano_capture_start(nano_capture_t *cap, nano_capture_callback_t cb, void *user_data) {
    if (!cap || !cap->engine) return false;

    NanoCaptureEngine *engine = cap->engine;
    [engine->_lock lock];
    engine->_callback = cb;
    engine->_user_data = user_data;
    engine->_is_running = YES;
    [engine->_lock unlock];

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block bool sck_started = false;

    __weak NanoCaptureEngine *weakEngine = engine;
    [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
        NanoCaptureEngine *strongEngine = weakEngine;
        if (!strongEngine) {
            dispatch_semaphore_signal(sem);
            return;
        }

        [strongEngine->_lock lock];
        if (!strongEngine->_is_running || error || !content || content.displays.count == 0) {
            [strongEngine->_lock unlock];
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
        strongEngine->_stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:nil];
        [strongEngine->_stream addStreamOutput:strongEngine type:SCStreamOutputTypeScreen sampleHandlerQueue:strongEngine->_queue error:&streamErr];

        if (!streamErr) {
            __weak NanoCaptureEngine *innerWeak = strongEngine;
            [strongEngine->_stream startCaptureWithCompletionHandler:^(NSError *startErr) {
                NanoCaptureEngine *innerStrong = innerWeak;
                if (innerStrong && !startErr) {
                    sck_started = true;
                    printf("[NANOCTRL] ScreenCaptureKit active: %lux%lu @ 30fps\n", (unsigned long)display.width, (unsigned long)display.height);
                }
                dispatch_semaphore_signal(sem);
            }];
        } else {
            strongEngine->_stream = nil;
            dispatch_semaphore_signal(sem);
        }
        [strongEngine->_lock unlock];
    }];

    /* Wait up to 500ms for ScreenCaptureKit */
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC));

    [engine->_lock lock];
    if (!sck_started && engine->_is_running) {
        [engine->_lock unlock];
        [engine startMockCapture];
    } else {
        [engine->_lock unlock];
    }

    return true;
}

void nano_capture_stop(nano_capture_t *cap) {
    if (!cap || !cap->engine) return;
    [cap->engine stop];
}

void nano_capture_destroy(nano_capture_t *cap) {
    if (!cap) return;
    if (cap->engine) {
        [cap->engine cleanup];
        cap->engine = nil;
    }
    free(cap);
}

