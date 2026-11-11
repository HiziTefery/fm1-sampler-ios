#import "AudioIO.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <UIKit/UIKit.h>
#include <atomic>
#include <cstring>
#include "Host.h"

namespace {
constexpr UInt32 kMaxFrames = 4096;

struct RenderState {
    Host* host = nullptr;
    AudioUnit unit = nullptr;
    bool input = false;                 // set before the unit starts, constant while it runs
    float mono[kMaxFrames];
    float stereo[kMaxFrames * 2];
};

// The I/O thread: pulls the microphone (if it is on) and lets the engine fill the output.
OSStatus renderCallback(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts, UInt32 bus, UInt32 frames, AudioBufferList* io) {
    RenderState* s = static_cast<RenderState*>(ref);
    if (io->mNumberBuffers < 1 || !io->mBuffers[0].mData) return noErr;
    float* out = static_cast<float*>(io->mBuffers[0].mData);
    const UInt32 room = io->mBuffers[0].mDataByteSize / (2 * sizeof(float));
    if (frames > room) frames = room;
    const float* in = nullptr;
    if (s->input && frames <= kMaxFrames) {
        AudioBufferList bl;
        bl.mNumberBuffers = 1;
        bl.mBuffers[0].mNumberChannels = 1;
        bl.mBuffers[0].mDataByteSize = frames * sizeof(float);
        bl.mBuffers[0].mData = s->mono;
        AudioUnitRenderActionFlags f = 0;
        if (AudioUnitRender(s->unit, &f, ts, 1, frames, &bl) == noErr) {
            for (UInt32 i = 0; i < frames; i++) { s->stereo[2 * i] = s->mono[i]; s->stereo[2 * i + 1] = s->mono[i]; }
            in = s->stereo;
        }
    }
    s->host->render(out, in, (int)frames);
    return noErr;
}

AudioStreamBasicDescription floatFormat(UInt32 channels) {
    AudioStreamBasicDescription f;
    memset(&f, 0, sizeof f);
    f.mSampleRate = SAMPLERATE;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;   // interleaved
    f.mChannelsPerFrame = channels;
    f.mBitsPerChannel = 32;
    f.mBytesPerFrame = 4 * channels;
    f.mBytesPerPacket = 4 * channels;
    f.mFramesPerPacket = 1;
    return f;
}
}  // namespace

@implementation FMAudioIO {
    RenderState* _rs;
    BOOL _wanted, _micAllowed, _running;
    std::atomic<bool> _usb;
    NSUInteger _restartGen;
}

