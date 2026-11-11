#import <Foundation/Foundation.h>

/// A picked audio file (wav, aiff, mp3, m4a / aac, flac, caf...: whatever the system decodes) -> a 16-bit WAV `sample_<name>.wav` in `dir`,
/// at most 90 s, mono or stereo. Returns the path of the written file, nil if the file cannot be read. The counterpart of AudioImport.java.
NSString* FMImportAudio(NSURL* url, NSString* dir);
