/**
 * Banco de pruebas de la simulación.
 *
 * Corre una ausencia y muestra en qué quedó la criatura y qué se le cuenta al
 * jugador cuando vuelve. Sirve para ajustar el balance sin abrir el navegador:
 * los números de desgaste se sienten distintos en una tabla que en la teoría.
 *
 *   npm run simular
 *   npm run simular -- --horas 6
 *   npm run simular -- --horas 72 --seed 42
 */

import { alimentar, jugar } from "../src/core/actions.ts";
import { decodeGenome, formatSeed, lineageName, metabolismName } from "../src/core/genome.ts";
import {
  type CreatureState,
  buildAbsenceDigest,
  createCreature,
  localHour,
  simulate,
} from "../src/core/simulation.ts";

function arg(name: string, fallback: number): number {
  const index = process.argv.indexOf(`--${name}`);
  if (index === -1) return fallback;
  const value = Number(process.argv[index + 1]);
  return Number.isFinite(value) ? value : fallback;
}

const HOUR = 3_600_000;
const horas = arg("horas", 6);
const seed = BigInt(arg("seed", 0xa3f091c4));
// Fecha fija: la simulación nunca lee el reloj real, así que esto es reproducible.
const T0 = Date.UTC(2026, 2, 2, 9, 0, 0);
const tz = -180; // Buenos Aires

const genes = decodeGenome(seed);
console.log(`Criatura ${formatSeed(seed)} — ${lineageName(genes)}`);
console.log(`Metabolismo: ${metabolismName(genes)} (${genes.metabolism}/7)`);
console.log(`Arranca 2026-03-02 09:00, huso ${tz / 60}h\n`);

function line(label: string, state: CreatureState): void {
  const { energia, animo, salud, vinculo } = state.stats;
  const hour = String(localHour(state.lastTickMs, tz)).padStart(2, "0");
  const flags = [state.durmiendo ? "durmiendo" : "", state.letargico ? "LETARGO" : ""]
    .filter(Boolean)
    .join(" ");
  console.log(
    `${label.padEnd(22)} ${hour}:00  ` +
      `energía ${energia.toFixed(1).padStart(5)}  ` +
      `ánimo ${animo.toFixed(1).padStart(5)}  ` +
      `salud ${salud.toFixed(1).padStart(5)}  ` +
      `vínculo ${vinculo.toFixed(0).padStart(3)}  ${flags}`,
  );
}

let state = createCreature(seed, T0, tz);
line("recién nacida", state);

// Una sesión de cuidado normal antes de irse.
const fed = alimentar(state, "larva", T0);
if (fed.ok) state = fed.state;
const played = jugar(state, T0);
if (played.ok) state = played.state;
line("después de cuidarla", state);

console.log(`\n--- se cierra la pestaña por ${horas} h ---\n`);

const result = simulate(state, T0 + horas * HOUR);
line("al volver", result.state);

console.log(`\nticks simulados: ${result.ticks}`);
console.log(`eventos: ${JSON.stringify(result.summary)}`);
if (result.omitted > 0) console.log(`(${result.omitted} eventos recortados del listado)`);

const digest = buildAbsenceDigest(result);
console.log("\n=== MIENTRAS NO ESTABAS ===");
if (!digest) {
  console.log("(ausencia demasiado corta para contar algo)");
} else {
  console.log(digest.headline);
  for (const event of digest.highlights) {
    const hour = String(localHour(event.atMs, tz)).padStart(2, "0");
    console.log(`  ${hour}:00  ${event.text}`);
  }
}
