# React Native UI with a C++ ANGLE renderer on UWP

Windows runs the shared React screen in `src/App.tsx`. All buttons, model
search/selection/random, rotation/flight/reset, colors, captions and error
messages are React Native components. C++ provides only the graphics view,
GLES resources, mesh state, and rendering/lifecycle integration.

```text
React Native controls (src/App.tsx)
  -> LegacyOpenGLView React properties
  -> C++ SwapChainPanel / EGL / OpenGL ES renderer
  -> ANGLE D3D11 -> GPU
```

`App.xaml` and `MainPage.xaml` are the UWP React host shell. They do not
implement viewer controls. `ReactPackageProvider.cpp` registers the ANGLE view
manager; `AutolinkedNativeModules.*` remains in place for community modules.
The Windows entrypoint `index.windows.js` registers `OpenGLLab` without loading
Expo's Apple bootstrap. iOS/Mac Catalyst keep their existing Expo entrypoint
and Objective-C++/MGLKit platform adapter over the same C++ renderer.

## Build Release x64 and ARM64

```powershell
npm ci
.\scripts\build-uwp-release.ps1
```

Open `microsoft_platform/OpenGLLab.sln` in Visual Studio. The solution has exactly two
configurations: Release x64 and Release ARM64. Defaults match this machine's
Visual Studio Community 2026 UWP v145 toolchain and Windows SDK 10.0.26100.0.
Install C++ UWP components for both architectures on another machine, or pass
`-MSBuild`, `-Toolset` and `-SDK` matching the installation.

