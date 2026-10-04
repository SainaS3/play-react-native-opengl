# Work log — October 4, 2026

**Current implementation:** React Native controls over an Objective-C GLKView/OpenGL ES 2 scene. Three.js and React Three Fiber have been removed at the user's direction. Earlier sections below preserve the work history, including the superseded approach; current instructions are in SETUP.md and ARCHITECTURE.md.

## Request and approach

Investigate an old fork, set it up with the current Node environment, document how it works, and brainstorm a React/GPU custom renderer for a painting app. User clarified **iOS / React Native first**.

Read the linked article and inspected the actual source, package scripts, vendored libraries, native project references, host Node/npm/Xcode and available simulator runtimes. Kept historical source intact. Added a separate root Expo app instead of presenting an incomplete historical dependency upgrade as a working fix. The original README is preserved verbatim in `ORIGINAL-README.md`.

## Reproduced historical failures

1. `npm start` inside `Rend Example Collection/js`: exits 127, missing `./packager/packager.sh`.
2. `xcodebuild -project 'Rend Example Collection.xcodeproj' -scheme TeapotExample -sdk iphonesimulator -configuration Debug CODE_SIGNING_ALLOWED=NO build`: exits 65, deprecated i386 architecture; iOS 5.1 deployment warning.
3. Same build with `ARCHS=arm64 IPHONEOS_DEPLOYMENT_TARGET=15.1`: exits 65, missing external shader resources (`/Users/khoi/Shaders/sRESpriteBatch.vsh` and `.fsh`). These were command-line diagnostic overrides, not changes committed to the old project.
4. Project inspection confirms external ReactKit project/header paths and Rend `Classes`/`Shaders` references. The exact native dependencies are not included.

## Added files and behavior

- Root package manifest/lockfile, Expo app config, root registration, TypeScript config, `.nvmrc`, generated-output ignores.
- Expo SDK 54-compatible GL, asset, filesystem, Metro runtime and safe-area dependencies selected with `expo install`.
- OBJ conversion script; deterministic sorted collection of all ten original models.
- Platform adapters for native/browser R3F, a declarative model scene and native controls.
- Model selection/search/random, material colors/wireframe, timed rotation/flight/reset. Deliberately removed historical fake model entries.
- Explicit geometry cleanup plus renderer-managed material children. No React state updates in animation loop.
- Tests for frame-rate independent motion, resume delta cap, and every asset's finite/nonzero triangle geometry.
- Setup, architecture and painter/custom renderer design notes with primary-source links.

## Verification

| Check | Result |
| --- | --- |
| Install using existing Node 23.1.0/npm 10.9.0 | Passed; lockfile created and prepare generated assets |
| `npx expo install --check` | Dependencies up to date |
| `npm ls --depth=0` | Expected SDK 54 package family installed |
| `npm run check` | Passed |
| `npm test` | 3 tests passed, including all 10 meshes |
| `npm run export:ios` | Passed, Hermes bundle, 623 modules, about 11.5 MB; not a native binary/signing check |
| `npm run export:web` | Passed, about 7.5 MB JS; separate `dist/web` directory |
| Browser visual check | Cone rendered; switched to teapot, paused rotation, enabled wireframe; no captured warning/error logs during initial scene check |
| iOS simulator launch | Native runtime verification remains incomplete; see below |
| Historical native target | Fails for documented architecture and missing upstream resources |

The simulator selected was iPhone 16 / iOS 18.1 (`13C4F680-DD4F-4B69-82EB-77850F63D480`). Its first boot stayed in migration/on the Apple progress screen for several minutes. Expo downloaded SDK-compatible Expo Go but installation waited on the simulator. A non-erasing shutdown/boot was attempted; the interrupted install reported an invalid device state / Mach server-died error. A fresh launch after restart reached Metro's “Opening” step, but the final simulator screenshot still showed the Apple boot progress screen. This is simulator startup evidence, not proof that the native graphics app rendered. No device data was erased, no signing was performed, no App Store build submitted, and physical-device rendering is still required.

