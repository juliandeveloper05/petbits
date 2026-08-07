/**
 * Genera los iconos de la PWA con el propio generador de criaturas.
 *
 * El icono de la app no es un dibujo aparte: es una criatura de PetBits, salida
 * del mismo código que las del juego. Cambiar el seed de acá abajo cambia la
 * mascota del ícono.
 *
 *   npm run iconos
 */

import { mkdirSync, writeFileSync } from "node:fs";
import { formatSeed } from "../src/core/genome.ts";
import { SPRITE_SIZE, generateSprite } from "../src/render/spriteGen.ts";
import { Sheet } from "./png.ts";

/** La criatura del ícono. Elegida a mano entre las de la hoja de contactos. */
const ICON_SEED = 0x7b3c91a4d5e60218n;

/**
 * Dibuja el ícono centrado, con margen.
 *
 * `padding` es la fracción de lado que queda libre a cada costado. Android
 * recorta los iconos "maskable" hasta un círculo inscripto, así que esos
 * necesitan bastante más aire o le come las orejas a la criatura.
 */
function renderIcon(size: number, padding: number): Uint8Array {
  const usable = size * (1 - padding * 2);
  const scale = Math.max(1, Math.floor(usable / SPRITE_SIZE));
  const drawn = SPRITE_SIZE * scale;
  const offset = Math.round((size - drawn) / 2);

  const sheet = new Sheet(size, size);
  sheet.blit(generateSprite(ICON_SEED, "adulto", "guardian"), offset, offset, scale);
  return sheet.toPng();
}

mkdirSync("public", { recursive: true });

const icons: [string, number, number][] = [
  ["public/icon-192.png", 192, 0.06],
  ["public/icon-512.png", 512, 0.06],
  // Maskable: Android recorta hasta el círculo inscripto del 80% central.
  ["public/icon-512-maskable.png", 512, 0.18],
  ["public/favicon.png", 64, 0.02],
];

for (const [path, size, padding] of icons) {
  writeFileSync(path, renderIcon(size, padding));
  console.log(`${path}  ${size}×${size}`);
}
console.log(`\ncriatura del ícono: ${formatSeed(ICON_SEED)}`);
