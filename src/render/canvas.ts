/**
 * Única capa que toca el DOM. Todo lo de arriba (genoma, paletas, sprites) es
 * puro y corre igual en Node, que es lo que permite testearlo sin navegador.
 */

import type { Sprite } from "./spriteGen.ts";

/**
 * Vuelca un sprite en un canvas, escalado por un entero.
 *
 * El escalado va por `drawImage` con `imageSmoothingEnabled = false`: si se deja
 * el suavizado por defecto, el navegador interpola y el pixel art se convierte
 * en un borrón. El factor tiene que ser entero o aparecen filas de píxeles de
 * distinto grosor.
 */
export function drawSprite(canvas: HTMLCanvasElement, sprite: Sprite, scale: number): void {
  const factor = Math.max(1, Math.round(scale));
  canvas.width = sprite.width * factor;
  canvas.height = sprite.height * factor;

  const ctx = canvas.getContext("2d");
  if (!ctx) throw new Error("No se pudo obtener el contexto 2D del canvas");

  const source = document.createElement("canvas");
  source.width = sprite.width;
  source.height = sprite.height;
  const sourceCtx = source.getContext("2d");
  if (!sourceCtx) throw new Error("No se pudo obtener el contexto 2D intermedio");

  // Se pide el ImageData al propio contexto y se copia el buffer, en vez de
  // usar `new ImageData(...)`: el constructor exige un Uint8ClampedArray
  // respaldado por ArrayBuffer y no acepta el tipo genérico que devuelve
  // el generador.
  const imageData = sourceCtx.createImageData(sprite.width, sprite.height);
  imageData.data.set(sprite.data);
  sourceCtx.putImageData(imageData, 0, 0);

  ctx.imageSmoothingEnabled = false;
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.drawImage(source, 0, 0, canvas.width, canvas.height);
}

/** Crea un canvas nuevo ya dibujado. */
export function spriteToCanvas(sprite: Sprite, scale: number): HTMLCanvasElement {
  const canvas = document.createElement("canvas");
  drawSprite(canvas, sprite, scale);
  return canvas;
}