`npm audit` reports 30 advisories (7 moderate, 23 high) in the selected dependency graph, including Expo/RN tooling chains. Automatic suggestions include incompatible major changes/downgrades. No `audit fix --force` was applied. This is an experiment baseline; SDK/native toolchain and advisories need review before production use.

## Practical limitations and next work

All model triangles are currently bundled (~6.2 MB generated JSON), original MTL/textures and authored smooth normals are not retained, flight can move offscreen until Reset, and graphics behavior is not yet validated on a physical iPhone. Native `expo run:ios` compilation requires CocoaPods/signing setup beyond the JS export check. The painter notebook is a proposal; no brush canvas or new reconciler was implemented.

Next useful step: run the lab on a physical device with a development build, then build one pressure-aware stroke/layer surface using Skia, measure input latency, and only then decide whether a domain-specific reconciler adds value.

## Connected iPhone follow-up

At the user's request, shut down the iPhone 16 simulator and confirmed no simulators remained booted. Detected a wired iPhone SE (3rd generation) running iOS 17.3.1. Initially unpaired; after the user unlocked/trusted it, `devicectl manage pair` succeeded. Developer Mode is enabled. CoreDevice currently reports a developer disk image mount issue, so debug services may need recovery even after signing is ready.

Added SDK-compatible `expo-dev-client`, generated the ignored native `ios/` workspace, and added `npm run ios:device`. CocoaPods failed initially because the execution environment forces `LC_ALL=C`; setting both `LC_ALL` and `LANG` to `en_US.UTF-8` resolved it. Pod installation completed: 84 Podfile dependencies, 83 pods. Build scripts now apply that locale locally. Expo prebuild changed npm scripts; restored `ios` to explicitly start simulator Expo Go and kept a separate physical-device command.

The first device build stopped with “No code signing certificates are available to use.” User is setting up an Apple account/development certificate in Xcode. Independently ran `xcodebuild` against `ios/ReactOpenGLLab.xcworkspace`, scheme `ReactOpenGLLab`, Debug/iphoneos, generic iOS destination and `CODE_SIGNING_ALLOWED=NO`. **BUILD SUCCEEDED** on Xcode 16.1. The unsigned app is at `/tmp/react-opengl-device-build/Build/Products/Debug-iphoneos/ReactOpenGLLab.app`; it cannot be installed on iPhone until signed. At the final check the keychain still reported zero valid signing identities. No device app installation or render has yet been verified.

Adding the development client changed the npm audit count to 33 advisories (10 moderate, 23 high). Signing certificates and account credentials are not checked into the repo.

## Signed device build retry

After the user signed into Xcode and created a development certificate, one valid Apple Development signing identity became available. Expo selected the user's Personal Team. `LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 npx expo run:ios --device <connected-iPhone-UDID>` completed the signed build (**BUILD SUCCEEDED**) and installed `dev.experiment.reactopengllab` on the iPhone SE. `devicectl device info apps` confirms React OpenGL Lab version 1.0.0 is installed.

The initial launch was denied by iOS with a developer-profile trust/security message. Asked the user to trust their development profile under Settings → General → VPN & Device Management. Started Metro with `expo start --dev-client --lan` for the installed development build. Physical-device rendering is pending that trust step and the next launch. The simulator remains shut down.

After the user trusted the profile, the app launched and connected to Metro. Native development mode exposed `TypeError: performance.clearMeasures is not a function` in R3F 9.8.1's bundled reconciler. Pinned R3F to 9.4.0 (reconciler 0.31), restarted Metro and relaunched. The error disappeared; TypeScript and all three tests still passed. **The user confirmed that both the rotating cone and selected teapot render on the iPhone.** This supersedes the earlier pending physical-render status.

Added `metro.config.js` to resolve all `three` imports/requires to a single build, addressing the duplicate Three engine warning. Re-exported native/web bundles and relaunched for final verification. The signed native development client remains installed; Metro is left running for JS changes. Local signing uses a free Personal Team; its profile needs renewal/reinstallation after seven days.

Final checks: device relaunch succeeded; Metro bundled 655 development modules without the prior runtime error or duplicate-Three warning. Production iOS export passed (620 modules, ~10.6 MB Hermes), web export passed (219 modules, ~7.44 MB JS), and `git diff --check` passed. No simulators are booted.

