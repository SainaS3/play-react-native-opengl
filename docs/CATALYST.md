# Mac Catalyst

Branch `desktop-mac-catalyst` starts from the working iOS MetalANGLE integration at `0c38464`. Catalyst uses the same Expo/React Native application, Objective-C++ MGLKView adapter over the shared C++ renderer, OBJ resources, and GLES shaders.

## Prepare, build, launch

```sh
npm ci
npm run angle:prepare
npm run angle:configure
npm run build:catalyst
npm run catalyst
```

The supplied ANGLE checkout must contain the combined `out/darwin-es3-metal/MetalANGLE.xcframework` with device, simulator, and `maccatalyst` slices. Run its iOS build script followed by `bash scripts/local/build-darwin-es3-metal-catalyst.sh` if rebuilding it. Running just its iOS script replaces the combined package with an iOS-only package. Preparation validates the platform metadata and available hash manifest before copying.

The Expo plugin enables Catalyst for the application and React Native CocoaPods post-install patches. Keep these changes in the plugin: `ios/` is generated and ignored. iOS still builds from that same project. This selects UIKit Mac Catalyst, as described in [Apple's Catalyst documentation](https://developer.apple.com/documentation/uikit/mac-catalyst).

The build script uses Release and the host architecture, with signing disabled for the local build. JavaScript is bundled into the app, so Metro is unnecessary. The product is `artifacts/catalyst/DerivedData/Build/Products/Release-maccatalyst/ReactOpenGLLab.app`. For signed distribution, configure your Apple signing identity and entitlements in Xcode and use the My Mac (Mac Catalyst) destination; the local build command does not produce a distribution archive.

Build both architectures when desired:

```sh
npm run build:catalyst -- 'ARCHS=arm64 x86_64'
```

## Validation

Runtime verification must check the actual application window plus `ANGLE backend` and `ANGLE first frame` logs. The ANGLE handoff's pbuffer smoke test alone does not establish that the UIKit drawable and React controls work. The viewer now requests GLES2, matching Windows and iOS; the framework's unused ES3 capability does not change this baseline.

Local verification on 2026-10-05 (Asia/Ho_Chi_Minh), with launch just before midnight:

- Release Xcode build succeeded for arm64 and x86_64. Both executable slices report Mach-O platform `MACCATALYST`, minimum 15.1.
- The app launched and remained running on Apple M2 without Metro. Runtime logs report `ANGLE (Metal Renderer: Apple M2)` and `OpenGL ES 3.0.0 (ANGLE 2.1.0.ec925142edeb)`.
- The first drawable frame was 1298×613 with GL error `0x0`; cone.obj loaded with 186 vertices. Subsequent runtime logs show all ten model resources loaded during interaction.
- TypeScript checks and iOS JavaScript export passed. A new iOS native build and Intel runtime were not tested in this pass.
- Desktop UI inspection timed out, so visual appearance, resizing, and every control were not independently verified by the agent. Runtime rendering and model changes are confirmed by app logs.

Local evidence: `artifacts/catalyst/build.log`, `runtime.log`, and `export-ios.log` (ignored by Git).


Shared-renderer follow-through on 2026-10-06: arm64/x86_64 Release build passed,
and Apple M2 launch reported Metal, cone 186 vertices and a 1163×649 first frame
with GL error 0x0. The same production C++ source passed the Metal smoke test:
`bash scripts/test-shared-renderer-mac.sh`. UI automation timed out; current
visual/control and lifecycle checks remain manual. See
[Shared renderer handoff](SHARED-RENDERER-HANDOFF.md) for full evidence and logs.

The subsequent GLES2 rebuild also passed for both architectures. Fresh Apple M2
launch reports OpenGL ES 2.0.0, cone 186 vertices and a 1163×613 first frame
with GL error 0x0. GLES2 is now the common baseline for Apple and Windows.

Validation after the platform-folder refactor on 2026-10-07: universal Release
build passed; both slices report `MACCATALYST`, minimum OS 15.1. Fresh Apple M2
launch reports GLES2 over Metal, cone with 186 vertices and a 1163×613 first frame
with GL error `0x0`. The shared renderer smoke test passes for all ten models,
wireframe, animation/reset and context recreation. The Apple host now has a
`.hpp` view declaration and a separate `.mm` React manager. Full-path Xcode
migration survives repeated regeneration without deleting relocated sources.
Logs: `artifacts/catalyst/{build,runtime,smoke}-refactor.log`. UI inspection timed
out; visual/control checks, Intel runtime and a fresh iOS native build remain
unverified in this pass.
