// Replays the draw ops built by the C++ SamplerUI (engine/ui/SamplerUI.cpp) and forwards touches to it. All layout lives in C++.
#import <UIKit/UIKit.h>

@class FMBackend;

@interface FMSamplerView : UIView
- (instancetype)initWithBackend:(FMBackend*)backend;
@end