## Correction: preserve the original native OpenGL concept

User clarified that Three.js was outside the intended experiment. Removed Three.js, React Three Fiber and Three types from the root package/lockfile, removed scene JSX/platform renderer adapters, Three-specific Metro resolution, geometry conversion, and the superseded JS animation implementation/tests. `npm ls three @react-three/fiber @types/three` returns an empty dependency tree.

Added `native/LegacyOpenGLView.m`: an Objective-C native view manager, GLKView with OpenGL ES 2 EAGLContext, original lighting shaders, native OBJ parsing and VBO upload, orthographic projection, native display-link animation, control property updates, error events and GPU cleanup. React Native wraps the view and renders controls; it does not reconcile or animate the GPU scene. Geometry is bundled as the original files, not serialized through JS. The generator now writes only the model filename list.

Added a reproducible Xcode config plugin registering native sources, all ten OBJ assets, original shaders and GLKit/OpenGLES frameworks. Initial plugin/build checks caught a missing Xcode virtual resource group and the writer's literal undefined group path; fixed the plugin and regenerated. TypeScript passed and iOS JS export passed (~1.73 MB Hermes), independently of the native build. Rewrote README/setup/architecture and narrowed the brainstorm to future work using the existing native separation.

This is an adaptation of the original architecture; the missing prerelease ReactKit/Rend engine sources remain missing. The replacement controller/view is native Objective-C and uses actual OpenGL ES. Expo supplies app development tooling, not the graphics renderer. Expo Go cannot host the custom view; the development client must be rebuilt.

The signed native build succeeded and was installed on the connected iPhone. Device logs confirmed native OBJ loading (cone: 186 vertices, cow-parts: 11,133, cow: 6,849). The user confirmed that models render and Rotate/Pause, Fly/Reset and Wireframe work in the native viewer. Compiler warnings include Apple's OpenGL ES/GLKit deprecations. Removed the unused `expo-gl` dependency as well, rebuilt the client, and restarted Metro with a cleared cache. TypeScript validation passed after dependency cleanup.

## ANGLE / Metal branch integration — 2026-10-04

User requested the supplied MetalANGLE artifact on their `angle-es3` branch. Initial branch state was clean at `6ac8b4c`. Read the local DARWIN_ES3_METAL_IOS_HANDOFF.md and checked the actual MGLKit headers/implementation. The ANGLE checkout is on `darwin-es3-metal` at `ec925142e`, matching the handoff.

Added a local vendored-framework pod and reproducible preparation/configuration scripts. The artifact stays in ignored `native/vendor/`; verified all 77 copied framework files against the handoff's SHA-256 manifest. The preparation script checks supplied hashes before copying. The Expo plugin removes app links to Apple GLKit/OpenGLES and registers the local pod. CocoaPods embeds/signs the selected XCFramework slice.

Replaced EAGLContext/GLKView with MGLContext/MGLKView at the iOS platform boundary. MGLKit forbids subclassing its view, so the existing registered React view now owns an MGLKView child/delegate. Requests GLES3 while retaining the same ES2-compatible GLSL sources, OBJ loading, VBOs, uniforms, draw calls, animation and React control props. Replaced GLKit math dependency with a small column-major math header; standalone checks validated translation/rotation/projection multiplication and face-normal math. TypeScript and script syntax checks passed.

The first build reused cached pods and could not find MGLKit.h. Explicit pod installation fixed it; `angle:configure` now makes this step reproducible. The next device launch caught null GL strings: MGLKit drawable-format setters release/unbind the EGL surface. Rebinding after drawable configuration fixed initialization. Errors are surfaced through the existing native error overlay.

Final device build succeeded, 0 errors / 55 warnings, signed and installed on the connected iPhone SE (3rd generation). Verified the embedded MetalANGLE framework signature and the app's direct dynamic dependencies: MetalANGLE is linked; Apple GLKit/OpenGLES are absent. Symbol inspection confirms glGetString/glGenBuffers/glDrawArrays resolve to MetalANGLE.

Device log evidence:

