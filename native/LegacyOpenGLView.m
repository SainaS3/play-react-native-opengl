#import <React/RCTViewManager.h>
#import <MetalANGLE/MGLKit.h>
#import <GLES3/gl3.h>
#import "ViewerMath.h"
#import <float.h>

// React owns the controls. This native view owns the GL context and frame loop.
typedef struct { ViewerVector3 position; ViewerVector3 normal; } MeshVertex;
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
@end

@implementation GLFrameProxy
- (void)tick:(CADisplayLink *)link { [self.view tick:link]; }
@end

@implementation LegacyOpenGLView {
  MGLKView *_glView;
  BOOL _reportedFrame;
  GLuint _program, _triangles, _lines;
  GLsizei _triangleCount, _lineCount;
  float _angle, _height, _velocity;
  CFTimeInterval _lastTime;
  CADisplayLink *_displayLink;
  NSString *_failure;
}

- (instancetype)init {
  if ((self = [super initWithFrame:CGRectZero])) {
    MGLContext *context = nil;
    @try { context = [[MGLContext alloc] initWithAPI:kMGLRenderingAPIOpenGLES3]; }
    @catch (NSException *exception) { [self fail:exception.reason]; return self; }
    if (!context || ![MGLContext setCurrentContext:context]) { [self fail:@"Cannot create ANGLE GLES 3 context"]; return self; }
    // MGLKView explicitly disallows subclassing: own it as a child UIView.
    _glView = [[MGLKView alloc] initWithFrame:self.bounds context:context];
    _glView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
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
    if (![MGLContext setCurrentContext:context forLayer:nil]) { [self fail:@"Cannot bind ANGLE context after drawable setup"]; return self; }
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    NSLog(@"ANGLE backend: %s | %s | %s", glGetString(GL_VENDOR), renderer, glGetString(GL_VERSION));
    if (!renderer || !strstr(renderer, "Metal")) { [self fail:@"ANGLE did not select the Metal backend"]; return self; }
    [self createProgram];
    glGenBuffers(1, &_triangles);
    glGenBuffers(1, &_lines);
    self.model = @"cone.obj";
  }
  return self;
}

- (void)fail:(NSString *)message {
  _failure = message;
  NSLog(@"Native OpenGL: %@", message);
  if (_onError) _onError(@{@"message": message});
}
- (void)setOnError:(RCTDirectEventBlock)onError {
  _onError = [onError copy];
  if (_failure && _onError) _onError(@{@"message": _failure});
}

- (GLuint)shader:(GLenum)type resource:(NSString *)name extension:(NSString *)extension {
  NSString *path = [[NSBundle mainBundle] pathForResource:name ofType:extension];
  NSString *source = path ? [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil] : nil;
  if (!source) { [self fail:[NSString stringWithFormat:@"Missing shader %@.%@", name, extension]]; return 0; }
  GLuint shader = glCreateShader(type);
  const GLchar *text = source.UTF8String;
  glShaderSource(shader, 1, &text, NULL);
  glCompileShader(shader);
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    GLchar log[2048]; glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    [self fail:[NSString stringWithUTF8String:log]]; glDeleteShader(shader); return 0;
  }
  return shader;
}
- (void)createProgram {
  GLuint vert = [self shader:GL_VERTEX_SHADER resource:@"sVertexLighting" extension:@"vsh"];
  GLuint frag = [self shader:GL_FRAGMENT_SHADER resource:@"sVertexLighting" extension:@"fsh"];
  if (!vert || !frag) { if (vert) glDeleteShader(vert); if (frag) glDeleteShader(frag); return; }
  _program = glCreateProgram();
  glAttachShader(_program, vert); glAttachShader(_program, frag);
  glBindAttribLocation(_program, 0, "a_position"); glBindAttribLocation(_program, 1, "a_normal");
  glLinkProgram(_program); glDeleteShader(vert); glDeleteShader(frag);
  GLint linked = 0; glGetProgramiv(_program, GL_LINK_STATUS, &linked);
  if (!linked) { GLchar log[2048]; glGetProgramInfoLog(_program, sizeof(log), NULL, log); [self fail:[NSString stringWithUTF8String:log]]; }
}

