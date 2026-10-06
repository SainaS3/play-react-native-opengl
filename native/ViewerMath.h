#pragma once
#include <math.h>

// Column-major math only. Keep Apple's GLKit/OpenGLES out of the ANGLE linker path.
typedef struct { float x, y, z; } ViewerVector3;
typedef struct { float m[16]; } ViewerMatrix4;
static inline ViewerVector3 ViewerVector3Make(float x, float y, float z) { return ViewerVector3{x,y,z}; }
static inline ViewerVector3 ViewerVector3Add(ViewerVector3 a, ViewerVector3 b) { return ViewerVector3Make(a.x+b.x,a.y+b.y,a.z+b.z); }
static inline ViewerVector3 ViewerVector3Subtract(ViewerVector3 a, ViewerVector3 b) { return ViewerVector3Make(a.x-b.x,a.y-b.y,a.z-b.z); }
static inline ViewerVector3 ViewerVector3MultiplyScalar(ViewerVector3 a, float s) { return ViewerVector3Make(a.x*s,a.y*s,a.z*s); }
static inline ViewerVector3 ViewerVector3DivideScalar(ViewerVector3 a, float s) { return ViewerVector3MultiplyScalar(a,1/s); }
static inline float ViewerVector3Length(ViewerVector3 a) { return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z); }
static inline ViewerVector3 ViewerVector3Normalize(ViewerVector3 a) { return ViewerVector3DivideScalar(a,ViewerVector3Length(a)); }
static inline ViewerVector3 ViewerVector3CrossProduct(ViewerVector3 a, ViewerVector3 b) { return ViewerVector3Make(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); }
static inline ViewerVector3 ViewerVector3Minimum(ViewerVector3 a, ViewerVector3 b) { return ViewerVector3Make(fminf(a.x,b.x),fminf(a.y,b.y),fminf(a.z,b.z)); }
static inline ViewerVector3 ViewerVector3Maximum(ViewerVector3 a, ViewerVector3 b) { return ViewerVector3Make(fmaxf(a.x,b.x),fmaxf(a.y,b.y),fmaxf(a.z,b.z)); }
static inline ViewerMatrix4 ViewerMatrix4MakeTranslation(float x, float y, float z) { return ViewerMatrix4{{1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1}}; }
static inline ViewerMatrix4 ViewerMatrix4MakeYRotation(float angle) { float c=cosf(angle),s=sinf(angle); return ViewerMatrix4{{c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1}}; }
static inline ViewerMatrix4 ViewerMatrix4MakeOrtho(float l, float r, float b, float t, float n, float f) { return ViewerMatrix4{{2/(r-l),0,0,0, 0,2/(t-b),0,0, 0,0,-2/(f-n),0, -(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n),1}}; }
static inline ViewerMatrix4 ViewerMatrix4Multiply(ViewerMatrix4 a, ViewerMatrix4 b) {
  ViewerMatrix4 result = {{0}};
  for (int col=0;col<4;col++) for (int row=0;row<4;row++) for (int k=0;k<4;k++) result.m[col*4+row] += a.m[k*4+row]*b.m[col*4+k];
  return result;
}