```text
ANGLE backend: Google Inc. | ANGLE (Metal Renderer: Apple A15 GPU) | OpenGL ES 3.0.0 (ANGLE 2.1.0.ec925142edeb)
Native OpenGL loaded cone.obj (186 vertices)
ANGLE first frame: 686x440, framebuffer=0, GL error=0x0
```

This verifies the supplied Metal backend is running and the first draw produced no GLES error. Visual/control confirmation is separate from that runtime evidence. The artifact's ES3 conformance limit from the handoff remains applicable; this work does not run an ES conformance suite. Only the physical-device integration was executed; the simulator slice was packaged but not launched.

The user confirmed that models render and Rotate/Pause, Fly/Reset and Wireframe all work with the ANGLE/Metal build. Subsequent device logs show all ten original meshes loaded during the user's interaction, including teapot (18,960 vertices), capsule (30,600) and jlongster (29,988). Metro remains running for JS edits; native/backend edits require rebuilding the installed client.

## Explain the ANGLE hosting boundary

Added GRAPHICS-CONCEPT.md at the user's request, explaining React controls → native GLES renderer → ANGLE → Metal, the difference between graphics API translation and iOS context/drawable hosting, required versus optional initializer changes, the observed context-unbind issue, native frame/presentation lifecycle, and the boundary for a future painter/custom reconciler. Updated the source-map diagram and linked the concept from README/ANGLE/painter notes. This documentation update makes no runtime code changes; checked against the actual initializer and supplied MGLKit source.
# Mac Catalyst follow-up — 2026-10-05

On `desktop-mac-catalyst`, enabled Catalyst in the durable Expo config plugin and React Native CocoaPods post-install hook. Replaced the ignored framework copy with the combined handoff XCFramework and added platform checks to preparation. The React UI and native rendering source are unchanged.

Release build succeeded for arm64/x86_64; both app binaries report MACCATALYST. The app launched on Apple M2, reported the ANGLE Metal backend and ES3 context, and drew its first 1298×613 frame with no GL error. Runtime logs recorded subsequent loading of all ten models. TypeScript and iOS JS export passed. Intel execution and a fresh iOS native build remain untested; desktop visual inspection was unavailable because the UI tool timed out. See [CATALYST.md](CATALYST.md) for commands and evidence.

## Remove obsolete Rend folders — 2026-10-05

On the user's `cleanup` branch, removed `Rend Example Collection/` and `Rend Example Collection.xcodeproj/`, including the obsolete native example, prerelease JS code and tracked vendored node_modules. Preserved all ten OBJ/MTL pairs in `native/resources/models/` and both active lighting shaders in `native/resources/shaders/`, byte-for-byte with license comments. MTL companions are retained as original asset metadata; the viewer still uses its selected diffuse color.

Updated the model-list generator and Xcode resource plugin to use the new locations. The plugin also removes stale Rend resource references when prebuilding an existing generated Xcode project. Current docs explain that the old source is available in Git history; earlier log sections remain historical records. Rendering code and ANGLE integration are unchanged.

Validation: model-list generation found all ten models; TypeScript passed; Expo iOS prebuild succeeded with 12 relocated resource references and no stale Rend references. The Mac Catalyst release build succeeded, and all ten OBJ files and both shaders in its app bundle match the relocated files byte-for-byte. No new device installation was needed for this resource-path cleanup.

At the user's request, subsequently opened the rebuilt Catalyst app and rebuilt/installed/launched the iPhone client to check the cleanup on both platforms. The iOS build succeeded with 0 errors / 58 warnings. Mac logs report `ANGLE (Metal Renderer: Apple M2)`, cone loading and a 1163×613 first frame with GL error `0x0`; iPhone logs report `ANGLE (Metal Renderer: Apple A15 GPU)`, cone loading and a 686×440 first frame with GL error `0x0`. The UI inspection tool timed out, so these are process/device-log checks; a visual/control check was requested from the user. Both apps and Metro were left running.

The user confirmed both viewers work after checking model selection, Rotate/Pause, Fly/Reset and Wireframe. This confirms visible rendering and control behavior after removing the old Rend directories.
