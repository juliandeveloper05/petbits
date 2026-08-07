/**
 * Comparador de formas evolutivas.
 *
 * Cada fila es una criatura; cada columna, en qué se convierte según cómo la
 * críes. Responde de un vistazo la pregunta que decide si la Fase 3 sirve:
 * ¿se nota la diferencia, o son la misma criatura con otro nombre?
 *
 *   npm run formas
 *   npm run formas -- --filas 8 --escala 6
 */

import { writeFileSync } from "node:fs";
import { ADULT_FORMS, type Form, formName } from "../src/core/evolution.ts";
import { formatSeed } from "../src/core/genome.ts";
import { splitmix64 } from "../src/core/rng.ts";
import { SPRITE_SIZE, generateSprite } from "../src/render/spriteGen.ts";
import { Sheet } from "./png.ts";

function arg(name: string, fallback: number): number {
  const index = process.argv.indexOf(`--${name}`);
  if (index === -1) return fallback;
  const value = Number(process.argv[index + 1]);
  return Number.isFinite(value) ? value : fallback;
}

const filas = arg("filas", 6);
const escala = arg("escala", 5);
const base = BigInt(arg("seed", 20260804));

// La columna "indefinida" va primera como referencia: es la criatura sin la
// deformación de ninguna rama.
const columnas: Form[] = ["indefinida", ...ADULT_FORMS];

const celda = SPRITE_SIZE * escala;
const gap = 8;
const width = columnas.length * (celda + gap) + gap;
const height = filas * (celda + gap) + gap;

const sheet = new Sheet(width, height);

let cursor = base;
const seeds: bigint[] = [];
for (let fila = 0; fila < filas; fila++) {
  cursor = splitmix64(cursor);
  seeds.push(cursor);
  for (let col = 0; col < columnas.length; col++) {
    const forma = columnas[col] ?? "indefinida";
    const sprite = generateSprite(cursor, "adulto", forma);
    sheet.blit(sprite, gap + col * (celda + gap), gap + fila * (celda + gap), escala);
  }
}

const outIndex = process.argv.indexOf("--out");
const outPath = outIndex === -1 ? "formas.png" : (process.argv[outIndex + 1] ?? "formas.png");
writeFileSync(outPath, sheet.toPng());

console.log(`columnas: ${columnas.map(formName).join("  |  ")}`);
for (const seed of seeds) console.log(`  ${formatSeed(seed)}`);
console.log(`→ ${outPath} [${width}×${height}]`);
