import assert from 'node:assert/strict';
import type { ConfigPlugin, ExportedConfig } from 'expo/config-plugins';
import type { MetroConfig } from '@react-native/metro-config';

// The Windows CLI injects platform defaults before loading the project config.
const metro = require('@react-native/metro-config') as typeof import('@react-native/metro-config');
metro.setFrameworkDefaults({ resolver: { platforms: ['ios', 'android', 'windows', 'native'] } });
const config = require('../metro.windows.config.js') as MetroConfig;
assert.ok(config.resolver?.platforms?.includes('windows'));
assert.equal(config.maxWorkers, 2);

const plugin = require('../plugins/with-native-opengl.js') as ConfigPlugin;
assert.equal(typeof plugin, 'function');
const nativeConfig = plugin({ name: 'React OpenGL Lab', slug: 'react-opengl-lab' }) as ExportedConfig;
const iosMods = nativeConfig.mods?.ios;
assert.ok(iosMods && 'podfile' in iosMods);
assert.equal(typeof iosMods.podfile, 'function');
assert.equal(typeof iosMods.xcodeproj, 'function');
console.log('PASS: Windows platform defaults and Expo native plugin registration');