- (instancetype)initWithHost:(Host*)host {
    if ((self = [super init])) {
        _rs = new RenderState;
        _rs->host = host;
        _usb = false;
        NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
        [nc addObserver:self selector:@selector(routeChanged:) name:AVAudioSessionRouteChangeNotification object:nil];
        [nc addObserver:self selector:@selector(interrupted:) name:AVAudioSessionInterruptionNotification object:nil];
        [nc addObserver:self selector:@selector(servicesReset:) name:AVAudioSessionMediaServicesWereResetNotification object:nil];
        [nc addObserver:self selector:@selector(becameActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
    }
    return self;
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [self closeUnit];
    delete _rs;
}

// ---- public (main thread) ----
- (void)start { _wanted = YES; [self restart]; }
- (void)stop { _wanted = NO; [self closeUnit]; [AVAudioSession.sharedInstance setActive:NO error:nil]; }
- (void)setMicAllowed:(BOOL)allowed {
    if (allowed == _micAllowed) return;
    _micAllowed = allowed;
    if (_wanted) [self restart];
}
- (BOOL)usbOutput { return _usb.load(); }

// ---- session and unit ----
- (void)restart {
    _restartGen++;
    [self closeUnit];
    if (!_wanted) return;
    AVAudioSession* s = AVAudioSession.sharedInstance;
    NSError* e = nil;
    BOOL record = _micAllowed;
    if (record && ![s setCategory:AVAudioSessionCategoryPlayAndRecord mode:AVAudioSessionModeDefault
                          options:AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetoothA2DP error:&e]) {
        NSLog(@"fm1sampler: play-and-record refused: %@", e);
        record = NO;
    }
    if (!record && ![s setCategory:AVAudioSessionCategoryPlayback mode:AVAudioSessionModeDefault options:0 error:&e]) NSLog(@"fm1sampler: category: %@", e);
    [s setPreferredSampleRate:SAMPLERATE error:nil];                   // the FM-1 runs its USB audio at 44.1 kHz
    [s setPreferredIOBufferDuration:256.0 / SAMPLERATE error:nil];
    if (![s setActive:YES error:&e]) { NSLog(@"fm1sampler: session not active: %@", e); return; }   // e.g. a phone call: the interruption end restarts
    [self updateRoute];
    if (![self openUnitWithInput:record && s.isInputAvailable] && record) [self openUnitWithInput:NO];
    NSLog(@"fm1sampler: audio %s, rate %.0f, buffer %.1f ms, mic %d, usb %d", _running ? "started" : "FAILED", s.sampleRate, s.IOBufferDuration * 1000.0, (int)_rs->input, (int)_usb.load());
}

- (BOOL)openUnitWithInput:(BOOL)input {
    AudioComponentDescription d = {kAudioUnitType_Output, kAudioUnitSubType_RemoteIO, kAudioUnitManufacturer_Apple, 0, 0};
    AudioComponent comp = AudioComponentFindNext(NULL, &d);
    AudioUnit u = NULL;
    if (!comp || AudioComponentInstanceNew(comp, &u) != noErr || !u) return NO;
    UInt32 on = input ? 1 : 0, maxFrames = kMaxFrames;
    AudioStreamBasicDescription stereo = floatFormat(2), mono = floatFormat(1);
    AURenderCallbackStruct cb = {renderCallback, _rs};
    OSStatus r = AudioUnitSetProperty(u, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &on, sizeof on);
    if (r == noErr) r = AudioUnitSetProperty(u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &stereo, sizeof stereo);
    if (r == noErr && input) r = AudioUnitSetProperty(u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &mono, sizeof mono);
    if (r == noErr) r = AudioUnitSetProperty(u, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof maxFrames);
    if (r == noErr) r = AudioUnitSetProperty(u, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof cb);
    _rs->unit = u; _rs->input = input;
    if (r == noErr) r = AudioUnitInitialize(u);
    if (r == noErr) {
        r = AudioOutputUnitStart(u);
        if (r != noErr) AudioUnitUninitialize(u);
    }
    if (r != noErr) {
        NSLog(@"fm1sampler: audio unit (input %d) failed: %d", (int)input, (int)r);
        AudioComponentInstanceDispose(u);
        _rs->unit = NULL; _rs->input = false;
        return NO;
    }
    _running = YES;
    return YES;
}

- (void)closeUnit {
    AudioUnit u = _rs->unit;
    if (!u) return;
    AudioOutputUnitStop(u);          // returns when the I/O thread has left the callback
    AudioUnitUninitialize(u);
    AudioComponentInstanceDispose(u);
    _rs->unit = NULL; _rs->input = false;
    _running = NO;
}

- (void)updateRoute {
    bool usb = false;
    for (AVAudioSessionPortDescription* p in AVAudioSession.sharedInstance.currentRoute.outputs)
        if ([p.portType isEqualToString:AVAudioSessionPortUSBAudio]) usb = true;
    _usb = usb;
}

// Several notifications come for one plug event: restart once, a moment later.
- (void)restartSoon {
    const NSUInteger gen = ++_restartGen;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (gen == self->_restartGen && self->_wanted) [self restart];
    });
}

// ---- notifications (any thread) ----
- (void)routeChanged:(NSNotification*)n {
    const NSUInteger reason = [n.userInfo[AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue];
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateRoute];
        // the FM-1 (or headphones) plugged or unplugged: the unit is rebuilt for the new route
        if (reason == AVAudioSessionRouteChangeReasonNewDeviceAvailable || reason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable) [self restartSoon];
    });
}
- (void)interrupted:(NSNotification*)n {
    const NSUInteger type = [n.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (type == AVAudioSessionInterruptionTypeBegan) [self closeUnit];
        else [self restartSoon];
    });
}
- (void)servicesReset:(NSNotification*)n { dispatch_async(dispatch_get_main_queue(), ^{ [self restartSoon]; }); }
- (void)becameActive:(NSNotification*)n { dispatch_async(dispatch_get_main_queue(), ^{ if (self->_wanted && !self->_running) [self restartSoon]; }); }
@end
