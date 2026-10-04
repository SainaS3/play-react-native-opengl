# React Native controls over native OpenGL ES

An iOS-first adaptation of [James Long's original experiment](https://archive.jlongster.com/First-Impressions-using-React-Native). React Native builds the controls; an Objective-C OpenGL ES view loads and animates the original OBJ models. No Three.js, React Three Fiber, or JavaScript scene engine.

The historical `Rend Example Collection/` and `.xcodeproj` are preserved. Their exact prerelease ReactKit/Rend dependencies are missing, so the runnable app adapts that same UI-over-native-graphics concept using current React Native tooling. It does not claim to restore the missing Rend engine.

## Run on iPhone

```sh
npm ci
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
                      → GLKit / OpenGL ES 2 shaders and buffers
                      → GPU
```

The native `CADisplayLink` advances rotation/flight and draws frames without React or JavaScript animation callbacks. The OBJ files and original lighting shaders are bundled as native resources. Controls: model search/selection/random, rotation, flight/reset, diffuse color and wireframe.

- [Setup](docs/SETUP.md)
- [Architecture and source map](docs/ARCHITECTURE.md)
- [Custom renderer/painter concepts](docs/RENDERER-IDEAS.md)
- [Work log](docs/WORKLOG.md)
- [Original README](docs/ORIGINAL-README.md)

`npm run check` checks TypeScript. `npm run export:ios` checks the JS bundle only; `npm run ios:device` validates native compilation/signing/installation. Check upstream licensing before distributing inherited models/shaders.