Dependencies are pinned to React Native 0.81.5, React 19.1.0 and React Native
Windows 0.81.4. Windows intentionally uses RNW's legacy Paper/UWP architecture:
[Microsoft removed this UWP path in RNW 0.82](https://microsoft.github.io/react-native-windows/docs/getting-started/).
Do not upgrade RNW independently to 0.82+ while targeting UWP. The project uses
the matching Microsoft.ReactNative/Cxx NuGet packages rather than compiling
the entire framework from source. NuGet restore runs as part of the build;
`microsoft_platform/NuGet.Config` includes Microsoft's public React Native feed.

The build script generates the model list, runs Windows autolinking, verifies
and copies the ANGLE package, restores NuGet, builds/bundles the application,
and inspects each actual MSIX payload. Release packages contain Hermes and
precompiled JavaScript; Metro is not required while running them. Bundling
errors stop the build so a stale bundle cannot silently be packaged.

Windows bundling uses `metro.windows.config.js`. `metro.config.js` retains
Expo defaults for Apple/web. Babel uses the Expo preset, compatible with the
shared TypeScript/React code. `npm run check` checks the TypeScript application.

## ANGLE integration

The default package source is
`C:\Users\ASUS\source\repos\angle\artifacts\angle-uwp-release`.
Pass `-AnglePackage` to use another portable copy. Preparation verifies its
SHA-256 inventory and copies the entire package to ignored `third_party/angle-uwp`.
Headers, import libraries and DLLs stay from the same build. To prepare only:

```powershell
.\scripts\prepare-angle-uwp.ps1
```

`ANGLE.UWP.props` supplies includes, architecture-specific import libraries,
and package-root libEGL.dll, libGLESv2.dll and d3dcompiler_47.dll. The app uses
Release `/MD`, preserves UWP's VCLibs dependency, and packages the ANGLE license,
ten original OBJ models and two lighting shaders.

`AngleViewManager.cpp` maps model, meshColor, spinning, flying, wireframe and
resetToken to C++ renderer state. Native `onError` events return rendering
failures to the React overlay. No native buttons, model list or color controls
are implemented in C++. Each React view owns its renderer through a holder;
weak references avoid panel/renderer cycles. React view removal and XAML
unload stop rendering and release resources.

The renderer requests a matching ES 2 context and EGL_OPENGL_ES2_BIT config for
the original ES2 shaders. Apple also requests ES2. It explicitly selects
ANGLE's D3D11 backend without a silent software fallback. The SwapChainPanel
is retained by the React visual tree and passed as an ABI IInspectable*;
a static assertion checks ANGLE's UWP native window type. No HWND is used.

C++ drives frames through CompositionTarget.Rendering, queries surface size
for resize/DPI changes, pauses when invisible, and releases/recreates resources
on suspension/resume. Context loss or invalidated surfaces recreate EGL,
shaders and mesh buffers. This frame loop is independent of JavaScript.

## Packages and local launch

| Architecture | App package |
| --- | --- |
| x64 | artifacts/packages/x64/OpenGLLab_1.0.1.0_x64_Test/OpenGLLab_1.0.1.0_x64.msix |
| ARM64 | artifacts/packages/ARM64/OpenGLLab_1.0.1.0_ARM64_Test/OpenGLLab_1.0.1.0_ARM64.msix |

Native output is under `artifacts/uwp/<arch>/OpenGLLab`. Build logs and binary
logs are under `artifacts/logs`. The verifier checks PE architectures,
AppContainer flags, ANGLE/Hermes/React runtime DLLs, JavaScript bundle, models,
shaders, ANGLE license and VCLibs dependency. To repeat inspection:

```powershell
.\scripts\verify-uwp-package.ps1 -Architecture x64
.\scripts\verify-uwp-package.ps1 -Architecture ARM64
```

Enable Developer Mode, then launch the matching architecture:

```powershell
.\scripts\run-uwp.ps1 -Architecture x64
```

To also verify a fresh React bundle load, native property update and ANGLE
frame presentation, close the running viewer and use:

```powershell
.\scripts\run-uwp.ps1 -Architecture x64 -VerifyRendering
```

This unpacks and registers an unsigned development layout before launching.
Close the viewer before registering an updated build. Keep `artifacts/deploy`
in place while this development registration is installed. Run ARM64 on an
ARM64 Windows device. If Windows reports a missing framework dependency,
install the architecture's VCLibs package from the release's Dependencies folder.

Packages are unsigned by default. To create a signed installable MSIX, supply
a certificate in the current user's certificate store with subject matching
Publisher="CN=ASUS" in Package.appxmanifest:

```powershell
.\scripts\build-uwp-release.ps1 -CertificateThumbprint YOUR_THUMBPRINT
```

The destination must trust the certificate and have the matching framework
dependencies. Replace the identity/publisher with your reserved identity for
Store distribution. RNW's minimum Windows version is 10.0.17763.0.

## Runtime diagnostics

The run script prints the exact log path:
`%LOCALAPPDATA%/Packages/<package-family>/LocalState/renderer.log`.
It records React startup/bundle loading, native-view creation, property updates,
GL renderer/version, model vertex counts, first-frame readback/presentation,
and rendering or JavaScript errors. The central framebuffer readback provides
evidence of drawing, not a visual regression test.

Both Release packages passed build and package verification. The x64 fresh-launch
check passed with the React bundle loaded and React properties received by C++.
ANGLE reported AMD Radeon Graphics through Direct3D11; the first frame was
1168x539 with GL_NO_ERROR, and the central readback contained 4096 pixels above
background. The captured session is `artifacts/logs/uwp-react-angle-runtime-x64.log`.
Builds completed with zero errors; warnings came from Hermes compilation and an
installed ATL library search path.

ARM64 device rendering and Store certification require separate validation.
Suspension/device-loss paths are implemented but have not been fault-injected.
Apple targets are not rebuilt on Windows. Check upstream licensing before
distributing inherited models and shaders.

## Shared renderer extraction (2026-10-06)

The UWP adapter now calls `shared/renderer/ViewerRenderer.cpp`, the same scene/GLES
implementation used by the Apple adapter. DirectXMath was replaced with the shared
column-major OpenGL projection. EGL setup, SwapChainPanel presentation, React
properties/events and suspend/device-loss handling remain Windows responsibilities.

Run `scripts/test-shared-renderer.ps1` for a D3D11 ANGLE pbuffer smoke test. It
compiles the production shared source and draws all ten bundled models, checks
wireframe coverage, animation/reset, malformed input and resource/context recreation.
The test needs the x64 ANGLE package and installed x64 Microsoft.VCLibs.140.00.
This pbuffer test complements the UWP fresh-launch test; it does not fault-inject
UWP suspension or physical device loss.

The Mac checkout also provides `bash scripts/test-shared-renderer-mac.sh`, which
runs the same production renderer and smoke-test cases against MetalANGLE with
an ES2 context. Windows keeps its D3D11/ES2 path. Changes to platform context
setup belong in the adapters; scene/rendering changes belong in `shared/renderer/`.
