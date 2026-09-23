#import "ui_mac.h"
#import "../../net/server.h"
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdio.h>

@interface RemoteViewportView : NSView
@property (nonatomic, assign) nano_session_t *session;
@property (nonatomic, assign) nano_frame_t *frameBuffer;
@end

@implementation RemoteViewportView {
    NSTrackingArea *_trackingArea;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                options:(NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect)
                                                  owner:self
                                               userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    if (!ctx) return;

    if (!self.frameBuffer || !self.frameBuffer->data || self.frameBuffer->width == 0 || self.frameBuffer->height == 0) {
        CGContextSetRGBFillColor(ctx, 0.1, 0.1, 0.12, 1.0);
        CGContextFillRect(ctx, NSRectToCGRect(self.bounds));
        return;
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, self.frameBuffer->data, self.frameBuffer->size, NULL);
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst; /* BGRA */

    CGImageRef img = CGImageCreate(self.frameBuffer->width,
                                   self.frameBuffer->height,
                                   8,
                                   32,
                                   self.frameBuffer->stride,
                                   colorSpace,
                                   bitmapInfo,
                                   provider,
                                   NULL,
                                   false,
                                   kCGRenderingIntentDefault);

    if (img) {
        CGContextDrawImage(ctx, NSRectToCGRect(self.bounds), img);
        CGImageRelease(img);
    }

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
}

- (void)getNormCoords:(NSEvent *)event outX:(uint16_t *)outX outY:(uint16_t *)outY {
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    NSRect b = self.bounds;
    if (b.size.width <= 0 || b.size.height <= 0) {
        *outX = 0; *outY = 0;
        return;
    }
    float nx = loc.x / b.size.width;
    float ny = loc.y / b.size.height;
    if (nx < 0.0f) nx = 0.0f; else if (nx > 1.0f) nx = 1.0f;
    if (ny < 0.0f) ny = 0.0f; else if (ny > 1.0f) ny = 1.0f;
    *outX = (uint16_t)(nx * 65535.0f);
    *outY = (uint16_t)(ny * 65535.0f);
}

- (void)mouseMoved:(NSEvent *)event {
    if (!self.session) return;
    uint16_t x, y;
    [self getNormCoords:event outX:&x outY:&y];
    nano_session_send_mouse_move(self.session, x, y);
}

- (void)mouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)mouseDown:(NSEvent *)event {
    if (!self.session) return;
    uint16_t x, y;
    [self getNormCoords:event outX:&x outY:&y];
    nano_session_send_mouse_button(self.session, NANO_MOUSE_LEFT, NANO_ACTION_DOWN, x, y);
}

- (void)mouseUp:(NSEvent *)event {
    if (!self.session) return;
    uint16_t x, y;
    [self getNormCoords:event outX:&x outY:&y];
    nano_session_send_mouse_button(self.session, NANO_MOUSE_LEFT, NANO_ACTION_UP, x, y);
}

- (void)rightMouseDown:(NSEvent *)event {
    if (!self.session) return;
    uint16_t x, y;
    [self getNormCoords:event outX:&x outY:&y];
    nano_session_send_mouse_button(self.session, NANO_MOUSE_RIGHT, NANO_ACTION_DOWN, x, y);
}

- (void)rightMouseUp:(NSEvent *)event {
    if (!self.session) return;
    uint16_t x, y;
    [self getNormCoords:event outX:&x outY:&y];
    nano_session_send_mouse_button(self.session, NANO_MOUSE_RIGHT, NANO_ACTION_UP, x, y);
}

- (void)scrollWheel:(NSEvent *)event {
    if (!self.session) return;
    nano_session_send_mouse_scroll(self.session, (int16_t)event.scrollingDeltaX, (int16_t)event.scrollingDeltaY);
}

- (void)keyDown:(NSEvent *)event {
    if (!self.session) return;
    uint8_t mods = 0;
    if (event.modifierFlags & NSEventModifierFlagShift) mods |= 1;
    if (event.modifierFlags & NSEventModifierFlagControl) mods |= 2;
    if (event.modifierFlags & NSEventModifierFlagOption) mods |= 4;
    if (event.modifierFlags & NSEventModifierFlagCommand) mods |= 8;
    nano_session_send_key(self.session, event.keyCode, NANO_ACTION_DOWN, mods);
}

