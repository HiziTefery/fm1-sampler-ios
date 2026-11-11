#import "MidiIO.h"
#import <CoreMIDI/CoreMIDI.h>
#include <algorithm>
#include <atomic>
#include <initializer_list>

// The byte-stream API (MIDIPacketList) is deprecated in favour of MIDI 2.0 event lists, but it is the one that hands over SysEx as plain bytes.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static NSString* endpointName(MIDIEndpointRef e) {
    NSMutableString* all = [NSMutableString string];
    for (CFStringRef key : {kMIDIPropertyDisplayName, kMIDIPropertyName, kMIDIPropertyModel, kMIDIPropertyManufacturer}) {
        CFStringRef s = NULL;
        if (MIDIObjectGetStringProperty(e, key, &s) == noErr && s) { [all appendFormat:@"%@ ", (__bridge NSString*)s]; CFRelease(s); }
    }
    return all.lowercaseString;
}
static BOOL isOffline(MIDIEndpointRef e) {
    SInt32 off = 0;
    return MIDIObjectGetIntegerProperty(e, kMIDIPropertyOffline, &off) == noErr && off != 0;
}
// The same rule as the Android app: the FM-1 calls itself "FM-1" / "M-VAVE ...".
static BOOL isFm1(MIDIEndpointRef e) {
    if (!e || isOffline(e)) return NO;
    NSString* n = endpointName(e);
    return [n containsString:@"fm"] || [n containsString:@"m-vave"] || [n containsString:@"mvave"];
}

@implementation FMMidiIO {
    FMMidiReceiver _receiver;
    MIDIClientRef _client;
    MIDIPortRef _inPort, _outPort;
    MIDIEndpointRef _source;                 // main thread
    std::atomic<MIDIEndpointRef> _dest;      // read by send: on the tick queue
}

- (instancetype)initWithReceiver:(FMMidiReceiver)receiver {
    if ((self = [super init])) { _receiver = [receiver copy]; _dest = 0; }
    return self;
}

- (void)dealloc {
    if (_client) MIDIClientDispose(_client);   // disposes its ports too
}

- (void)start {
    if (_client) return;
    __weak FMMidiIO* weak = self;
    OSStatus r = MIDIClientCreateWithBlock(CFSTR("FM1 Sampler"), &_client, ^(const MIDINotification* n) {
        // devices came or went (delivered on the run loop that created the client: the main one)
        if (n->messageID == kMIDIMsgSetupChanged || n->messageID == kMIDIMsgObjectAdded || n->messageID == kMIDIMsgObjectRemoved || n->messageID == kMIDIMsgPropertyChanged)
            dispatch_async(dispatch_get_main_queue(), ^{ [weak rescan]; });
    });
    if (r != noErr) { NSLog(@"fm1sampler: MIDI client: %d", (int)r); _client = 0; return; }
    FMMidiReceiver receiver = _receiver;
    r = MIDIInputPortCreateWithBlock(_client, CFSTR("FM-1 in"), &_inPort, ^(const MIDIPacketList* list, void* srcConnRefCon) {
        const MIDIPacket* p = &list->packet[0];
        for (UInt32 i = 0; i < list->numPackets; i++) {
            if (p->length) receiver(p->data, p->length);
            p = MIDIPacketNext(p);
        }
    });
    if (r != noErr) NSLog(@"fm1sampler: MIDI in port: %d", (int)r);
    r = MIDIOutputPortCreate(_client, CFSTR("FM-1 out"), &_outPort);
    if (r != noErr) NSLog(@"fm1sampler: MIDI out port: %d", (int)r);
    [self rescan];
}

- (void)rescan {
    if (!_client) return;
    MIDIEndpointRef src = 0, dst = 0;
    for (ItemCount i = 0, n = MIDIGetNumberOfSources(); i < n; i++) { MIDIEndpointRef e = MIDIGetSource(i); if (isFm1(e)) src = e; }
    for (ItemCount i = 0, n = MIDIGetNumberOfDestinations(); i < n; i++) { MIDIEndpointRef e = MIDIGetDestination(i); if (isFm1(e)) dst = e; }
    if (src != _source) {
        if (_source) MIDIPortDisconnectSource(_inPort, _source);
        _source = 0;
        if (src && MIDIPortConnectSource(_inPort, src, NULL) == noErr) _source = src;
    }
    if (dst != _dest.load()) {
        _dest = dst;
        NSLog(@"fm1sampler: FM-1 MIDI %s", dst ? "connected" : "gone");
    }
}

- (void)send:(const uint8_t*)bytes length:(size_t)length {
    const MIDIEndpointRef dst = _dest.load();
    if (!dst || !_outPort || !length) return;
    Byte buffer[sizeof(MIDIPacketList) + 512];
    for (size_t off = 0; off < length;) {   // the surface's frames are at most 300 bytes; anything longer goes out in pieces
        const size_t n = std::min<size_t>(length - off, 256);
        MIDIPacketList* list = (MIDIPacketList*)buffer;
        MIDIPacket* p = MIDIPacketListInit(list);
        p = MIDIPacketListAdd(list, sizeof buffer, p, 0, (ByteCount)n, bytes + off);
        if (!p || MIDISend(_outPort, dst, list) != noErr) return;
        off += n;
    }
}

- (BOOL)connected { return _dest.load() != 0; }
@end
