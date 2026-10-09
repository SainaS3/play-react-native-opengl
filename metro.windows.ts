import { getDefaultConfig, mergeConfig, type MetroConfig } from '@react-native/metro-config';

import fs from 'node:fs';
import path from 'node:path';
const rnwPath = fs.realpathSync(
  path.resolve(require.resolve('react-native-windows/package.json'), '..'),
);


const config: MetroConfig = {
  maxWorkers: 2,
  resolver: {
    blockList: [
      // Ignore native build outputs while Metro is watching.
      new RegExp(`${path.resolve(__dirname, 'microsoft_platform').replace(/[/\\]/g, '/')}.*`),
      new RegExp(`${rnwPath}/build/.*`),
      new RegExp(`${rnwPath}/target/.*`),
      /.*\.ProjectImports\.zip/,
    ],
    },
  transformer: {
    getTransformOptions: async () => ({
      transform: {
        experimentalImportSupport: false,
        inlineRequires: true,
      },
    }),
  },
};

export default mergeConfig(getDefaultConfig(__dirname), config);