- (void)keyUp:(NSEvent *)event {
    if (!self.session) return;
    uint8_t mods = 0;
    if (event.modifierFlags & NSEventModifierFlagShift) mods |= 1;
    if (event.modifierFlags & NSEventModifierFlagControl) mods |= 2;
    if (event.modifierFlags & NSEventModifierFlagOption) mods |= 4;
    if (event.modifierFlags & NSEventModifierFlagCommand) mods |= 8;
    nano_session_send_key(self.session, event.keyCode, NANO_ACTION_UP, mods);
}
@end

/* Global UI Coordinator */
@interface NanoUIApp : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow *mainWindow;
@property (nonatomic, strong) NSWindow *viewportWindow;
@property (nonatomic, strong) RemoteViewportView *viewportView;
@property (nonatomic, strong) NSTextField *statusLabel;
@property (nonatomic, strong) NSTextField *idLabel;
@property (nonatomic, strong) NSTextField *pinLabel;
@property (nonatomic, strong) NSTextField *targetInput;
@property (nonatomic, strong) NSTextField *pinInput;
@property (nonatomic, strong) NSTextField *serverInput;
@property (nonatomic, assign) nano_session_t *session;
@end

static NanoUIApp *g_ui_app = nil;

@implementation NanoUIApp

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    (void)aNotification;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return YES;
}

- (void)stopSession {
    if (self.session) {
        nano_session_stop(self.session);
        nano_session_destroy(self.session);
        self.session = NULL;
    }
    if (self.viewportWindow) {
        [self.viewportWindow close];
        self.viewportWindow = nil;
    }
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    (void)notification;
    [self stopSession];
}

@end

static void host_ui_state_change(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data) {
    (void)s; (void)user_data;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!g_ui_app || !g_ui_app.statusLabel) return;
        if (state == SESSION_STATE_LISTENING) {
            g_ui_app.statusLabel.stringValue = @"WAITING FOR CONTROLLER...";
            g_ui_app.statusLabel.textColor = [NSColor secondaryLabelColor];
        } else if (state == SESSION_STATE_CONNECTING) {
            g_ui_app.statusLabel.stringValue = [NSString stringWithFormat:@"Connecting to server... (%s)", msg ? msg : ""];
            g_ui_app.statusLabel.textColor = [NSColor systemOrangeColor];
        } else if (state == SESSION_STATE_ACTIVE) {
            g_ui_app.statusLabel.stringValue = [NSString stringWithFormat:@"● REMOTE SESSION ACTIVE (%s)", msg ? msg : ""];
            g_ui_app.statusLabel.textColor = [NSColor systemGreenColor];
        } else if (state == SESSION_STATE_DISCONNECTED) {
            g_ui_app.statusLabel.stringValue = [NSString stringWithFormat:@"Disconnected: %s", msg ? msg : ""];
            g_ui_app.statusLabel.textColor = [NSColor systemRedColor];
        }
    });
}

static bool host_ui_approval(nano_session_t *s, const char *remote_ip, void *user_data) {
    (void)s; (void)user_data;
    __block bool approved = false;
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"Remote Control Request";
        alert.informativeText = [NSString stringWithFormat:@"A controller (%s) is requesting permission to control your computer.\n\nDo you want to allow this?", remote_ip ? remote_ip : "remote peer"];
        [alert addButtonWithTitle:@"ACCEPT"];
        [alert addButtonWithTitle:@"REJECT"];
        alert.alertStyle = NSAlertStyleCritical;

        NSModalResponse res = [alert runModal];
        approved = (res == NSAlertFirstButtonReturn);
    });
    return approved;
}

