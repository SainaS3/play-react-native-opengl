#import <React/RCTViewManager.h>
#import <MetalANGLE/MGLKit.h>
#import <GLES2/gl2.h>
#include "../shared/renderer/ViewerRenderer.hpp"
#include <stdexcept>
#include <cstring>

// React owns the controls. This native view owns the GL context and frame loop.
@class LegacyOpenGLView;
@interface GLFrameProxy : NSObject
@property(nonatomic, weak) LegacyOpenGLView *view;
- (void)tick:(CADisplayLink *)link;
@end

@interface LegacyOpenGLView : UIView <MGLKViewDelegate>
@property(nonatomic, copy) NSString *model;
@property(nonatomic, copy) NSString *meshColor;
@property(nonatomic) BOOL spinning;
@property(nonatomic) BOOL flying;
@property(nonatomic) BOOL wireframe;
@property(nonatomic) NSInteger resetToken;
@property(nonatomic, copy) RCTDirectEventBlock onError;
- (void)tick:(CADisplayLink *)link;
- (std::string)resource:(NSString *)name extension:(NSString *)extension;
@end

@implementation GLFrameProxy
- (void)tick:(CADisplayLink *)link {
    [self.view tick:link];
}
@end

@implementation LegacyOpenGLView {
    MGLKView *_glView;
    BOOL _reportedFrame;
    viewer::Renderer _renderer;
    CFTimeInterval _lastTime;
    float _frameElapsed;
    CADisplayLink *_displayLink;
    NSString *_failure;
}

- (instancetype)init {
    if ((self = [super initWithFrame:CGRectZero])) {
        MGLContext *context = nil;
        @try {
            context = [[MGLContext alloc] initWithAPI:kMGLRenderingAPIOpenGLES2];
        } @catch (NSException *exception) {
            [self fail:exception.reason];
            return self;
        }
        if (!context || ![MGLContext setCurrentContext:context]) {
            [self fail:@"Cannot create ANGLE GLES 2 context"];
            return self;
        }
        // MGLKView explicitly disallows subclassing: own it as a child UIView.
        _glView = [[MGLKView alloc] initWithFrame:self.bounds context:context];
        _glView.autoresizingMask =
            UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        _glView.userInteractionEnabled = NO;
        _glView.delegate = self;
        _glView.enableSetNeedsDisplay = NO;
        _glView.drawableDepthFormat = MGLDrawableDepthFormat24;
        _glView.drawableMultisample = MGLDrawableMultisample4X;
        [self addSubview:_glView];
        _spinning = YES;
        _meshColor = @"#e8b56b";
        // Drawable format setters can release/unbind MGLKit's EGL surface.
        // Rebind after configuring the view, before any GLES resource calls.
        if (![MGLContext setCurrentContext:context forLayer:nil]) {
            [self fail:@"Cannot bind ANGLE context after drawable setup"];
            return self;
        }
        const char *renderer = (const char *)glGetString(GL_RENDERER);
        NSLog(@"ANGLE backend: %s | %s | %s", glGetString(GL_VENDOR), renderer,
              glGetString(GL_VERSION));
        if (!renderer || !strstr(renderer, "Metal")) {
            [self fail:@"ANGLE did not select the Metal backend"];
            return self;
        }
        try {
            _renderer.createResources([self resource:@"sVertexLighting" extension:@"vsh"],
                                      [self resource:@"sVertexLighting" extension:@"fsh"]);
        } catch (const std::exception &error) {
            [self fail:[NSString stringWithUTF8String:error.what()]];
            return self;
        }
        self.model = @"cone.obj";
    }
    return self;
}

- (void)fail:(NSString *)message {
    _failure = message;
    [_displayLink invalidate];
    _displayLink = nil;
    NSLog(@"Native OpenGL: %@", message);
    if (_onError)
        _onError(@{@"message" : message});
}
- (void)setOnError:(RCTDirectEventBlock)onError {
    _onError = [onError copy];
    if (_failure && _onError)
        _onError(@{@"message" : _failure});
}

