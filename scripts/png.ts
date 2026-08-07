/**
 * Codificador PNG mínimo, sin dependencias.
 *
 * Existe para poder mirar sprites a resolución cómoda desde la terminal, sin
 * navegador. Alcanza con filtro 0 y zlib de Node: no hay que optimizar bytes,
 * hay que poder ver las criaturas.
 */

import { deflateSync } from "node:zlib";
import type { Sprite } from "../src/render/spriteGen.ts";

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

export function encodePng(width: number, height: number, rgba: Uint8Array): Uint8Array {
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

/** Lienzo RGBA con fondo oscuro, para que el contorno de las criaturas se lea. */
export class Sheet {
  readonly width: number;
  readonly height: number;
  readonly pixels: Uint8Array;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.pixels = new Uint8Array(width * height * 4);
    for (let i = 0; i < width * height; i++) {
      this.pixels[i * 4] = 10;
      this.pixels[i * 4 + 1] = 14;
      this.pixels[i * 4 + 2] = 10;
      this.pixels[i * 4 + 3] = 255;
    }
  }

  /** Pega un sprite escalado por un entero, ignorando los píxeles transparentes. */
  blit(sprite: Sprite, originX: number, originY: number, scale: number): void {
    for (let y = 0; y < sprite.height; y++) {
      for (let x = 0; x < sprite.width; x++) {
        const src = (y * sprite.width + x) * 4;
        if ((sprite.data[src + 3] ?? 0) === 0) continue;

        for (let dy = 0; dy < scale; dy++) {
          for (let dx = 0; dx < scale; dx++) {
            const px = originX + x * scale + dx;
            const py = originY + y * scale + dy;
            if (px < 0 || py < 0 || px >= this.width || py >= this.height) continue;
            const dest = (py * this.width + px) * 4;
            this.pixels[dest] = sprite.data[src] ?? 0;
            this.pixels[dest + 1] = sprite.data[src + 1] ?? 0;
            this.pixels[dest + 2] = sprite.data[src + 2] ?? 0;
            this.pixels[dest + 3] = 255;
          }
        }
      }
    }
  }

  toPng(): Uint8Array {
    return encodePng(this.width, this.height, this.pixels);
  }
}
