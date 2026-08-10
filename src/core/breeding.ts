/**
 * Cruza: dos genomas dan uno nuevo.
 *
 * ---
 *
 * LA DECISIÓN CENTRAL: se cruza por GEN, no por bit.
 *
 * Lo intuitivo sería mezclar bit a bit, y está mal. El genoma es un número
 * empaquetado: `hue` son los bits 24 a 31. Si de esos ocho se toman cuatro de
 * cada padre, el resultado no es "un color intermedio" — es un color al azar
 * que no se parece a ninguno de los dos. Lo mismo con cada gen: mezclar bits
 * dentro de un campo destruye su significado.
 *
 * Cruzando por gen, cada campo viene entero de uno de los padres. El hijo tiene
 * los ojos de uno y el color del otro, y eso se VE. Es la diferencia entre que
 * la herencia sea un mecanismo o sea ruido.
 *
 * La mutación sí actúa bit a bit, y ahí es lo correcto: un bit dado vuelta en
 * `hue` corre el tono un poco, uno en `lineage` cambia el linaje entero. Eso es
 * exactamente lo que tiene que hacer una mutación.
 *
 * ---
 *
 * Módulo puro y determinista: los mismos padres con el mismo `nonce` dan
 * siempre el mismo hijo. El azar entra por el `nonce`, que lo elige quien
 * llama, así que la función se puede testear sin trucos.
 */

import { GENOME_LAYOUT } from "./genome.ts";
import { deriveSeed, mulberry32 } from "./rng.ts";
import type { CreatureState } from "./simulation.ts";

/** Cuántos bits se espera que muten, en promedio, sobre los 64. */
const MUTACIONES_ESPERADAS = 2.5;

/** Ocho horas de descanso entre cruzas. */
export const CRUZA_COOLDOWN_MS = 8 * 60 * 60 * 1000;

/**
 * Vínculo mínimo para poder cruzar.
 *
 * Con el tope diario de 12, llegar a 20 lleva por lo menos dos días de
 * atención. Es deliberado: ata la cruza al bucle de cuidado en vez de
 * convertirla en un atajo para saltárselo.
 */
export const CRUZA_MIN_VINCULO = 20;
export const CRUZA_MIN_SALUD = 50;

export type Origen = "A" | "B" | "mutado";

export interface Cruza {
  seed: bigint;
  /** De dónde salió cada gen. Permite mostrar a quién salió el hijo. */
  herencia: Record<string, Origen>;
  /** Cuántos bits mutaron. */
  mutaciones: number;
}

/**
 * Combina dos genomas.
 *
 * Es simétrica: cruzar A con B da lo mismo que cruzar B con A. El par
 * determina el hijo, no el orden en que los elegiste.
 */
export function cruzar(seedA: bigint, seedB: bigint, nonce: number): Cruza {
  // Se ordenan para sembrar el azar, que es lo que la vuelve simétrica.
  const [menor, mayor] = seedA <= seedB ? [seedA, seedB] : [seedB, seedA];
  const rng = mulberry32(deriveSeed(menor, `cruza:${mayor}:${nonce}`));

  const herencia: Record<string, Origen> = {};
  let hijo = 0n;

  for (const field of GENOME_LAYOUT) {
    const mask = ((1n << BigInt(field.bits)) - 1n) << BigInt(field.offset);
    const desde = rng.bool() ? menor : mayor;
    hijo |= desde & mask;
    herencia[field.key] = desde === seedA ? "A" : "B";
  }

  // Mutación, bit a bit. La probabilidad por bit se reparte para que el
  // promedio sea MUTACIONES_ESPERADAS sobre el genoma entero.
  let mutaciones = 0;
  const probabilidad = MUTACIONES_ESPERADAS / 64;
  for (let bit = 0; bit < 64; bit++) {
    if (rng.next() >= probabilidad) continue;
    hijo ^= 1n << BigInt(bit);
    mutaciones++;

    const campo = GENOME_LAYOUT.find((f) => bit >= f.offset && bit < f.offset + f.bits);
    if (campo) herencia[campo.key] = "mutado";
  }

  return { seed: hijo, herencia, mutaciones };
}

export type Elegibilidad = { puede: true } | { puede: false; motivo: string };

/**
 * ¿Esta criatura está en condiciones de cruzar?
 *
 * Se separa de `puedenCruzar` para poder decirle al jugador cuál de las dos es
 * la que no puede, y por qué.
 */
export function elegibilidad(criatura: CreatureState, nowMs: number): Elegibilidad {
  if (criatura.etapa !== "adulto") {
    return { puede: false, motivo: "Todavía no terminó de crecer." };
  }
  if (criatura.letargico) {
    return { puede: false, motivo: "Está en letargo. Primero hay que reconectar con ella." };
  }
  if (criatura.stats.salud < CRUZA_MIN_SALUD) {
    return { puede: false, motivo: "No está lo bastante sana." };
  }
  if (criatura.stats.vinculo < CRUZA_MIN_VINCULO) {
    return { puede: false, motivo: "Todavía no hay suficiente vínculo con ella." };
  }

  const ultima = criatura.ultimaCruzaMs;
  if (ultima !== null && nowMs - ultima < CRUZA_COOLDOWN_MS) {
    const faltanMs = CRUZA_COOLDOWN_MS - (nowMs - ultima);
    const horas = Math.ceil(faltanMs / 3_600_000);
    return { puede: false, motivo: `Necesita descansar. Faltan ${horas} h.` };
  }

  return { puede: true };
}

/** ¿Se puede cruzar este par? */
export function puedenCruzar(a: CreatureState, b: CreatureState, nowMs: number): Elegibilidad {
  if (a.id === b.id) {
    return { puede: false, motivo: "Hace falta otra criatura." };
  }

  const ea = elegibilidad(a, nowMs);
  if (!ea.puede) return ea;

  const eb = elegibilidad(b, nowMs);
  if (!eb.puede) return eb;

  return { puede: true };
}

/** Marca a una criatura como recién cruzada, sin mutar la original. */
export function marcarCruzada(criatura: CreatureState, nowMs: number): CreatureState {
  return { ...criatura, stats: { ...criatura.stats }, ultimaCruzaMs: nowMs };
}

/**
 * A quién salió el hijo, en texto.
 *
 * Cuenta genes, no bits: lo que el jugador percibe es "tiene la cara del padre",
 * y eso se corresponde con campos enteros.
 */
export function describirHerencia(cruza: Cruza): string {
  const valores = Object.values(cruza.herencia);
  const deA = valores.filter((v) => v === "A").length;
  const deB = valores.filter((v) => v === "B").length;

  if (cruza.mutaciones >= 5) return "Salió con bastante de suyo. Mutó mucho.";
  if (deA > deB * 2) return "Salió casi calcado al primero.";
  if (deB > deA * 2) return "Salió casi calcado al segundo.";
  return "Salió una mezcla pareja de los dos.";
}