// OBJ positions and faces only; fan-triangulates polygons, supports negative indices.
// The original assets' materials are replaced by the selected diffuse color.
- (void)setModel:(NSString *)model {
  if (!_glView || _failure || [_model isEqualToString:model]) return;
  if (![model.lastPathComponent isEqualToString:model] || ![model.pathExtension isEqualToString:@"obj"]) { [self fail:@"Invalid model name"]; return; }
  NSString *path = [[NSBundle mainBundle] pathForResource:model.stringByDeletingPathExtension ofType:@"obj"];
  NSString *source = path ? [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil] : nil;
  if (!source) { [self fail:[NSString stringWithFormat:@"Missing model %@", model]]; return; }
  NSMutableData *positions = [NSMutableData data], *vertices = [NSMutableData data];
  for (NSString *line in [source componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet]) {
    NSString *clean = [[line componentsSeparatedByString:@"#"][0] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    NSArray *parts = [[clean componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceCharacterSet] filteredArrayUsingPredicate:[NSPredicate predicateWithFormat:@"length > 0"]];
    if (parts.count >= 4 && [parts[0] isEqualToString:@"v"]) {
      ViewerVector3 p = ViewerVector3Make([parts[1] floatValue], [parts[2] floatValue], [parts[3] floatValue]);
      if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z)) { [self fail:@"Non-finite OBJ position"]; return; }
      [positions appendBytes:&p length:sizeof(p)];
    } else if (parts.count >= 4 && [parts[0] isEqualToString:@"f"]) {
      NSMutableArray<NSNumber *> *indices = [NSMutableArray array];
      NSInteger count = positions.length / sizeof(ViewerVector3);
      for (NSUInteger i = 1; i < parts.count; i++) {
        NSInteger index = [[[parts[i] componentsSeparatedByString:@"/"] firstObject] integerValue];
        index = index > 0 ? index - 1 : count + index;
        if (index < 0 || index >= count) { [self fail:@"OBJ face index outside vertex array"]; return; }
        [indices addObject:@(index)];
      }
      const ViewerVector3 *points = positions.bytes;
      for (NSUInteger i = 1; i + 1 < indices.count; i++) {
        ViewerVector3 a = points[indices[0].integerValue], b = points[indices[i].integerValue], c = points[indices[i + 1].integerValue];
        ViewerVector3 normal = ViewerVector3CrossProduct(ViewerVector3Subtract(b, a), ViewerVector3Subtract(c, a));
        float length = ViewerVector3Length(normal);
        normal = length > 1e-12f ? ViewerVector3DivideScalar(normal, length) : ViewerVector3Make(0, 1, 0);
        MeshVertex triangle[] = {{a, normal}, {b, normal}, {c, normal}};
        [vertices appendBytes:triangle length:sizeof(triangle)];
      }
    }
  }
  if (!vertices.length) { [self fail:@"Model contains no triangle faces"]; return; }
  MeshVertex *data = vertices.mutableBytes;
  NSUInteger count = vertices.length / sizeof(MeshVertex);
  ViewerVector3 low = ViewerVector3Make(FLT_MAX, FLT_MAX, FLT_MAX), high = ViewerVector3Make(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (NSUInteger i = 0; i < count; i++) { low = ViewerVector3Minimum(low, data[i].position); high = ViewerVector3Maximum(high, data[i].position); }
  ViewerVector3 center = ViewerVector3MultiplyScalar(ViewerVector3Add(low, high), .5f), size = ViewerVector3Subtract(high, low);
  float scale = 2.4f / fmaxf(fmaxf(size.x, size.y), fmaxf(size.z, 1e-6f));
  NSMutableData *edges = [NSMutableData data];
  for (NSUInteger i = 0; i < count; i++) data[i].position = ViewerVector3MultiplyScalar(ViewerVector3Subtract(data[i].position, center), scale);
  for (NSUInteger i = 0; i < count; i += 3) {
    MeshVertex edge[] = {data[i], data[i+1], data[i+1], data[i+2], data[i+2], data[i]};
    [edges appendBytes:edge length:sizeof(edge)];
  }
  [MGLContext setCurrentContext:_glView.context];
  glBindBuffer(GL_ARRAY_BUFFER, _triangles); glBufferData(GL_ARRAY_BUFFER, vertices.length, vertices.bytes, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, _lines); glBufferData(GL_ARRAY_BUFFER, edges.length, edges.bytes, GL_STATIC_DRAW);
  _triangleCount = (GLsizei)count; _lineCount = (GLsizei)(edges.length / sizeof(MeshVertex));
  _model = [model copy]; _height = _velocity = _angle = 0;
  NSLog(@"Native OpenGL loaded %@ (%d vertices)", model, _triangleCount);
}
- (void)setResetToken:(NSInteger)resetToken { _resetToken = resetToken; _height = _velocity = _angle = 0; }
- (void)didMoveToWindow {
  [super didMoveToWindow];
  if (self.window && _glView && !_failure && !_displayLink) {
    GLFrameProxy *proxy = [GLFrameProxy new]; proxy.view = self;
    _displayLink = [CADisplayLink displayLinkWithTarget:proxy selector:@selector(tick:)];
    [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    _lastTime = 0;
  } else if (!self.window) { [_displayLink invalidate]; _displayLink = nil; }
}
- (void)tick:(CADisplayLink *)link {
  if (UIApplication.sharedApplication.applicationState != UIApplicationStateActive) { _lastTime = 0; return; }
  float dt = _lastTime ? fminf(fmaxf(link.timestamp - _lastTime, 0), .05f) : 0;
  _lastTime = link.timestamp;
  if (_spinning) _angle += dt * .6f;
  float acceleration = _flying ? .5f : 0;
  _height += _velocity * dt + .5f * acceleration * dt * dt; _velocity += acceleration * dt;
  if (_failure || CGRectIsEmpty(_glView.bounds)) return;
  @try { [_glView display]; }
  @catch (NSException *exception) { [self fail:exception.reason]; [_displayLink invalidate]; _displayLink = nil; }
}
- (void)uniform4:(const char *)name x:(float)x y:(float)y z:(float)z w:(float)w { glUniform4f(glGetUniformLocation(_program, name), x, y, z, w); }
- (void)mglkView:(MGLKView *)view drawInRect:(CGRect)rect {
  glViewport(0, 0, (GLsizei)view.drawableWidth, (GLsizei)view.drawableHeight);
  glClearColor(.067f, .106f, .161f, 1); glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!_program || !_triangleCount || view.drawableHeight == 0) return;
  glUseProgram(_program);
  float aspect = (float)view.drawableWidth / (float)view.drawableHeight;
  ViewerMatrix4 model = ViewerMatrix4Multiply(ViewerMatrix4MakeTranslation(0, _height, 0), ViewerMatrix4MakeYRotation(_angle));
  ViewerMatrix4 projection = ViewerMatrix4MakeOrtho(-1.8f * aspect, 1.8f * aspect, -1.8f, 1.8f, -10, 10);
  ViewerMatrix4 mvp = ViewerMatrix4Multiply(projection, model);
  glUniformMatrix4fv(glGetUniformLocation(_program, "u_mvMatrix"), 1, GL_FALSE, model.m);
  glUniformMatrix4fv(glGetUniformLocation(_program, "u_mvpMatrix"), 1, GL_FALSE, mvp.m);
  ViewerVector3 light = ViewerVector3Normalize(ViewerVector3Make(.4f, .7f, 1));
  glUniform3f(glGetUniformLocation(_program, "u_directionalLight.direction"), light.x, light.y, light.z);
  glUniform3f(glGetUniformLocation(_program, "u_directionalLight.halfplane"), 0, 0, 1);
  [self uniform4:"u_directionalLight.ambientColor" x:.4f y:.4f z:.4f w:1];
  [self uniform4:"u_directionalLight.diffuseColor" x:1 y:1 z:1 w:1];
  [self uniform4:"u_directionalLight.specularColor" x:.3f y:.3f z:.3f w:1];
  unsigned int rgb = 0xe8b56b;
  if ([_meshColor hasPrefix:@"#"]) [[NSScanner scannerWithString:[_meshColor substringFromIndex:1]] scanHexInt:&rgb];
  [self uniform4:"u_material.ambientFactor" x:1 y:1 z:1 w:1];
  [self uniform4:"u_material.diffuseFactor" x:((rgb>>16)&255)/255.f y:((rgb>>8)&255)/255.f z:(rgb&255)/255.f w:1];
  [self uniform4:"u_material.specularFactor" x:.5f y:.6f z:.5f w:1];
  glUniform1f(glGetUniformLocation(_program, "u_material.shininess"), 24);
  glBindBuffer(GL_ARRAY_BUFFER, _wireframe ? _lines : _triangles);
  glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)offsetof(MeshVertex, position));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)offsetof(MeshVertex, normal));
  glDrawArrays(_wireframe ? GL_LINES : GL_TRIANGLES, 0, _wireframe ? _lineCount : _triangleCount);
  if (!_reportedFrame) {
    _reportedFrame = YES;
    GLenum error = glGetError();
    NSLog(@"ANGLE first frame: %ldx%ld, framebuffer=%u, GL error=0x%x", (long)view.drawableWidth, (long)view.drawableHeight, view.defaultOpenGLFrameBufferID, error);
    if (error != GL_NO_ERROR) [self fail:[NSString stringWithFormat:@"ANGLE draw error 0x%x", error]];
  }
}
- (void)dealloc {
  [_displayLink invalidate];
  if (!_glView) return;
  MGLContext *previous = MGLContext.currentContext;
  MGLLayer *previousLayer = MGLContext.currentLayer;
  [MGLContext setCurrentContext:_glView.context];
  glDeleteBuffers(1, &_triangles); glDeleteBuffers(1, &_lines); if (_program) glDeleteProgram(_program);
  [MGLContext setCurrentContext:previous == _glView.context ? nil : previous forLayer:previous == _glView.context ? nil : previousLayer];
}
@end

@interface LegacyOpenGLViewManager : RCTViewManager @end
@implementation LegacyOpenGLViewManager
RCT_EXPORT_MODULE(LegacyOpenGLView)
+ (BOOL)requiresMainQueueSetup { return YES; }
- (UIView *)view { return [LegacyOpenGLView new]; }
RCT_EXPORT_VIEW_PROPERTY(model, NSString)
RCT_EXPORT_VIEW_PROPERTY(meshColor, NSString)
RCT_EXPORT_VIEW_PROPERTY(spinning, BOOL)
RCT_EXPORT_VIEW_PROPERTY(flying, BOOL)
RCT_EXPORT_VIEW_PROPERTY(wireframe, BOOL)
RCT_EXPORT_VIEW_PROPERTY(resetToken, NSInteger)
RCT_EXPORT_VIEW_PROPERTY(onError, RCTDirectEventBlock)
@end
