/**
 * Hoja de contactos del generador.
 *
 * Genera una grilla de criaturas en un PNG para poder juzgarlas de un vistazo,
 * a resolución cómoda y sin navegador. Es la herramienta de validación visual
 * de la Fase 1: si acá salen manchas, el generador está mal y no importa nada
 * de lo que venga después.
 *
 *   npx vite-node scripts/contactSheet.ts
 *   npx vite-node scripts/contactSheet.ts --seed 42 --cols 10 --rows 6
 *
 * Por defecto usa semillas derivadas de una base fija, así que la hoja es
 * reproducible y sirve para comparar antes/después de tocar el generador.
 */

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { splitmix64 } from "../src/core/rng.ts";
import { SPRITE_SIZE, type Stage, generateSprite } from "../src/render/spriteGen.ts";

// --------------------------------------------------------------------- PNG

const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    table[n] = c >>> 0;
  }
  return table;
})();

function crc32(bytes: Uint8Array): number {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc = ((CRC_TABLE[(crc ^ byte) & 0xff] ?? 0) ^ (crc >>> 8)) >>> 0;
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function chunk(type: string, data: Uint8Array): Uint8Array {
  const typeBytes = new Uint8Array([...type].map((ch) => ch.charCodeAt(0)));
  const body = new Uint8Array(typeBytes.length + data.length);
  body.set(typeBytes, 0);
  body.set(data, typeBytes.length);

  // longitud(4) + [tipo(4) + datos] + crc(4)
  const out = new Uint8Array(4 + body.length + 4);
  const view = new DataView(out.buffer);
  view.setUint32(0, data.length);
  out.set(body, 4);
  view.setUint32(4 + body.length, crc32(body));
  return out;
}

/** Codifica RGBA a PNG. Filtro 0 por scanline: simple y suficiente acá. */
function encodePng(width: number, height: number, rgba: Uint8Array): Uint8Array {
  const raw = new Uint8Array(height * (1 + width * 4));
  for (let y = 0; y < height; y++) {
    raw[y * (1 + width * 4)] = 0;
    raw.set(rgba.subarray(y * width * 4, (y + 1) * width * 4), y * (1 + width * 4) + 1);
  }

  const ihdr = new Uint8Array(13);
  const view = new DataView(ihdr.buffer);
  view.setUint32(0, width);
  view.setUint32(4, height);
  ihdr[8] = 8; // profundidad de bit
  ihdr[9] = 6; // RGBA
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;

  const parts = [
    new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk("IHDR", ihdr),
    chunk("IDAT", new Uint8Array(deflateSync(raw))),
    chunk("IEND", new Uint8Array(0)),
  ];

  const total = parts.reduce((sum, part) => sum + part.length, 0);
  const png = new Uint8Array(total);
  let offset = 0;
  for (const part of parts) {
    png.set(part, offset);
    offset += part.length;
  }
  return png;
}

// ------------------------------------------------------------------- hoja

function parseArg(name: string, fallback: number): number {
  const index = process.argv.indexOf(`--${name}`);
  if (index === -1) return fallback;
  const value = Number(process.argv[index + 1]);
  return Number.isFinite(value) ? value : fallback;
}

const cols = parseArg("cols", 10);
const rows = parseArg("rows", 6);
const scale = parseArg("scale", 4);
const base = BigInt(parseArg("seed", 20260804));
const stage = (
  process.argv.includes("--stage") ? process.argv[process.argv.indexOf("--stage") + 1] : "adulto"
) as Stage;

const cell = SPRITE_SIZE * scale;
const gap = 6;
const width = cols * (cell + gap) + gap;
const height = rows * (cell + gap) + gap;

const canvas = new Uint8Array(width * height * 4);
// Fondo oscuro, para que el contorno de las criaturas se lea.
for (let i = 0; i < width * height; i++) {
  canvas[i * 4] = 10;
  canvas[i * 4 + 1] = 14;
  canvas[i * 4 + 2] = 10;
  canvas[i * 4 + 3] = 255;
}

let cursor = base;
for (let row = 0; row < rows; row++) {
  for (let col = 0; col < cols; col++) {
    cursor = splitmix64(cursor);
    const sprite = generateSprite(cursor, stage);
    const originX = gap + col * (cell + gap);
    const originY = gap + row * (cell + gap);

    for (let y = 0; y < SPRITE_SIZE; y++) {
      for (let x = 0; x < SPRITE_SIZE; x++) {
        const src = (y * SPRITE_SIZE + x) * 4;
        const alpha = sprite.data[src + 3] ?? 0;
        if (alpha === 0) continue;

        for (let dy = 0; dy < scale; dy++) {
          for (let dx = 0; dx < scale; dx++) {
            const px = originX + x * scale + dx;
            const py = originY + y * scale + dy;
            const dest = (py * width + px) * 4;
            canvas[dest] = sprite.data[src] ?? 0;
            canvas[dest + 1] = sprite.data[src + 1] ?? 0;
            canvas[dest + 2] = sprite.data[src + 2] ?? 0;
            canvas[dest + 3] = 255;
          }
        }
      }
    }
  }
}

const outIndex = process.argv.indexOf("--out");
const outPath =
  outIndex === -1 ? "contact-sheet.png" : (process.argv[outIndex + 1] ?? "contact-sheet.png");
writeFileSync(outPath, encodePng(width, height, canvas));
console.log(`${cols * rows} criaturas (${stage}) → ${outPath} [${width}×${height}]`);
