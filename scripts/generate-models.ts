import { readdir, mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
const root = path.resolve(__dirname, '..');
async function main() {
  const names = (await readdir(path.join(root, 'shared/resources/models')))
    .filter((name) => name.endsWith('.obj'))
    .sort();
  await mkdir(path.join(root, 'src/generated'), { recursive: true });
  await writeFile(path.join(root, 'src/generated/modelNames.json'), JSON.stringify(names));
  console.log(`Listed ${names.length} original OBJ models; native code loads their bundled files.`);
}
main().catch((error: unknown) => {
  console.error(error);
  process.exitCode = 1;
});
