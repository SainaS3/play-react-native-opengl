#pragma once

#import <React/RCTComponent.h>
#import <UIKit/UIKit.h>
#import <MetalANGLE/MGLKit.h>

// UIKit host for the shared renderer. React properties are registered in the manager.
@interface AngleView : UIView <MGLKViewDelegate>
@property(nonatomic, copy) NSString *model;
@property(nonatomic, copy) NSString *meshColor;
@property(nonatomic) CGFloat zoom;
@property(nonatomic) CGFloat rotationX;
@property(nonatomic) CGFloat rotationY;
@property(nonatomic) BOOL spinning;
@property(nonatomic) BOOL flying;
@property(nonatomic) BOOL wireframe;
@property(nonatomic) NSInteger resetToken;
@property(nonatomic, copy) RCTDirectEventBlock onError;
@property(nonatomic, copy) RCTDirectEventBlock onZoom;
@end
