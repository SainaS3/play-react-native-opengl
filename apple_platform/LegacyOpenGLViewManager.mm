#import "LegacyOpenGLView.hpp"
#import <React/RCTViewManager.h>

// Keep the React bridge separate from context, drawable and frame-loop ownership.
@interface LegacyOpenGLViewManager : RCTViewManager
@end
@implementation LegacyOpenGLViewManager
RCT_EXPORT_MODULE(LegacyOpenGLView)
+ (BOOL)requiresMainQueueSetup {
    return YES;
}
- (UIView *)view {
    return [LegacyOpenGLView new];
}
RCT_EXPORT_VIEW_PROPERTY(model, NSString)
RCT_EXPORT_VIEW_PROPERTY(meshColor, NSString)
RCT_EXPORT_VIEW_PROPERTY(rotationX, CGFloat)
RCT_EXPORT_VIEW_PROPERTY(rotationY, CGFloat)
RCT_EXPORT_VIEW_PROPERTY(spinning, BOOL)
RCT_EXPORT_VIEW_PROPERTY(flying, BOOL)
RCT_EXPORT_VIEW_PROPERTY(wireframe, BOOL)
RCT_EXPORT_VIEW_PROPERTY(resetToken, NSInteger)
RCT_EXPORT_VIEW_PROPERTY(onError, RCTDirectEventBlock)
@end
