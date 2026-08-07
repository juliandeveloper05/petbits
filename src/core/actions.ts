/**
 * Acciones del jugador.
 *
 * Regla de diseño que corrige el problema central del juego anterior: **toda
 * acción da algo y cobra algo**. Antes, alimentar y jugar eran botones que
 * subían barras sin contrapartida, así que no había ninguna decisión que tomar
 * — solo clickear.
 *
 * Acá comer de más pasa factura, jugar gasta energía, y el vínculo tiene tope
 * diario para que la constancia valga más que el spam.
 */

import type { FoodKind } from "./evolution.ts";
import type { CreatureState } from "./simulation.ts";
import { localDayIndex } from "./simulation.ts";

export type { FoodKind };

export interface Food {
  id: string;
  name: string;
  /** Artículo, para poder armar frases sin que se rompa la concordancia. */
  articulo: "el" | "la";
  energia: number;
  animo: number;
  salud: number;
  /** Alimenta el vector de crianza que en la Fase 3 decide la rama evolutiva. */
  tipo: FoodKind;
}

export const FOODS: readonly Food[] = [
  { id: "baya", name: "Baya", articulo: "la", energia: 18, animo: 5, salud: 0, tipo: "dulce" },
  { id: "raiz", name: "Raíz", articulo: "la", energia: 26, animo: 0, salud: 1, tipo: "mineral" },
  {
    id: "larva",
    name: "Larva",
    articulo: "la",
    energia: 34,
    animo: -2,
    salud: 2,
    tipo: "proteina",
  },
  {
    id: "cristal",
    name: "Cristal",
    articulo: "el",
    energia: 12,
    animo: 12,
    salud: 3,
    tipo: "raro",
  },
];

/** A partir de acá, seguir comiendo rinde poco y empieza a hacer mal. */
const FULL_THRESHOLD = 85;
const OVERFEED_EFFICIENCY = 0.25;
const OVERFEED_HEALTH_COST = 2.5;

const PLAY_ENERGY_COST = 12;
const PLAY_MIN_ENERGY = 15;
const PLAY_MOOD_GAIN = 16;

const PET_MOOD_GAIN = 4;

const BOND_PER_ACTION = 2;
/**
 * Tope diario de vínculo.
 *
 * Es la regla que convierte "abrir la app" en un hábito en vez de en una sesión
 * de farmeo: alcanzado el tope, seguir interactuando no suma más. Premia volver
 * todos los días, no clickear cien veces hoy.
 */
export const BOND_DAILY_CAP = 12;

/** Salir del letargo cuesta vínculo. Se recupera la criatura, no el progreso. */
const LETHARGY_BOND_PENALTY = 0.25;

export type ActionResult =
  | { ok: true; state: CreatureState; message: string }
  | { ok: false; reason: string };

function clamp01to100(value: number): number {
  return value < 0 ? 0 : value > 100 ? 100 : value;
}

/** Copia el estado y aplica lo común a toda interacción. */
function touch(state: CreatureState, nowMs: number): { next: CreatureState; woke: boolean } {
  // Copia profunda: `crianza.dieta` es un objeto anidado y compartirlo por
  // referencia haría que la acción mutara el estado que recibió.
  const next: CreatureState = {
    ...state,
    stats: { ...state.stats },
    crianza: { ...state.crianza, dieta: { ...state.crianza.dieta } },
  };

  // El tope de vínculo se reinicia por día local, igual que en el tick.
  const day = localDayIndex(nowMs, next.tzOffsetMin);
  if (day !== next.diaIndice) {
    next.diaIndice = day;
    next.vinculoHoy = 0;
  }

  const woke = next.letargico;
  if (woke) {
    next.letargico = false;
    next.stats.vinculo = Math.max(0, next.stats.vinculo * (1 - LETHARGY_BOND_PENALTY));
  }
  next.ticksSinCuidado = 0;

  return { next, woke };
}

/** Suma vínculo respetando el tope diario. Devuelve cuánto entró de verdad. */
function grantBond(state: CreatureState, amount: number): number {
  const room = Math.max(0, BOND_DAILY_CAP - state.vinculoHoy);
  const granted = Math.min(amount, room);
  state.vinculoHoy += granted;
  state.stats.vinculo += granted;
  return granted;
}

function withWakeNote(message: string, woke: boolean): string {
  return woke ? `${message} Salió del letargo, pero el vínculo quedó golpeado.` : message;
}

export function alimentar(state: CreatureState, foodId: string, nowMs: number): ActionResult {
  const food = FOODS.find((item) => item.id === foodId);
  if (!food) return { ok: false, reason: `No existe el alimento "${foodId}"` };

  const { next, woke } = touch(state, nowMs);

  // Comer de más: rinde una cuarta parte y encima hace mal. Sin esto,
  // alimentar sería siempre la jugada correcta y no habría decisión.
  const full = next.stats.energia >= FULL_THRESHOLD;
  const gained = full ? food.energia * OVERFEED_EFFICIENCY : food.energia;

  next.stats.energia = clamp01to100(next.stats.energia + gained);
  next.stats.animo = clamp01to100(next.stats.animo + food.animo);
  next.stats.salud = clamp01to100(
    next.stats.salud + food.salud - (full ? OVERFEED_HEALTH_COST : 0),
  );
  grantBond(next, BOND_PER_ACTION);
  // La dieta se registra siempre, aun cuando comió sin ganas: lo que le diste
  // moldea en qué se convierte, más allá de cuánto le rindió esta vez.
  next.crianza.dieta[food.tipo]++;

  const comida = `${food.articulo} ${food.name.toLowerCase()}`;
  const message = full
    ? `Picoteó ${comida} sin ganas. Ya estaba llena.`
    : `Se morfó ${comida} sin respirar.`;

  return { ok: true, state: next, message: withWakeNote(message, woke) };
}

export function jugar(state: CreatureState, nowMs: number): ActionResult {
  if (state.stats.energia < PLAY_MIN_ENERGY) {
    return { ok: false, reason: "No le da la energía para jugar. Primero tiene que comer algo." };
  }

  const { next, woke } = touch(state, nowMs);

  // Jugar decaído rinde la mitad: la salud baja se nota en todo.
  const efficiency = next.stats.salud < 30 ? 0.5 : 1;
  next.stats.animo = clamp01to100(next.stats.animo + PLAY_MOOD_GAIN * efficiency);
  next.stats.energia = clamp01to100(next.stats.energia - PLAY_ENERGY_COST);
  grantBond(next, BOND_PER_ACTION);
  next.crianza.juego++;

  const message =
    efficiency < 1
      ? "Jugó un rato pero se cansó enseguida, pobre."
      : "Jugó hasta quedar rendida de contenta.";

  return { ok: true, state: next, message: withWakeNote(message, woke) };
}

export function acariciar(state: CreatureState, nowMs: number): ActionResult {
  const { next, woke } = touch(state, nowMs);

  next.stats.animo = clamp01to100(next.stats.animo + PET_MOOD_GAIN);
  const granted = grantBond(next, BOND_PER_ACTION);
  // Cuenta como crianza aunque el vínculo ya esté topeado: el tope limita el
  // vínculo, no el hecho de haber estado ahí.
  next.crianza.calma++;

  // Cuando el tope ya está alcanzado se dice, en vez de fingir que sumó.
  const message =
    granted > 0
      ? "Se dejó hacer mimos un buen rato."
      : "Está a gusto, pero por hoy ya tuvo lo suyo.";

  return { ok: true, state: next, message: withWakeNote(message, woke) };
}