static void create_host_window(nano_session_t *s, const char *server_str) {
    NSRect frame = NSMakeRect(0, 0, 360, 320);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:@"NANOCTRL Host"];
    [win center];

    NSView *content = win.contentView;

    /* App Title */
    NSTextField *title = [NSTextField labelWithString:@"NANOCTRL"];
    title.frame = NSMakeRect(20, 270, 320, 26);
    title.font = [NSFont systemFontOfSize:20 weight:NSFontWeightHeavy];
    title.alignment = NSTextAlignmentCenter;
    [content addSubview:title];

    /* Device ID Display */
    char formatted_id[32];
    nano_format_device_id(s->device_id, formatted_id, sizeof(formatted_id));

    NSTextField *idHeader = [NSTextField labelWithString:@"Your Device ID"];
    idHeader.frame = NSMakeRect(20, 235, 320, 16);
    idHeader.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    idHeader.textColor = [NSColor secondaryLabelColor];
    idHeader.alignment = NSTextAlignmentCenter;
    [content addSubview:idHeader];

    NSTextField *idView = [NSTextField labelWithString:[NSString stringWithUTF8String:formatted_id]];
    idView.frame = NSMakeRect(20, 205, 320, 28);
    idView.font = [NSFont monospacedSystemFontOfSize:24 weight:NSFontWeightBold];
    idView.alignment = NSTextAlignmentCenter;
    [content addSubview:idView];
    g_ui_app.idLabel = idView;

    /* PIN Display */
    NSTextField *pinHeader = [NSTextField labelWithString:@"One-Time PIN"];
    pinHeader.frame = NSMakeRect(20, 175, 320, 16);
    pinHeader.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    pinHeader.textColor = [NSColor secondaryLabelColor];
    pinHeader.alignment = NSTextAlignmentCenter;
    [content addSubview:pinHeader];

    char formatted_pin[16];
    snprintf(formatted_pin, sizeof(formatted_pin), "%.3s %.3s", s->pin, s->pin + 3);
    NSTextField *pinView = [NSTextField labelWithString:[NSString stringWithUTF8String:formatted_pin]];
    pinView.frame = NSMakeRect(20, 145, 320, 28);
    pinView.font = [NSFont monospacedSystemFontOfSize:24 weight:NSFontWeightBold];
    pinView.alignment = NSTextAlignmentCenter;
    [content addSubview:pinView];
    g_ui_app.pinLabel = pinView;

    /* Mode / Server Info */
    NSString *modeStr = server_str ? [NSString stringWithFormat:@"Relay Server: %s", server_str] : [NSString stringWithFormat:@"Direct LAN Mode: port %u", s->port];
    NSTextField *srvLbl = [NSTextField labelWithString:modeStr];
    srvLbl.frame = NSMakeRect(20, 115, 320, 16);
    srvLbl.font = [NSFont systemFontOfSize:10];
    srvLbl.textColor = [NSColor tertiaryLabelColor];
    srvLbl.alignment = NSTextAlignmentCenter;
    [content addSubview:srvLbl];

    /* Status */
    NSTextField *status = [NSTextField labelWithString:@"WAITING FOR CONTROLLER..."];
    status.frame = NSMakeRect(20, 75, 320, 22);
    status.font = [NSFont systemFontOfSize:12 weight:NSFontWeightMedium];
    status.alignment = NSTextAlignmentCenter;
    [content addSubview:status];
    g_ui_app.statusLabel = status;

    /* Stop Button */
    NSButton *stopBtn = [NSButton buttonWithTitle:@"[ STOP ]" target:g_ui_app action:@selector(stopSession)];
    stopBtn.frame = NSMakeRect(120, 25, 120, 32);
    stopBtn.bezelStyle = NSBezelStyleRounded;
    [content addSubview:stopBtn];

    g_ui_app.mainWindow = win;
    [win makeKeyAndOrderFront:nil];
}

void nano_ui_start_host_gui(uint16_t port, const char *pin) {
    nano_session_t *s = nano_session_create(NANO_ROLE_HOST);
    g_ui_app.session = s;
    s->on_state_change = host_ui_state_change;
    s->on_approval_request = host_ui_approval;

    nano_session_start_host(s, port, pin);
    create_host_window(s, NULL);
}

