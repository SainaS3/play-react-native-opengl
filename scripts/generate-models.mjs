import { readdir, mkdir, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const root = fileURLToPath(new URL('../', import.meta.url));
const names = (await readdir(path.join(root, 'Rend Example Collection/Resources'))).filter(name => name.endsWith('.obj')).sort();
await mkdir(path.join(root, 'src/generated'), { recursive: true });
await writeFile(path.join(root, 'src/generated/modelNames.json'), JSON.stringify(names));
console.log(`Listed ${names.length} original OBJ models; native code loads their bundled files.`);
