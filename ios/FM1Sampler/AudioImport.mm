#import "AudioImport.h"
#import <AudioToolbox/AudioToolbox.h>
#include <cstring>
#include <string>
#include <vector>
#include "Wav.h"

static const int kMaxSeconds = 90;

NSString* FMImportAudio(NSURL* url, NSString* dir) {
    // the file name without extension, reduced to plain characters
    NSString* base = url.lastPathComponent.stringByDeletingPathExtension ?: @"";
    NSMutableString* clean = [NSMutableString string];
    for (NSUInteger i = 0; i < base.length; i++) {
        const unichar c = [base characterAtIndex:i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-') [clean appendFormat:@"%C", c];
    }
    NSString* name = [clean stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    if (name.length == 0) name = @"import";
    NSString* outPath = [dir stringByAppendingPathComponent:[NSString stringWithFormat:@"sample_%@.wav", name]];

    const BOOL scoped = [url startAccessingSecurityScopedResource];   // needed when the picker hands over the original and not a copy
    ExtAudioFileRef file = NULL;
    std::vector<float> pcm;
    UInt32 channels = 0;
    double rate = 0;
    if (ExtAudioFileOpenURL((__bridge CFURLRef)url, &file) == noErr && file) {
        AudioStreamBasicDescription src;
        memset(&src, 0, sizeof src);
        UInt32 size = sizeof src;
        if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileDataFormat, &size, &src) == noErr && src.mChannelsPerFrame >= 1) {
            channels = src.mChannelsPerFrame >= 2 ? 2 : 1;
            rate = src.mSampleRate >= 4000 ? src.mSampleRate : 44100;
            AudioStreamBasicDescription dst;   // what we read: interleaved floats at the file's own rate (the engine resamples on load)
            memset(&dst, 0, sizeof dst);
            dst.mSampleRate = rate;
            dst.mFormatID = kAudioFormatLinearPCM;
            dst.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
            dst.mChannelsPerFrame = channels;
            dst.mBitsPerChannel = 32;
            dst.mBytesPerFrame = 4 * channels;
            dst.mBytesPerPacket = 4 * channels;
            dst.mFramesPerPacket = 1;
            if (ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof dst, &dst) == noErr) {
                const UInt32 chunk = 16384;
                const size_t limit = (size_t)(kMaxSeconds * rate) * channels;
                std::vector<float> buf((size_t)chunk * channels);
                while (pcm.size() < limit) {
                    AudioBufferList bl;
                    bl.mNumberBuffers = 1;
                    bl.mBuffers[0].mNumberChannels = channels;
                    bl.mBuffers[0].mDataByteSize = (UInt32)(buf.size() * sizeof(float));
                    bl.mBuffers[0].mData = buf.data();
                    UInt32 frames = chunk;
                    if (ExtAudioFileRead(file, &frames, &bl) != noErr || frames == 0) break;
                    pcm.insert(pcm.end(), buf.begin(), buf.begin() + (size_t)frames * channels);
                }
                if (pcm.size() > limit) pcm.resize(limit);
            }
        }
        ExtAudioFileDispose(file);
    }
    if (scoped) [url stopAccessingSecurityScopedResource];
    if (pcm.empty() || channels == 0) return nil;

    std::vector<float> stereo;
    if (channels == 2) stereo.swap(pcm);
    else { stereo.resize(pcm.size() * 2); for (size_t i = 0; i < pcm.size(); i++) { stereo[2 * i] = pcm[i]; stereo[2 * i + 1] = pcm[i]; } }
    if (!writeWav16(std::string(outPath.fileSystemRepresentation), stereo, (int)(rate + 0.5))) return nil;
    return outPath;
}
