# Mac Catalyst

Branch `desktop-mac-catalyst` starts from the working iOS MetalANGLE integration at `0c38464`. Catalyst uses the same Expo/React Native application, Objective-C MGLKView renderer, OBJ resources, and GLES shaders.

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

Runtime verification must check the actual application window plus `ANGLE backend` and `ANGLE first frame` logs. The ANGLE handoff's pbuffer smoke test alone does not establish that the UIKit drawable and React controls work. The fork's ES3 context still has the handoff's ES2 conformance limit.

Local verification on 2026-10-05 (Asia/Ho_Chi_Minh), with launch just before midnight:

- Release Xcode build succeeded for arm64 and x86_64. Both executable slices report Mach-O platform `MACCATALYST`, minimum 15.1.
- The app launched and remained running on Apple M2 without Metro. Runtime logs report `ANGLE (Metal Renderer: Apple M2)` and `OpenGL ES 3.0.0 (ANGLE 2.1.0.ec925142edeb)`.
- The first drawable frame was 1298×613 with GL error `0x0`; cone.obj loaded with 186 vertices. Subsequent runtime logs show all ten model resources loaded during interaction.
- TypeScript checks and iOS JavaScript export passed. A new iOS native build and Intel runtime were not tested in this pass.
- Desktop UI inspection timed out, so visual appearance, resizing, and every control were not independently verified by the agent. Runtime rendering and model changes are confirmed by app logs.

Local evidence: `artifacts/catalyst/build.log`, `runtime.log`, and `export-ios.log` (ignored by Git).
