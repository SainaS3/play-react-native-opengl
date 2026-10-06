# Shared C++ renderer assessment

Initial source assessment on 2026-10-06, retained as the design rationale. The extraction has since been implemented; see [Shared renderer handoff](SHARED-RENDERER-HANDOFF.md) for the current API, integration and validation.

## Conclusion

One shared C++ scene/GLES renderer is feasible, with two platform adapters. Use an adapter/bridge boundary for native context, drawable and lifecycle operations. A proxy forwarding every GLES call is unnecessary for the current one-runtime-per-platform packaging.

The Apple adapter must remain Objective-C++ to use UIKit/MGLKit. The Windows adapter remains C++/WinRT. Both can call the same C++ renderer implementation, compiled separately for their platform and linked to their respective ANGLE runtime.

## Current implementation

| Responsibility | Apple: native/LegacyOpenGLView.mm | Windows: windows/OpenGLLab/AngleViewManager.cpp |
| --- | --- | --- |
| Native host | UIView with an MGLKView child/delegate | SwapChainPanel |
| Context | MGLContext, GLES 2 requested | EGL, GLES 2 requested, D3D11 explicitly selected |
| Frame trigger | CADisplayLink, MGLKView display/delegate | CompositionTarget.Rendering |
| Presentation | MGLKView manages framebuffer and presentation | eglSwapBuffers |
| Assets | NSBundle and NSString | Packaged resources and std::ifstream |
| Renderer | Calls shared `viewer::Renderer` | Calls the same shared `viewer::Renderer` |
| Math | Shared ViewerMath, column-major OpenGL orthographic projection | Same shared ViewerMath |
| Recovery | Error event; stops drawing on failure | Recreates context/resources after selected swap failures; releases on suspension/unload |
| Verification | First-frame GL error and framebuffer log | GL error, central-tile readback and presentation log |

Both expose the same React component name and model, meshColor, spinning, flying, wireframe, resetToken properties, plus onError. The JS-facing interface is already shared.

## Names and runtime linkage

The Apple source calls Objective-C classes MGLContext, MGLKView and MGLLayer. Its GLES calls still use glCreateShader, glBufferData, glUniformMatrix4fv and glDrawArrays. Windows uses those same GLES function names. MGL class naming therefore belongs behind the Apple adapter; it does not require a different scene renderer interface.

The local Apple pod configuration links MetalANGLE.xcframework and supplies its Khronos headers. Windows ANGLE.UWP.props links libEGL.lib/libGLESv2.lib and packages their DLLs. Each app selects its ANGLE implementation through platform build/link configuration, and creates a native context at runtime. This is not runtime switching between Metal and DirectX inside one executable.

Original Windows assessment evidence limit: native/vendor and the exact Darwin framework headers/binary were absent in the Windows checkout. The sibling angle checkout lacks the Darwin handoff and MGLKit files. docs/WORKLOG.md records earlier Apple symbol inspection resolving GLES calls to MetalANGLE, but this assessment cannot independently confirm the exact artifact's export names, header macros or ABI. Recheck its selected-slice headers and exports on the Mac before committing the migration. Do not substitute the Windows ANGLE headers for that artifact.

If a future requirement truly loads multiple GLES implementations into one process, add a runtime-owned GLES dispatch table with correctly typed function pointers/calling conventions and strict context ownership. Existing globally linked gl* calls cannot select arbitrary libraries by themselves. That requirement is separate from sharing source across Apple and Windows apps.

## Proposed boundary

```text
React props / platform frame callback
                  |
      Shared ViewerRenderer (C++)
      OBJ, math, animation, GLES resources/draw
                  |
      GLES calls linked per platform
         /                         \
MetalANGLE framework          libGLESv2 / libEGL
         |                         |
       Metal                     D3D11

Apple adapter: MGLContext/MGLKView, assets, frame timing, errors
Windows adapter: EGL/SwapChainPanel, assets, frame timing, errors/recovery
```

A suggested shared public API uses C++ data only:

```cpp
class ViewerRenderer {
public:
    // Called with the owning context current; sources supplied by host.
    void createResources(const ShaderSources& shaders);
    void setMesh(const MeshData& mesh);
    void setSettings(const ViewerSettings& settings);
    void resetAnimation();
    void advance(float elapsedSeconds);
    void draw(int drawableWidth, int drawableHeight);
    void releaseResources(); // Valid current context required.
    void abandonResources(); // Context lost: discard stale handles, no GL deletes.
};
```

This is an API sketch, not implemented declarations. Keep GL types/private resource handles behind the implementation. Asset loading can remain in adapters initially; share a pure C++ parser accepting text. Explicit resource release avoids assuming a C++ destructor always runs with the correct context current.

Do not force a single beginFrame/present pair onto both hosts: MGLKView binds its drawable and invokes the draw delegate, whereas Windows explicitly binds EGL and swaps afterward. Invoke the shared draw from each platform's correct point. The shared draw must preserve the host-provided framebuffer; never assume framebuffer zero or present Apple's surface itself.

## Migration checks

1. Extract CPU-only mesh parsing, normalization, animation and math first. Adopt deliberate common validation: Windows currently rejects malformed/zero face indices and invalid colors more strictly than Apple.
2. Resolve projection behavior explicitly. DirectX RH orthographic depth is 0..1; ViewerMath uses OpenGL -1..1. Transposing matrix storage does not remove that depth-range difference. Also make ViewerMath standard C++: its C compound literals need replacement for portable MSVC compilation.
3. Extract GLES2-compatible resource/draw code. Both adapters now request ES2 and use the same ES2 operations/shaders. Keep ES3 features out of the common baseline until a future explicit upgrade.
4. Retain context ownership, UI-thread execution, drawable size in pixels, Apple depth/MSAA setup, Windows recovery, suspend/resume timing and error-event behavior in adapters. CPU scene/settings must survive GPU resource recreation; decide explicitly whether animation resets during recovery.
5. Register new .cpp files in the Expo Xcode plugin and the existing Windows vcxproj, with shared sources configured appropriately for Windows precompiled headers. Preserve platform header paths and library linkage.
6. On Apple, verify the exact MGLKit delegate/framebuffer lifecycle and the shipped artifact's GLES headers/exports; build device/simulator/Catalyst slices. On Windows, build supported x64/ARM64 configurations. Verify actual backend strings, model counts, controls, resize, detach/reattach and suspension/recovery, then actual rendered frames/readback on each platform. Apple MSAA readback needs the correct resolved target; do not transplant Windows readback blindly.

The proposed extraction is now implemented. Maintain scene changes in the shared core and platform lifecycle changes in the native adapters. Apple artifact verification and repeatable Metal smoke-test evidence are recorded in [Shared renderer handoff](SHARED-RENDERER-HANDOFF.md); the earlier assessment limits above describe what was available on Windows.
