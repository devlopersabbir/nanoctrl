#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char * argv[]) {
    (void)argc; (void)argv;
    @autoreleasepool {
        int sizes[] = {16, 32, 64, 128, 256, 512, 1024};
        NSString *iconset = @"/tmp/nanoctrl_icon.iconset";
        [[NSFileManager defaultManager] removeItemAtPath:iconset error:nil];
        [[NSFileManager defaultManager] createDirectoryAtPath:iconset withIntermediateDirectories:YES attributes:nil error:nil];

        for (int i = 0; i < 7; i++) {
            int s = sizes[i];
            NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(s, s)];
            [img lockFocus];

            // Rounded dark slate background squircle
            NSBezierPath *bg = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0, 0, s, s) xRadius:s*0.22 yRadius:s*0.22];
            [[NSColor colorWithCalibratedRed:0.08 green:0.09 blue:0.12 alpha:1.0] set];
            [bg fill];

            // Outer cyan border ring
            NSBezierPath *ring = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(s*0.06, s*0.06, s*0.88, s*0.88) xRadius:s*0.18 yRadius:s*0.18];
            ring.lineWidth = s * 0.035;
            [[NSColor colorWithCalibratedRed:0.0 green:0.82 blue:0.95 alpha:0.9] set];
            [ring stroke];

            // Minimalist remote lightning / screen glyph in center
            NSBezierPath *path = [NSBezierPath bezierPath];
            [path moveToPoint:NSMakePoint(s*0.52, s*0.80)];
            [path lineToPoint:NSMakePoint(s*0.26, s*0.48)];
            [path lineToPoint:NSMakePoint(s*0.48, s*0.48)];
            [path lineToPoint:NSMakePoint(s*0.44, s*0.20)];
            [path lineToPoint:NSMakePoint(s*0.74, s*0.52)];
            [path lineToPoint:NSMakePoint(s*0.52, s*0.52)];
            [path closePath];
            [[NSColor colorWithCalibratedRed:0.0 green:0.95 blue:0.8 alpha:1.0] set];
            [path fill];

            [img unlockFocus];

            CGImageRef cgRef = [img CGImageForProposedRect:NULL context:nil hints:nil];
            NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cgRef];
            NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];

            NSString *p1 = [NSString stringWithFormat:@"%@/icon_%dx%d.png", iconset, s, s];
            [png writeToFile:p1 atomically:YES];
            if (s <= 512) {
                NSString *p2 = [NSString stringWithFormat:@"%@/icon_%dx%d@2x.png", iconset, s/2, s/2];
                [png writeToFile:p2 atomically:YES];
            }
        }

        NSString *cmd = @"iconutil -c icns /tmp/nanoctrl_icon.iconset -o resources/AppIcon.icns";
        int res = system([cmd UTF8String]);
        [[NSFileManager defaultManager] removeItemAtPath:iconset error:nil];

        if (res == 0) {
            printf("[OK] Successfully generated resources/AppIcon.icns\n");
        } else {
            fprintf(stderr, "Error: iconutil failed with code %d\n", res);
            return 1;
        }
    }
    return 0;
}
