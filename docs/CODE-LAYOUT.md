# Native code layout and conventions

| Directory | Responsibility |
| --- | --- |
| `apple_platform/` | iOS and Mac Catalyst view, display link, MGLKit context and local MetalANGLE pod |
| `microsoft_platform/` | Windows UWP application, React Native view manager and EGL/SwapChainPanel host |
| `shared/renderer/` | Platform-independent C++ GLES renderer, OBJ mesh preparation and matrix math |
| `shared/resources/` | Models and shaders bundled by both platform builds |
| `src/` | React controls, state and native view properties |

The macOS target is Mac Catalyst, so it shares the UIKit adapter with iOS.
The Apple host includes the same C++ renderer header and compiles the same `.cpp`
as Windows. Shared code must not expose UIKit, Objective-C, WinRT or EGL host types.

All handwritten C++ headers use `.hpp`; implementations use `.cpp`. Both `.h` and
`.hpp` are valid C++ header names: the extension is a convention, not a compiler
requirement. Generated C++/WinRT and React Native headers retain `.h` because their
generators define those names. Do not rename generated headers.

The Windows precompiled header is `pch.hpp`. MSBuild generates forwarding headers
for `App.h`, `MainPage.h` and `pch.h` in the ignored
`Generated Files/HeaderCompatibility/` directory. These satisfy fixed includes in
C++/WinRT, XAML and RNW generated code while all handwritten declarations stay in
`.hpp` files. No generator or third-party template is modified.
MSVC force-includes `pch.hpp` for Windows host sources so generated and external
sources consume the renamed precompiled header. The shared renderer opts out of
both the precompiled header and its forced include.

Apple implementation files use `.mm` because they mix Objective-C and C++.
Objective-C++ can include `.hpp` directly, including Objective-C declarations.
Use `.hpp` for Objective-C++ headers in this project as well.
`apple_platform/LegacyOpenGLView.hpp` declares the Apple view and its React-facing
properties. `LegacyOpenGLView.mm` owns context setup, assets, frame scheduling and
drawing; `LegacyOpenGLViewManager.mm` registers the React module and properties.
Pure Objective-C `.m` files cannot include a header containing C++ declarations.

The root `.clang-format` defines four-space indentation, a 100-column limit and
expanded function/control-flow bodies. Run `clang-format -i` on edited native
source files. Keep generated files and third-party code untouched.

Mesh parsing, normalization and edge generation are named CPU helpers in
`ViewerRenderer.cpp`. GPU uploads and draws stay with the renderer implementation.
The platform adapters retain resource loading, frame scheduling, error events and
context recovery because those operations depend on their host APIs.

After this directory migration, regenerate an existing Apple project with
`npm run angle:configure` on macOS. The Expo plugin replaces the old pod path and
removes old source/resource references before registering the new paths.
Windows CLI discovery uses `react-native.config.js` and its explicit
`microsoft_platform` source directory.

Vendored MetalANGLE lives only in ignored `apple_platform/vendor/`. The old
`native/` folder is obsolete after migration; prepare the new artifact and
regenerate the Apple project before removing an old local copy. Xcode migration
removes references by full path so relocated files with the same name survive.