// Platform asset adapter. The shared renderer receives UTF-8 source only.
- (std::string)resource:(NSString *)name extension:(NSString *)extension {
    NSString *path = [[NSBundle mainBundle] pathForResource:name ofType:extension];
    NSString *source = path ? [NSString stringWithContentsOfFile:path
                                                        encoding:NSUTF8StringEncoding
                                                           error:nullptr]
                            : nil;
    if (!source)
        throw std::runtime_error("Missing bundled resource: " + std::string(name.UTF8String));
    return std::string(source.UTF8String);
}
- (void)setModel:(NSString *)model {
    if (!_glView || _failure || [_model isEqualToString:model])
        return;
    if (!model)
        model = @"cone.obj";
    if (![model.lastPathComponent isEqualToString:model] ||
        ![model.pathExtension isEqualToString:@"obj"]) {
        [self fail:@"Invalid model name"];
        return;
    }
    try {
        if (![MGLContext setCurrentContext:_glView.context])
            throw std::runtime_error("Cannot bind ANGLE context for mesh upload");
        _renderer.loadModel(model.UTF8String, [self resource:model.stringByDeletingPathExtension
                                                   extension:@"obj"]);
        _model = [model copy];
        _lastTime = 0;
        NSLog(@"Native OpenGL loaded %@ (%d vertices)", model, _renderer.triangleCount());
    } catch (const std::exception &error) {
        [self fail:[NSString stringWithUTF8String:error.what()]];
    }
}
- (void)setResetToken:(NSInteger)resetToken {
    _resetToken = resetToken;
    _renderer.resetAnimation();
    _lastTime = 0;
}
- (void)didMoveToWindow {
    [super didMoveToWindow];
    if (self.window && _glView && !_failure && !_displayLink) {
        GLFrameProxy *proxy = [GLFrameProxy new];
        proxy.view = self;
        _displayLink = [CADisplayLink displayLinkWithTarget:proxy selector:@selector(tick:)];
        [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
        _lastTime = 0;
    } else if (!self.window) {
        [_displayLink invalidate];
        _displayLink = nil;
    }
}
- (void)tick:(CADisplayLink *)link {
    if (UIApplication.sharedApplication.applicationState != UIApplicationStateActive) {
        _lastTime = 0;
        return;
    }
    // Shared renderer owns elapsed-time validation/clamping on both platforms.
    float dt = _lastTime ? (float)(link.timestamp - _lastTime) : 0;
    _lastTime = link.timestamp;
    _frameElapsed = dt;
    if (_failure || CGRectIsEmpty(_glView.bounds))
        return;
    @try {
        [_glView display];
    } @catch (NSException *exception) {
        [self fail:exception.reason];
        [_displayLink invalidate];
        _displayLink = nil;
    }
}
- (void)mglkView:(MGLKView *)view drawInRect:(CGRect)rect {
    if (_failure)
        return;
    // MGLKView has bound its drawable. Shared code never binds framebuffer zero
    // and never presents; the view owns those operations, including MSAA.
    try {
        viewer::Settings settings;
        settings.spinning = _spinning;
        settings.flying = _flying;
        settings.wireframe = _wireframe;
        settings.color =
            viewer::Renderer::parseColor(_meshColor ? _meshColor.UTF8String : "#e8b56b");
        _renderer.draw((int)view.drawableWidth, (int)view.drawableHeight, _frameElapsed, settings);
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            [self fail:[NSString stringWithFormat:@"ANGLE draw error 0x%x", error]];
            return;
        }
        if (!_reportedFrame) {
            _reportedFrame = YES;
            NSLog(@"ANGLE first frame: %ldx%ld, framebuffer=%u, GL error=0x%x",
                  (long)view.drawableWidth, (long)view.drawableHeight,
                  view.defaultOpenGLFrameBufferID, error);
        }
    } catch (const std::exception &error) {
        [self fail:[NSString stringWithUTF8String:error.what()]];
    }
}
- (void)dealloc {
    [_displayLink invalidate];
    if (!_glView)
        return;
    MGLContext *previous = MGLContext.currentContext;
    MGLLayer *previousLayer = MGLContext.currentLayer;
    if ([MGLContext setCurrentContext:_glView.context])
        _renderer.releaseResources();
    else
        _renderer.abandonResources();
    [MGLContext setCurrentContext:previous == _glView.context ? nil : previous
                         forLayer:previous == _glView.context ? nil : previousLayer];
}
@end

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
RCT_EXPORT_VIEW_PROPERTY(spinning, BOOL)
RCT_EXPORT_VIEW_PROPERTY(flying, BOOL)
RCT_EXPORT_VIEW_PROPERTY(wireframe, BOOL)
RCT_EXPORT_VIEW_PROPERTY(resetToken, NSInteger)
RCT_EXPORT_VIEW_PROPERTY(onError, RCTDirectEventBlock)
@end
