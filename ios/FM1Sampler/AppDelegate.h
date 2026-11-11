#import <UIKit/UIKit.h>

@interface FMAppDelegate : UIResponder <UIApplicationDelegate>
@end

// One window with the sampler (Info.plist names this class in its scene manifest).
@interface FMSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow* window;
@end