void nano_ui_start_host_relay_gui(const char *server_host, uint16_t server_port, uint32_t device_id, const char *pin) {
    nano_session_t *s = nano_session_create(NANO_ROLE_HOST);
    g_ui_app.session = s;
    s->on_state_change = host_ui_state_change;
    s->on_approval_request = host_ui_approval;

    char srv_display[160];
    snprintf(srv_display, sizeof(srv_display), "%s:%u", server_host ? server_host : "127.0.0.1", server_port > 0 ? server_port : NANO_DEFAULT_PORT);

    nano_session_start_host_relay(s, server_host, server_port, device_id, pin);
    create_host_window(s, srv_display);
}

static void controller_ui_state_change(nano_session_t *s, nano_session_state_t state, const char *msg, void *user_data) {
    (void)s; (void)user_data;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!g_ui_app) return;
        if (g_ui_app.statusLabel) {
            g_ui_app.statusLabel.stringValue = [NSString stringWithUTF8String:msg ? msg : ""];
        }
        if (state == SESSION_STATE_ACTIVE && !g_ui_app.viewportWindow) {
            /* Open viewport window */
            NSRect vframe = NSMakeRect(0, 0, 1024, 768);
            NSWindow *vwin = [[NSWindow alloc] initWithContentRect:vframe
                                                         styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                           backing:NSBackingStoreBuffered
                                                             defer:NO];
            [vwin setTitle:@"NANOCTRL Remote View"];
            [vwin center];

            RemoteViewportView *view = [[RemoteViewportView alloc] initWithFrame:vwin.contentView.bounds];
            view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            view.session = g_ui_app.session;
            view.frameBuffer = &g_ui_app.session->controller_frame;
            [vwin.contentView addSubview:view];

            g_ui_app.viewportWindow = vwin;
            g_ui_app.viewportView = view;
            [vwin makeKeyAndOrderFront:nil];
            [vwin makeFirstResponder:view];
        }
    });
}

static void controller_ui_frame_update(nano_session_t *s, const nano_frame_t *frame, void *user_data) {
    (void)s; (void)frame; (void)user_data;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (g_ui_app && g_ui_app.viewportView) {
            [g_ui_app.viewportView setNeedsDisplay:YES];
        }
    });
}

void nano_ui_start_controller_gui(const char *host, uint16_t port, const char *pin) {
    nano_session_t *s = nano_session_create(NANO_ROLE_CONTROLLER);
    g_ui_app.session = s;
    s->on_state_change = controller_ui_state_change;
    s->on_frame_update = controller_ui_frame_update;

    if (host && pin && strlen(pin) == 6) {
        nano_session_start_controller(s, host, port, pin);
        return;
    }

    NSRect frame = NSMakeRect(0, 0, 360, 300);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:@"NANOCTRL Controller"];
    [win center];

    NSView *content = win.contentView;

    NSTextField *title = [NSTextField labelWithString:@"NANOCTRL"];
    title.frame = NSMakeRect(20, 255, 320, 26);
    title.font = [NSFont systemFontOfSize:20 weight:NSFontWeightHeavy];
    title.alignment = NSTextAlignmentCenter;
    [content addSubview:title];

    /* Target ID or IP:Port */
    NSTextField *hostLbl = [NSTextField labelWithString:@"Device ID / IP:Port:"];
    hostLbl.frame = NSMakeRect(20, 215, 140, 18);
    [content addSubview:hostLbl];

    NSTextField *hostField = [[NSTextField alloc] initWithFrame:NSMakeRect(165, 212, 175, 22)];
    hostField.placeholderString = @"e.g. 842 190 345";
    if (host) hostField.stringValue = [NSString stringWithFormat:@"%s:%u", host, port];
    [content addSubview:hostField];
    g_ui_app.targetInput = hostField;

    /* PIN */
    NSTextField *pinLbl = [NSTextField labelWithString:@"6-Digit PIN:"];
    pinLbl.frame = NSMakeRect(20, 175, 140, 18);
    [content addSubview:pinLbl];

    NSTextField *pinField = [[NSTextField alloc] initWithFrame:NSMakeRect(165, 172, 175, 22)];
    pinField.placeholderString = @"e.g. 582914";
    if (pin) pinField.stringValue = [NSString stringWithUTF8String:pin];
    [content addSubview:pinField];
    g_ui_app.pinInput = pinField;

    /* Relay Server (Optional) */
    NSTextField *srvLbl = [NSTextField labelWithString:@"Relay Server:"];
    srvLbl.frame = NSMakeRect(20, 135, 140, 18);
    [content addSubview:srvLbl];

    NSTextField *srvField = [[NSTextField alloc] initWithFrame:NSMakeRect(165, 132, 175, 22)];
    srvField.placeholderString = @"127.0.0.1:7443";
    srvField.stringValue = @"127.0.0.1:7443";
    [content addSubview:srvField];
    g_ui_app.serverInput = srvField;

    NSTextField *status = [NSTextField labelWithString:@"Enter Target ID & PIN, then click Connect."];
    status.frame = NSMakeRect(20, 85, 320, 18);
    status.font = [NSFont systemFontOfSize:11];
    status.alignment = NSTextAlignmentCenter;
    status.textColor = [NSColor secondaryLabelColor];
    [content addSubview:status];
    g_ui_app.statusLabel = status;

    NSButton *connBtn = [NSButton buttonWithTitle:@"[ CONNECT ]" target:g_ui_app action:@selector(onConnectClicked)];
    connBtn.frame = NSMakeRect(110, 30, 140, 34);
    connBtn.bezelStyle = NSBezelStyleRounded;
    [content addSubview:connBtn];

    g_ui_app.mainWindow = win;
    [win makeKeyAndOrderFront:nil];
}

