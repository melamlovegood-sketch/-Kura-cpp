// Requires sharp. Optional first argument: directory containing its package.json.
import {createRequire} from 'node:module';
import {readFile, writeFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const require = createRequire(process.argv[2] ? path.join(process.argv[2], 'package.json') : import.meta.url);
const sharp = require('sharp');
const iconDirectory = fileURLToPath(new URL('../resources/icons/', import.meta.url));
const svg = await readFile(path.join(iconDirectory, 'kura.svg'));
const sizes = [16, 20, 24, 32, 48, 64, 128, 256];
const frames = await Promise.all(sizes.map(size => sharp(svg).resize(size, size).png().toBuffer()));
const header = Buffer.alloc(6 + 16 * sizes.length);
header.writeUInt16LE(1, 2);
header.writeUInt16LE(sizes.length, 4);
let offset = header.length;
for (let index = 0; index < sizes.length; index++) {
  const entry = 6 + index * 16;
  header[entry] = sizes[index] % 256;
  header[entry + 1] = sizes[index] % 256;
  header.writeUInt16LE(1, entry + 4);
  header.writeUInt16LE(32, entry + 6);
  header.writeUInt32LE(frames[index].length, entry + 8);
  header.writeUInt32LE(offset, entry + 12);
  offset += frames[index].length;
}
await writeFile(path.join(iconDirectory, 'kura.ico'), Buffer.concat([header, ...frames]));
await writeFile(path.join(iconDirectory, 'kura.png'), frames.at(-1));
