#import "SamplerViewController.h"
#import "Backend.h"
#import "SamplerView.h"
#import <AVFoundation/AVFoundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface FMSamplerViewController () <UIDocumentPickerDelegate>
@end

@implementation FMSamplerViewController {
    FMBackend* _backend;
}

- (void)loadView {
    _backend = [FMBackend shared];
    self.view = [[FMSamplerView alloc] initWithBackend:_backend];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    __weak FMSamplerViewController* weak = self;
    _backend.onFileRequest = ^{ [weak pickFile]; };
    [_backend start];
    // The sound starts right away; the microphone joins once the permission is there (asked on the first launch).
    FMBackend* backend = _backend;
    [AVAudioSession.sharedInstance requestRecordPermission:^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{ [backend setMicAllowed:granted]; });
    }];
}

// full screen, landscape, no accidental swipes home while turning a knob at the edge
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }

// ---- LOAD FILE FROM DISK ----
- (void)pickFile {
    if (self.presentedViewController) return;
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeAudio] asCopy:YES];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    if (urls.firstObject) [_backend importFileAtURL:urls.firstObject];
}
@end
