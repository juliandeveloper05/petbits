/**
 * Buffers de píxeles sin dependencias del DOM.
 *
 * Que esto no toque `document` ni `canvas` es lo que permite generar y testear
 * criaturas enteras en Node, dentro de Vitest, sin navegador ni mocks.
 */

import type { Rgb } from "../core/palette.ts";

/** Máscara booleana: define qué píxeles son "cuerpo" antes de pintarlos. */
export class Mask {
  readonly width: number;
  readonly height: number;
  private readonly cells: Uint8Array;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.cells = new Uint8Array(width * height);
  }

  has(x: number, y: number): boolean {
    if (x < 0 || y < 0 || x >= this.width || y >= this.height) return false;
    return this.cells[y * this.width + x] === 1;
  }

  set(x: number, y: number): void {
    if (x < 0 || y < 0 || x >= this.width || y >= this.height) return;
    this.cells[y * this.width + x] = 1;
  }

  /** Marca el píxel y su espejo respecto del eje vertical. */
  setMirrored(x: number, y: number): void {
    this.set(x, y);
    this.set(this.width - 1 - x, y);
  }

  clear(x: number, y: number): void {
    if (x < 0 || y < 0 || x >= this.width || y >= this.height) return;
    this.cells[y * this.width + x] = 0;
  }

  /** ¿Tiene al menos un vecino de 4 direcciones dentro de la máscara? */
  touchesBody(x: number, y: number): boolean {
    return this.has(x - 1, y) || this.has(x + 1, y) || this.has(x, y - 1) || this.has(x, y + 1);
  }

  count(): number {
    let total = 0;
    for (const cell of this.cells) total += cell;
    return total;
  }
}

/** Lienzo RGBA. 4 bytes por píxel, igual que `ImageData`. */
export class PixelBuffer {
  readonly width: number;
  readonly height: number;
  readonly data: Uint8ClampedArray;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.data = new Uint8ClampedArray(width * height * 4);
  }

  set(x: number, y: number, color: Rgb, alpha = 255): void {
    if (x < 0 || y < 0 || x >= this.width || y >= this.height) return;
    const i = (y * this.width + x) * 4;
    this.data[i] = color.r;
    this.data[i + 1] = color.g;
    this.data[i + 2] = color.b;
    this.data[i + 3] = alpha;
  }

  /** Igual que `set`, pero también pinta el píxel espejado. */
  setMirrored(x: number, y: number, color: Rgb, alpha = 255): void {
    this.set(x, y, color, alpha);
    this.set(this.width - 1 - x, y, color, alpha);
  }

  isOpaque(x: number, y: number): boolean {
    if (x < 0 || y < 0 || x >= this.width || y >= this.height) return false;
    return (this.data[(y * this.width + x) * 4 + 3] ?? 0) > 0;
  }
}

/**
 * Hash FNV-1a del contenido de un buffer.
 *
 * Se usa en los tests de determinismo: dos sprites del mismo seed tienen que dar
 * exactamente el mismo hash, en cualquier máquina.
 */
export function hashPixels(data: Uint8ClampedArray): string {
  let hash = 0x811c9dc5;
  for (let i = 0; i < data.length; i++) {
    hash ^= data[i] ?? 0;
    hash = Math.imul(hash, 0x01000193) >>> 0;
  }
  return hash.toString(16).padStart(8, "0");
}