void nano_ui_start_controller_relay_gui(const char *server_host, uint16_t server_port, uint32_t target_id, const char *pin) {
    nano_session_t *s = nano_session_create(NANO_ROLE_CONTROLLER);
    g_ui_app.session = s;
    s->on_state_change = controller_ui_state_change;
    s->on_frame_update = controller_ui_frame_update;

    if (server_host && target_id > 0 && pin && strlen(pin) == 6) {
        nano_session_start_controller_relay(s, server_host, server_port, target_id, pin);
        return;
    }

    nano_ui_start_controller_gui(NULL, 0, pin);
}

@implementation NanoUIApp (Actions)

- (void)onConnectClicked {
    NSString *targetStr = self.targetInput.stringValue;
    NSString *pinStr = self.pinInput.stringValue;
    NSString *srvStr = self.serverInput.stringValue;

    if (pinStr.length != 6) {
        self.statusLabel.stringValue = @"Please enter a valid 6-digit PIN.";
        self.statusLabel.textColor = [NSColor systemRedColor];
        return;
    }

    if ([targetStr containsString:@":"]) {
        /* Direct IP:Port */
        char hostBuf[128] = "127.0.0.1";
        uint16_t port = NANO_DEFAULT_PORT;
        NSArray *parts = [targetStr componentsSeparatedByString:@":"];
        if (parts.count > 0 && [parts[0] length] > 0) {
            strncpy(hostBuf, [parts[0] UTF8String], sizeof(hostBuf) - 1);
        }
        if (parts.count > 1) {
            port = (uint16_t)[parts[1] intValue];
        }

        self.statusLabel.stringValue = @"Connecting directly...";
        self.statusLabel.textColor = [NSColor labelColor];
        nano_session_start_controller(self.session, hostBuf, port, [pinStr UTF8String]);
    } else {
        /* Self-Hosted Relay ID */
        uint32_t target_id = nano_parse_device_id([targetStr UTF8String]);
        if (target_id == 0) {
            self.statusLabel.stringValue = @"Please enter a valid 9-digit Device ID.";
            self.statusLabel.textColor = [NSColor systemRedColor];
            return;
        }

        char srvBuf[128] = "127.0.0.1";
        uint16_t srvPort = NANO_DEFAULT_PORT;
        NSArray *sparts = [srvStr componentsSeparatedByString:@":"];
        if (sparts.count > 0 && [sparts[0] length] > 0) {
            strncpy(srvBuf, [sparts[0] UTF8String], sizeof(srvBuf) - 1);
        }
        if (sparts.count > 1) {
            srvPort = (uint16_t)[sparts[1] intValue];
        }

        self.statusLabel.stringValue = @"Connecting via relay server...";
        self.statusLabel.textColor = [NSColor labelColor];
        nano_session_start_controller_relay(self.session, srvBuf, srvPort, target_id, [pinStr UTF8String]);
    }
}

