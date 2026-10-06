# ANGLE / Metal integration on angle-es3

The starting branch was `angle-es3`, clean at `6ac8b4c`. The artifact handoff is `/Users/khoi/Desktop/angle/doc/DARWIN_ES3_METAL_IOS_HANDOFF.md`: SainaS3 MetalANGLE fork, `darwin-es3-metal`, commit `ec925142edeb1da3158fd8710ecc6dc2fb1f1f97`, built on 2026-10-04. This is the supplied fork, not a replacement with current upstream ANGLE.

For the layer boundaries, the longer initializer and the frame lifecycle, read [Graphics concept](GRAPHICS-CONCEPT.md).

## Prepare and run

```sh
npm ci
npm run angle:prepare
npm run angle:configure
npm run ios:device
```

`angle:prepare` copies the entire device/simulator/Catalyst XCFramework from the sibling ANGLE checkout to ignored `apple_platform/vendor/`. It requires all three variants. See [Mac Catalyst](CATALYST.md) for the desktop build. For a different checkout:

```sh
npm run angle:prepare -- /path/to/angle
```

`angle:configure` runs Expo prebuild and explicitly installs pods. This matters when switching an existing checkout: `expo run:ios` can reuse its pod cache despite a plugin changing the Podfile. Keep the native signing identity in your local Xcode project; it is not stored in this integration.

The source repo does not contain the framework binary. Another machine needs the handoff build artifact, including both platform slices. The preparation script checks the supplied SHA-256 manifest before copying, and also copies the LICENSE, handoff and package hashes for local reference. Follow the handoff's third-party license requirements if distributing a binary.

## What changed

- `ViewerMetalANGLE.podspec` embeds the vendored dynamic XCFramework through CocoaPods. CocoaPods selects the device/simulator slice and adds Embed & Sign and runtime search paths.
- The Expo plugin adds the local pod and removes stale app links to Apple GLKit/OpenGLES. Khronos GLES headers come from the selected MetalANGLE slice.
- The native React view owns an `MGLKView` child and implements its delegate. MGLKit explicitly says not to subclass `MGLKView`.
- `MGLContext` now requests GLES 2 on both iOS and Mac Catalyst. The old mesh buffers, OBJ parsing, GLSL shaders, uniforms, draw calls and native display-link animation keep their logic.
- `ViewerMath.hpp` provides the small column-major vector/matrix subset previously obtained through GLKit, avoiding that framework and its Apple GLES dependencies.

A header/linker change alone cannot replace Apple's EAGL context and drawable lifecycle. MGLKit provides the corresponding ANGLE surface API; the changes here are at that platform boundary.

React still owns controls, and native code owns the frame loop. No Three.js, Expo GL or JavaScript GPU scene engine is added.

## Verify the actual backend

At context creation the viewer logs `GL_VENDOR`, `GL_RENDERER` and `GL_VERSION`. It rejects a renderer string without `Metal`, rather than silently using another backend. This fork's MGLKit default display selects Metal when available; the device log is the decisive verification.

The first drawable frame logs its pixel dimensions, framebuffer ID and `glGetError`. After configuring drawable formats, the viewer rebinds its EGL context: those MGLKit setters can release/unbind the surface. Context/shader/drawing failures are forwarded to the existing React error overlay. MGLKView owns the framebuffer; the viewer does not assume framebuffer zero or manually present it.

The fork advertises ES 3.0 but maximum conformant ES 2.0. This viewer now requests an ES2 context on every platform. The framework’s ES3 capability remains unused; no ES3 feature or conformance claim is made. Build success alone does not establish visible rendering. Device results are recorded in WORKLOG.md.

## Observed device result

On the connected iPhone SE (3rd generation), the signed app reports `ANGLE (Metal Renderer: Apple A15 GPU)` and `OpenGL ES 3.0.0 (ANGLE 2.1.0.ec925142edeb)`. The cone loaded with 186 vertices; its first 686×440 frame reported GL error `0x0`. The embedded framework signature verifies, and app symbol inspection resolves GLES calls to MetalANGLE. See WORKLOG.md for the validation details and limits.
