/**
 * Hoja de contactos del generador.
 *
 * Genera una grilla de criaturas en un PNG para poder juzgarlas de un vistazo,
 * a resolución cómoda y sin navegador. Es la herramienta de validación visual
 * de la Fase 1: si acá salen manchas, el generador está mal y no importa nada
 * de lo que venga después.
 *
 *   npm run sheet
 *   npm run sheet -- --seed 42 --cols 10 --rows 6
 *   npm run sheet -- --stage bebe
 *   npm run sheet -- --forma coloso
 *
 * Por defecto usa semillas derivadas de una base fija, así que la hoja es
 * reproducible y sirve para comparar antes/después de tocar el generador.
 */

import { writeFileSync } from "node:fs";
import type { Form } from "../src/core/evolution.ts";
import { splitmix64 } from "../src/core/rng.ts";
import { SPRITE_SIZE, type Stage, generateSprite } from "../src/render/spriteGen.ts";
import { Sheet } from "./png.ts";

function numberArg(name: string, fallback: number): number {
  const index = process.argv.indexOf(`--${name}`);
  if (index === -1) return fallback;
  const value = Number(process.argv[index + 1]);
  return Number.isFinite(value) ? value : fallback;
}

function textArg<T extends string>(name: string, fallback: T): T {
  const index = process.argv.indexOf(`--${name}`);
  if (index === -1) return fallback;
  return (process.argv[index + 1] ?? fallback) as T;
}

const cols = numberArg("cols", 10);
const rows = numberArg("rows", 6);
const scale = numberArg("scale", 4);
const base = BigInt(numberArg("seed", 20260804));
const stage = textArg<Stage>("stage", "adulto");
const forma = textArg<Form>("forma", "indefinida");

const cell = SPRITE_SIZE * scale;
const gap = 6;
const sheet = new Sheet(cols * (cell + gap) + gap, rows * (cell + gap) + gap);

let cursor = base;
for (let row = 0; row < rows; row++) {
  for (let col = 0; col < cols; col++) {
    cursor = splitmix64(cursor);
    sheet.blit(
      generateSprite(cursor, stage, forma),
      gap + col * (cell + gap),
      gap + row * (cell + gap),
      scale,
    );
  }
}

const outIndex = process.argv.indexOf("--out");
const outPath =
  outIndex === -1 ? "contact-sheet.png" : (process.argv[outIndex + 1] ?? "contact-sheet.png");
writeFileSync(outPath, sheet.toPng());
console.log(
  `${cols * rows} criaturas (${stage}/${forma}) → ${outPath} [${sheet.width}×${sheet.height}]`,
);
