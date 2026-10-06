import { cpSync, existsSync, mkdirSync, readFileSync, rmSync } from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const angleRoot = path.resolve(process.argv[2] || path.join(root, '../angle'));
const framework = path.join(angleRoot, 'out/darwin-es3-metal/MetalANGLE.xcframework');
if (!existsSync(framework))
  throw new Error(
    `Missing ${framework}. Build the handoff artifact first or pass the ANGLE checkout to npm run angle:prepare -- /path/to/angle.`,
  );
const metadata = JSON.parse(
  execFileSync('plutil', ['-convert', 'json', '-o', '-', path.join(framework, 'Info.plist')], {
    encoding: 'utf8',
  }),
);
for (const variant of [undefined, 'simulator', 'maccatalyst']) {
  if (
    !metadata.AvailableLibraries.some(
      (slice) => slice.SupportedPlatform === 'ios' && slice.SupportedPlatformVariant === variant,
    )
  ) {
    throw new Error(
      `MetalANGLE is missing the ${variant || 'device'} slice. Run the iOS build followed by scripts/local/build-darwin-es3-metal-catalyst.sh in the ANGLE checkout to produce the combined package.`,
    );
  }
}
const destination = path.join(root, 'apple_platform/vendor');
mkdirSync(destination, { recursive: true });
const hashes = path.join(angleRoot, 'out/darwin-es3-metal/SHA256SUMS.json');
if (existsSync(hashes)) {
  const manifest = JSON.parse(readFileSync(hashes, 'utf8'));
  for (const [name, expected] of Object.entries(manifest)) {
    if (!name.startsWith('MetalANGLE.xcframework/')) continue;
    const actual = createHash('sha256')
      .update(readFileSync(path.join(path.dirname(hashes), name)))
      .digest('hex');
    if (actual !== expected) throw new Error(`Artifact hash mismatch: ${name}`);
  }
}
const frameworkDestination = path.join(destination, 'MetalANGLE.xcframework');
rmSync(frameworkDestination, { recursive: true, force: true });
cpSync(framework, frameworkDestination, { recursive: true });
for (const name of [
  'LICENSE',
  'doc/DARWIN_ES3_METAL_IOS_HANDOFF.md',
  'out/darwin-es3-metal/SHA256SUMS.json',
]) {
  const source = path.join(angleRoot, name);
  if (existsSync(source)) cpSync(source, path.join(destination, path.basename(name)));
}
console.log(`Prepared MetalANGLE from ${angleRoot} in ${destination}`);
