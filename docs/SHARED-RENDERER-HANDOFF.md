# Shared renderer handoff

Implemented 2026-10-06 for Windows and Apple hosts. Windows evidence and the Mac checkout’s Apple follow-through are recorded separately below.

## Pattern and ownership

`viewer::Renderer` in `native/shared/ViewerRenderer.h` is the common C++ facade. Its private implementation owns OBJ parsing/normalization, triangle and edge buffers, GLSL programs, lighting, OpenGL matrix math and animation. Both native adapters call that same implementation; the adapters own their platform APIs and lifecycles. This is an adapter boundary with Pimpl hiding GLES handles, rather than a proxy around every GLES function.

| Adapter | Runtime | Responsibilities |
| --- | --- | --- |
| `native/LegacyOpenGLView.mm` | MetalANGLE framework | MGLContext/MGLKView, NSBundle text, CADisplayLink timing, React props/errors, framebuffer/presentation ownership |
| `windows/OpenGLLab/AngleViewManager.cpp` | libEGL/libGLESv2, D3D11 | EGL/SwapChainPanel, packaged resource text, CompositionTarget timing, React props/errors, swap, readback, suspension and recreation |

The runtime is linked separately for each platform. No shared public header includes MGLKit, WinRT, EGL or GLES. The shared `.cpp` uses the target's Khronos GLES2 header and standard `gl*` names. Apple and Windows both request GLES2. The used draw operations remain GLES2-compatible.

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

## Apple follow-through (2026-10-06)

The Mac checkout regenerated Xcode/CocoaPods with `npm run angle:configure`.
Both `LegacyOpenGLView.mm` and `native/shared/ViewerRenderer.cpp` are registered
in the generated application's Sources phase. Keep source registration in the
Expo plugin; the ignored Xcode project is not the durable configuration.

Verified all 116 files against the supplied `native/vendor/SHA256SUMS.json`.
The compiler header trace selects the Catalyst slice's `Headers/GLES2/gl2.h`.
The public renderer interface remains free of Objective-C, WinRT and GL types.
Apple forwards elapsed seconds to the common clamp and stops CADisplayLink on
native failure. Context/drawable binding, depth/MSAA and presentation remain
MGLKView responsibilities. There is no second Apple scene implementation.

Repeat the common smoke test on Mac:

```sh
bash scripts/test-shared-renderer-mac.sh
```

The script compiles the production renderer and `tests/ViewerRendererSmoke.cpp`
as a Catalyst executable, linking the local Catalyst MetalANGLE framework.
The same test chooses Metal/ES2 on Apple and D3D11/ES2 on Windows. The EXT display
entry point receives `nullptr` for the default native display because Darwin's
`EGLNativeDisplayType` is an integer while this entry point accepts `void*`.
No Windows package or SDK headers are substituted for Apple headers.

On Apple M2 it reported `ANGLE (Metal Renderer: Apple M2)` and passed pixel
readback for all ten models (cone: 186 vertices, 3698 mesh pixels), wireframe,
animation/reset, malformed OBJ/color rejection, negative indices and explicit
release/abandon/context/resource recreation. Evidence is in
`artifacts/renderer-tests-mac/smoke.log`; the header trace is beside it.
This pbuffer test has no MGLKView MSAA surface, so it does not establish UIKit
presentation, view reattachment, resizing or background/foreground behavior.


Apple application verification:

- Catalyst Release build succeeded for arm64 and x86_64. Both adapter and shared
  renderer compile in each architecture. It launched on Apple M2 with bundled
  Release JavaScript, without Metro.
- Runtime: `ANGLE (Metal Renderer: Apple M2)`, GLES 3.0, cone 186 vertices,
  first drawable `1163x649`, framebuffer 0, `GL error=0x0`.
- GLES symbols `glDrawArrays`, `glGenBuffers` and `glGetString` resolve to
  MetalANGLE in the Catalyst arm64 app and signed iPhone app. Neither app links
  Apple GLKit/OpenGLES directly.
- Signed iPhone Release build succeeded and installation succeeded. Launch was
  denied by the device lock screen; the user then requested Mac-only runtime
  testing. No new iOS runtime or visual success is claimed.
- TypeScript, plugin/script syntax and diff whitespace checks passed.
- UI automation timed out; visible controls, resize, detach/reattach and app
  background/foreground behavior remain unverified in this pass. Intel runtime
  and simulator runtime were not executed. The pbuffer recreation test does not
  establish platform device-loss recovery or ES conformance.

The first iPhone build compiled both renderer sources but failed during JS
bundling because local `node_modules` lacked the declared `babel-preset-expo`.
Repairing the local dependency installation restored the lockfile's Expo 54.0.37,
React Native 0.81.5 and Babel preset 54.0.12; tracked manifests were unchanged.
The retry passed. Native code changes still require rebuilding each host.

Logs: `artifacts/logs/shared-catalyst-build.log`, `shared-catalyst-runtime.log`,
`shared-ios-build-retry.log`, `shared-ios-install.log`, and
`shared-ios-runtime.log` (the latter records the launch denial). The Catalyst
app is left running for a manual check of model selection, color, Rotate/Pause,
Fly/Reset, Wireframe and resizing.


## GLES2 baseline (2026-10-06)

At the user’s request, both Apple targets now import MetalANGLE’s GLES2 header
and create `kMGLRenderingAPIOpenGLES2` contexts. Windows already requests EGL
client version 2; the single shared renderer and existing shaders remain
unchanged. ES3 is reserved for a future deliberate upgrade. Earlier ES3 runtime
entries above are historical evidence before this change.

The shared smoke test now requests ES2 on both platforms and asserts the runtime
version. On Apple M2 it reports `OpenGL ES 2.0.0` through MetalANGLE and passes
all ten models, wireframe, animation/reset, validation and resource recreation.
The existing Mac runner is unchanged. iOS runtime testing remains skipped at
the user’s request; its context selection shares this same Apple source.

Catalyst Release rebuild passed for arm64/x86_64. Fresh Mac launch reports
`ANGLE (Metal Renderer: Apple M2) | OpenGL ES 2.0.0`, cone 186 vertices and
first frame 1163×613 with GL error 0x0. Logs are
`artifacts/logs/shared-es2-catalyst-build.log` and
`artifacts/logs/shared-es2-catalyst-runtime.log`. The ES2 app is left running.
No new iOS rebuild/runtime or Windows rerun was performed for this change.
