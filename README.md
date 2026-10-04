# React Native controls over native OpenGL ES

An iOS-first adaptation of [James Long's original experiment](https://archive.jlongster.com/First-Impressions-using-React-Native). React Native builds the controls; an Objective-C OpenGL ES view loads and animates the original OBJ models. No Three.js, React Three Fiber, or JavaScript scene engine.

The historical `Rend Example Collection/` and `.xcodeproj` are preserved. Their exact prerelease ReactKit/Rend dependencies are missing, so the runnable app adapts that same UI-over-native-graphics concept using current React Native tooling. It does not claim to restore the missing Rend engine.

This app uses the local MetalANGLE fork to translate GLES to Metal on iOS and Mac Catalyst. The native view requests an ES3 context; the existing ES2-compatible draw logic and shaders remain intact. See [ANGLE integration](docs/ANGLE.md) for artifact preparation and verification.

## Run on Mac Catalyst

```sh
npm ci
npm run angle:prepare
npm run angle:configure
npm run build:catalyst
npm run catalyst
```

Uses the same React UI and native renderer. The Release app bundles JavaScript and runs without Metro. See [Catalyst setup](docs/CATALYST.md) for the framework requirement and validation.

## Run on iPhone

```sh
npm ci
npm run angle:prepare
npm run angle:configure
npm run ios:device
```

This builds the native view and installs the development client. Select your connected phone and Personal Team when prompted. A free Apple account works; trust the developer profile on the phone after initial installation.

For later JS-only edits:

```sh
npm start -- --dev-client
```

Open the installed React OpenGL Lab on the phone. Native edits require rebuilding with `npm run ios:device`. Expo Go cannot load this custom Objective-C view. Web/Android do not implement the graphics view.

## What runs where

```text
React Native controls → native view properties/commands
                      → Objective-C mesh/controller state
                      → GLES shaders and buffers
                      → MetalANGLE → Metal → GPU
```

The native `CADisplayLink` advances rotation/flight and draws frames without React or JavaScript animation callbacks. The OBJ files and original lighting shaders are bundled as native resources. Controls: model search/selection/random, rotation, flight/reset, diffuse color and wireframe.

- [Setup](docs/SETUP.md)
- [Graphics concept and why init changed](docs/GRAPHICS-CONCEPT.md)
- [Architecture and source map](docs/ARCHITECTURE.md)
- [Custom renderer/painter concepts](docs/RENDERER-IDEAS.md)
- [Work log](docs/WORKLOG.md)
- [Original README](docs/ORIGINAL-README.md)

`npm run check` checks TypeScript. `npm run export:ios` checks the JS bundle only; `npm run ios:device` validates native compilation/signing/installation. Check upstream licensing before distributing inherited models/shaders.
