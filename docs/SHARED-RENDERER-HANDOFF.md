# Shared renderer handoff

Implemented 2026-10-06 for Windows and Apple hosts. Windows validation is performed here; Mac/iOS validation belongs to the Mac checkout.

## Pattern and ownership

`viewer::Renderer` in `native/shared/ViewerRenderer.h` is the common C++ facade. Its private implementation owns OBJ parsing/normalization, triangle and edge buffers, GLSL programs, lighting, OpenGL matrix math and animation. Both native adapters call that same implementation; the adapters own their platform APIs and lifecycles. This is an adapter boundary with Pimpl hiding GLES handles, rather than a proxy around every GLES function.

| Adapter | Runtime | Responsibilities |
| --- | --- | --- |
| `native/LegacyOpenGLView.mm` | MetalANGLE framework | MGLContext/MGLKView, NSBundle text, CADisplayLink timing, React props/errors, framebuffer/presentation ownership |
| `windows/OpenGLLab/AngleViewManager.cpp` | libEGL/libGLESv2, D3D11 | EGL/SwapChainPanel, packaged resource text, CompositionTarget timing, React props/errors, swap, readback, suspension and recreation |

The runtime is linked separately for each platform. No shared public header includes MGLKit, WinRT, EGL or GLES. The shared `.cpp` uses the target's Khronos GLES2 header and standard `gl*` names. Apple still requests GLES3; Windows still requests GLES2. The used draw operations remain GLES2-compatible.

All renderer GL calls require its owning context to be current on the host thread. The host binds the drawable before `draw`; the renderer neither changes framebuffer binding nor presents. Call `releaseResources()` while that context is valid/current, or `abandonResources()` when it cannot be bound. The renderer destructor releases CPU ownership only. Reload shaders and the model after context recreation; settings stay in the host. Loading a mesh resets animation, matching the previous behavior.

Elapsed time is clamped to 0..0.05 seconds in the shared core. Reset clears rotation/height/velocity and adapters reset their timing baseline. Both platforms use ViewerMath's OpenGL -1..1 depth projection. Windows previously used DirectXMath's 0..1 projection; this change deliberately makes the shared GLES camera consistent. ViewerMath now uses standard C++ aggregate initialization.

The stricter OBJ index/position and color validation previously used on Windows is shared with Apple. Invalid input produces the existing native error event. Materials, textures and authored smooth normals remain outside this viewer's supported path.

## Build integration

Windows vcxproj and filters include the shared source/header and ViewerMath. The shared source opts out of Windows precompiled headers. The Expo plugin registers both the Apple `.mm` adapter and the shared `.cpp`, with their respective Xcode file types. The pod's existing framework/header configuration remains the ANGLE linkage source.

## Windows evidence

The production shared source compiled in both Release x64 and ARM64 UWP builds, and both package verifications passed. A fresh UWP launch loaded the React bundle, received properties, loaded the 186-vertex cone and reported:

```text
Renderer: ANGLE (AMD Radeon Graphics Direct3D11 vs_5_0 ps_5_0)
Readback: 4096 pixels above background in central tile
First frame presented: 1373x595, GL_NO_ERROR
```

`scripts/test-shared-renderer.ps1` compiles the same source into an x64 D3D11 ANGLE pbuffer test. All ten models produced non-background pixels. Wireframe coverage, animation/reset, malformed OBJ and color rejection, negative indices, and release/abandon/context/resource recreation passed. This is actual drawing/readback, not just shader creation. The test host copies UWP CRT DLLs from the installed x64 VCLibs package into its ignored artifact directory.

Build logs live under `artifacts/logs/uwp-release-{x64,ARM64}.log`. ARM64 runtime rendering requires an ARM64 device. UWP suspension and actual device-loss recovery have not been fault-injected by these checks.

## Mac/iOS checks

1. Use the intended MetalANGLE artifact and regenerate the project with `npm run angle:configure` so the shared `.cpp` enters the Xcode sources. Keep local signing settings as usual.
2. Confirm `<GLES2/gl2.h>` in the shared source comes from the selected MetalANGLE slice, and `gl*` symbols resolve to that framework. The exact artifact is absent on Windows, so those headers/exports are not newly verified here.
3. Build/run iOS and Catalyst. Confirm the Metal backend log, cone vertex count, first-frame drawable dimensions/framebuffer and `GL_NO_ERROR`, plus visible output.
4. Exercise all models, color, wireframe, spinning, flying, reset, resizing, leaving/re-entering the view and app background/foreground. Confirm MGLKView retains its drawable/MSAA/presentation ownership. If adding readback, choose its resolved framebuffer correctly.

No Mac/iOS compilation or runtime success is claimed by the Windows checks.
