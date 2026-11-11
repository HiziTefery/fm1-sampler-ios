#import "Backend.h"
#import "AudioIO.h"
#import "AudioImport.h"
#import "MidiIO.h"
#import <QuartzCore/QuartzCore.h>
#include <memory>
#include <vector>
#include "Host.h"

@implementation FMBackend {
    std::unique_ptr<Host> _host;
    FMAudioIO* _audio;
    FMMidiIO* _midi;
    dispatch_queue_t _tickQueue;
    dispatch_source_t _ticker;
    CFTimeInterval _t0;
    std::vector<uint8_t> _out;   // tick queue only
    BOOL _started;
}

+ (instancetype)shared {
    static FMBackend* b;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ b = [FMBackend new]; });
    return b;
}

- (instancetype)init {
    if ((self = [super init])) {
        _t0 = CACurrentMediaTime();
        // Recordings, imported files and the theme live in Documents (visible in the Files app); the bundled library is read from the app bundle.
        NSString* data = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        [NSFileManager.defaultManager createDirectoryAtPath:data withIntermediateDirectories:YES attributes:nil error:nil];
        NSString* samples = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"samples"];
        _host = std::make_unique<Host>(data.fileSystemRepresentation, samples.fileSystemRepresentation);
        _tickQueue = dispatch_queue_create("fm1.surface", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (Host*)host { return _host.get(); }
- (uint32_t)nowMs { return (uint32_t)((CACurrentMediaTime() - _t0) * 1000.0); }

- (void)start {
    if (_started) return;
    _started = YES;
    Host* host = _host.get();
    _audio = [[FMAudioIO alloc] initWithHost:host];
    [_audio start];
    _midi = [[FMMidiIO alloc] initWithReceiver:^(const uint8_t* bytes, size_t n) { host->midiIn(bytes, n); }];
    [_midi start];

    _ticker = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _tickQueue);
    dispatch_source_set_timer(_ticker, dispatch_time(DISPATCH_TIME_NOW, 0), 16 * NSEC_PER_MSEC, 2 * NSEC_PER_MSEC);
    __weak FMBackend* weak = self;
    dispatch_source_set_event_handler(_ticker, ^{ [weak tick]; });
    dispatch_resume(_ticker);
}

- (void)tick {
    Host* host = _host.get();
    host->tick([self nowMs], _out);
    FMMidiIO* midi = _midi;
    Host::forEachFrame(_out, [midi](const uint8_t* m, size_t n) { [midi send:m length:n]; });   // one SysEx frame per packet
    host->setLink([midi connected], [_audio usbOutput]);
    if (host->wantFile()) {
        dispatch_async(dispatch_get_main_queue(), ^{ if (self.onFileRequest) self.onFileRequest(); });
    }
}

- (void)setMicAllowed:(BOOL)allowed { [_audio setMicAllowed:allowed]; }

- (void)importFileAtURL:(NSURL*)url {
    Host* host = _host.get();
    host->say("READING THE FILE...");
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{   // decoding can take a moment
        NSString* dir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        NSString* wav = FMImportAudio(url, dir);
        if (wav) host->importFile(wav.fileSystemRepresentation);
        else host->say("CANNOT READ THIS FILE");
    });
}
@end
