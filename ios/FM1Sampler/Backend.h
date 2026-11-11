// Owns the engine (engine/host/Host.h) and its link to the outside: audio (AudioIO), the FM-1 on USB MIDI (MidiIO), the 60 Hz surface tick.
// The counterpart of android/.../Backend.java.
#import <Foundation/Foundation.h>

class Host;

@interface FMBackend : NSObject
+ (instancetype)shared;
@property(nonatomic, readonly) Host* host;
/// Called on the main queue when the phone UI asks for a file from the disk.
@property(nonatomic, copy) void (^onFileRequest)(void);
/// Milliseconds since the app started: the one clock of the tick, the UI frames and the touches.
- (uint32_t)nowMs;
- (void)start;
/// Call once the microphone permission is known.
- (void)setMicAllowed:(BOOL)allowed;
/// Converts a picked audio file into the library and loads it on the selected track (off the main thread).
- (void)importFileAtURL:(NSURL*)url;
@end
