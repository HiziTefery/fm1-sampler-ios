// Audio out (and the microphone in) through one RemoteIO unit at the engine's 44.1 kHz; the system converts to whatever the route runs at.
// With the FM-1 plugged in, iOS routes the output to its USB audio by itself.
#import <Foundation/Foundation.h>

class Host;

@interface FMAudioIO : NSObject
- (instancetype)initWithHost:(Host*)host;
- (void)start;
- (void)stop;
/// The microphone permission was granted (or not): with it the session records too.
- (void)setMicAllowed:(BOOL)allowed;
/// The sound goes to a USB audio device (the FM-1). Any thread.
- (BOOL)usbOutput;
@end
