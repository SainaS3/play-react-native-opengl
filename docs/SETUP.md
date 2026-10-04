# Native iOS setup

Observed environment: Node 23.1.0/npm 10.9.0, Xcode 16.1, CocoaPods 1.16.2, wired iPhone SE (3rd generation) running iOS 17.3.1. `.nvmrc` recommends Node 22 for future shells; the existing Node was used for validation.

This branch requires the built local MetalANGLE XCFramework. See [ANGLE.md](ANGLE.md). `angle:prepare` defaults to the sibling `../angle` checkout; pass another checkout path when needed.

## Device build

```sh
npm ci
npm run angle:prepare
npm run angle:configure
npm run ios:device
```

Select the phone and your development signing identity. Unlock/trust the Mac, enable Developer Mode on the phone, and use your Personal Team. A free account permits local testing; its provisioning lasts seven days. See [Apple's membership comparison](https://developer.apple.com/support/compare-memberships/).

In Xcode → Settings → Accounts, select Personal Team and create an Apple Development certificate via Manage Certificates. If the installed app cannot launch because its developer is untrusted, trust your profile in Settings → General → VPN & Device Management on the phone.

The scripts set `LC_ALL` and `LANG` to UTF-8 for CocoaPods without modifying global shell configuration. The native source/resources/frameworks are added through `plugins/with-native-opengl.js`, so Expo prebuild can reproduce the integration. The local pod embeds and signs the XCFramework. Run `angle:configure` after switching to this branch or replacing the framework: Expo device builds may reuse cached pods even after the Podfile changes. Native directories remain ignored; keep durable changes in `native/` and the plugin.

## Development

```sh
npm start -- --dev-client
```

Open the installed development client on the phone, with phone and Mac on the same network. Changes to JS controls reload through Metro. Changes to Objective-C, shaders, bundled OBJ files or config plugins require another device build. No simulator needs to run.

`npm run ios` starts the development client in a simulator, which first needs a native build. `npm run build:ios` compiles that simulator client. Prefer the physical phone for this OpenGL experiment.

**Expo Go cannot load the custom native view.** Reinstall the development build after removing the prior Three/R3F implementation. Web/Android render an iOS-only notice rather than substitute a different graphics engine.

## Checks

```sh
npm run check
npm run export:ios
npm run ios:device
```

JS export does not validate Objective-C or produce a signed IPA. Native compilation and phone launch are the decisive checks. Native error events report missing shaders/meshes or shader compilation failures in the control UI. Use Xcode/device logs for native crashes.

`npm ci` generates only `src/generated/modelNames.json`; the Xcode plugin bundles all ten original OBJ files from `native/resources/models/` and both original vertex-lighting shaders from `native/resources/shaders/`. No geometry conversion through a third-party engine is needed.

## Historical target

The obsolete Rend source tree, vendored prerelease JavaScript dependencies and original Xcode project were removed. They depended on missing external ReactKit/Rend code and are not build targets for this app. Original assets are retained in `native/resources/`; the old source is available in Git history (for example, commit `6ac8b4c`). The original README is preserved in `ORIGINAL-README.md`.