- (void)launchHost {
    [self.mainWindow close];
    nano_ui_start_host_relay_gui("127.0.0.1", NANO_DEFAULT_PORT, 0, NULL);
}

- (void)launchController {
    [self.mainWindow close];
    nano_ui_start_controller_gui(NULL, 0, NULL);
}

@end

static void show_launcher_window(void) {
    NSRect frame = NSMakeRect(0, 0, 340, 240);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:@"NANOCTRL"];
    [win center];

    NSView *content = win.contentView;

    NSTextField *title = [NSTextField labelWithString:@"NANOCTRL"];
    title.frame = NSMakeRect(20, 180, 300, 28);
    title.font = [NSFont systemFontOfSize:22 weight:NSFontWeightHeavy];
    title.alignment = NSTextAlignmentCenter;
    [content addSubview:title];

    NSTextField *sub = [NSTextField labelWithString:@"Tiny Remote Control, Nothing Else."];
    sub.frame = NSMakeRect(20, 155, 300, 18);
    sub.font = [NSFont systemFontOfSize:11 weight:NSFontWeightMedium];
    sub.textColor = [NSColor secondaryLabelColor];
    sub.alignment = NSTextAlignmentCenter;
    [content addSubview:sub];

    NSButton *hostBtn = [NSButton buttonWithTitle:@"Share This Mac (Host)" target:g_ui_app action:@selector(launchHost)];
    hostBtn.frame = NSMakeRect(50, 95, 240, 38);
    hostBtn.bezelStyle = NSBezelStyleRounded;
    [content addSubview:hostBtn];

    NSButton *ctrlBtn = [NSButton buttonWithTitle:@"Control Remote Computer" target:g_ui_app action:@selector(launchController)];
    ctrlBtn.frame = NSMakeRect(50, 45, 240, 38);
    ctrlBtn.bezelStyle = NSBezelStyleRounded;
    [content addSubview:ctrlBtn];

    g_ui_app.mainWindow = win;
    [win makeKeyAndOrderFront:nil];
}

static void setup_app_menu(void) {
    [NSApplication sharedApplication];
    g_ui_app = [[NanoUIApp alloc] init];
    [NSApp setDelegate:g_ui_app];

    NSMenu *menuBar = [[NSMenu alloc] init];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menuBar addItem:appMenuItem];
    [NSApp setMainMenu:menuBar];

    NSMenu *appMenu = [[NSMenu alloc] init];
    NSString *appName = @"NANOCTRL";
    NSString *quitTitle = [@"Quit " stringByAppendingString:appName];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
}

int nano_ui_run_app(int argc, char *argv[]) {
    (void)argc; (void)argv;
    @autoreleasepool {
        setup_app_menu();
        show_launcher_window();
        [NSApp run];
    }
    return 0;
}

int nano_ui_run_host_app(uint16_t port, const char *pin) {
    @autoreleasepool {
        setup_app_menu();
        nano_ui_start_host_gui(port, pin);
        [NSApp run];
    }
    return 0;
}

int nano_ui_run_host_relay_app(const char *server_host, uint16_t server_port, uint32_t device_id, const char *pin) {
    @autoreleasepool {
        setup_app_menu();
        nano_ui_start_host_relay_gui(server_host, server_port, device_id, pin);
        [NSApp run];
    }
    return 0;
}

int nano_ui_run_controller_app(const char *host, uint16_t port, const char *pin) {
    @autoreleasepool {
        setup_app_menu();
        nano_ui_start_controller_gui(host, port, pin);
        [NSApp run];
    }
    return 0;
}

int nano_ui_run_controller_relay_app(const char *server_host, uint16_t server_port, uint32_t target_id, const char *pin) {
    @autoreleasepool {
        setup_app_menu();
        nano_ui_start_controller_relay_gui(server_host, server_port, target_id, pin);
        [NSApp run];
    }
    return 0;
}
