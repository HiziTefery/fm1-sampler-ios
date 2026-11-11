// The FM-1 on USB MIDI through CoreMIDI: finds it by name, forwards its bytes, sends the SysEx frames of the surface.
#import <Foundation/Foundation.h>

typedef void (^FMMidiReceiver)(const uint8_t* bytes, size_t length);   // called on CoreMIDI's thread

@interface FMMidiIO : NSObject
- (instancetype)initWithReceiver:(FMMidiReceiver)receiver;
/// Main thread. Connects now if the FM-1 is there and follows plugging / unplugging from then on.
- (void)start;
/// One complete message (a SysEx frame). Any thread.
- (void)send:(const uint8_t*)bytes length:(size_t)length;
/// The FM-1 is there. Any thread.
- (BOOL)connected;
@end
