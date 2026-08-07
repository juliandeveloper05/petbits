/**
 * Simulación por marca de tiempo.
 *
 * El error de diseño más grave de la versión anterior era que el tiempo corría
 * con `setInterval`: la criatura solo existía mientras mirabas la pantalla. Era
 * un idle que no idleaba, y por eso nunca había motivo para volver al día
 * siguiente.
 *
 * Acá el tiempo es real. El estado guarda la frontera del último tick procesado
 * y al abrir se recalcula todo lo que pasó mientras no estabas.
 *
 * ---
 *
 * INVARIANTE CENTRAL: simular un intervalo de una sola vez tiene que dar
 * exactamente lo mismo que simularlo en pedazos.
 *
 * Se sostiene con tres reglas:
 *
 * 1. El tiempo avanza SOLO en ticks enteros de 1 minuto. Lo que sobra queda
 *    pendiente para la próxima llamada, así que la secuencia de ticks es la
 *    misma se llame como se llame.
 * 2. Cada tick lee la hora de su propia frontera, nunca de `Date.now()`.
 * 3. El azar se siembra por índice de tick, no por un flujo que se arrastra.
 *    Un PRNG compartido daría resultados distintos según en cuántos pedazos se
 *    haya simulado.
 *
 * Ese invariante está testeado en `test/simulation.test.ts` comparando 7 días
 * de corrido contra 10.080 pasos de un minuto.
 */

import { decodeGenome } from "./genome.ts";
import { deriveSeed, mulberry32 } from "./rng.ts";

/** Un tick = un minuto de tiempo real. */
export const TICK_MS = 60_000;
const HOUR_MS = 3_600_000;
const DAY_MS = 86_400_000;

/** Sin atención, a las 48 horas la criatura entra en letargo. */
export const LETHARGY_TICKS = (48 * HOUR_MS) / TICK_MS;

const NIGHT_START_HOUR = 23;
const NIGHT_END_HOUR = 7;

/** Techo de eventos devueltos. Lo que se recorta se informa en el resumen. */
const MAX_EVENTS = 60;

// Desgaste por tick, interpolado según el gen de metabolismo (0-7).
// Aletargado agota 100 puntos de energía en ~48 h; Frenético, en ~12 h.
const ENERGY_DRAIN_MIN = 100 / ((48 * HOUR_MS) / TICK_MS);
const ENERGY_DRAIN_MAX = 100 / ((12 * HOUR_MS) / TICK_MS);
const MOOD_DRAIN = 100 / ((30 * HOUR_MS) / TICK_MS);
const HEALTH_DECAY = 0.012;
const HEALTH_RECOVER = 0.004;

const LOW_ENERGY = 25;
const LOW_MOOD = 25;
const LOW_HEALTH = 30;

export interface Stats {
  energia: number;
  animo: number;
  salud: number;
  vinculo: number;
}

export interface CreatureState {
  /** Genoma serializado en decimal: JSON no sabe representar un bigint. */
  seed: string;
  nacimientoMs: number;
  /** Frontera del último tick procesado. Siempre múltiplo de TICK_MS desde el nacimiento. */
  lastTickMs: number;
  /** Contador monótono de ticks vividos. Nunca baja, ni con el reloj hacia atrás. */
  ticksVividos: number;
  /**
   * Minutos de desfasaje horario, guardados en el estado.
   *
   * La hora local NO se lee del sistema al simular: si lo hiciera, la misma
   * partida daría resultados distintos en otra zona horaria y el invariante de
   * composición se rompería.
   */
  tzOffsetMin: number;
  stats: Stats;
  /** Vínculo ganado en el día en curso, para poder toparlo. */
  vinculoHoy: number;
  /** Índice de día local, para saber cuándo reiniciar `vinculoHoy`. */
  diaIndice: number;
  ticksSinCuidado: number;
  letargico: boolean;
  durmiendo: boolean;
}

export type SimEventKind =
  | "hambre"
  | "animo"
  | "salud"
  | "durmio"
  | "desperto"
  | "letargo"
  | "hallazgo"
  | "reloj";

export interface SimEvent {
  kind: SimEventKind;
  atMs: number;
  text: string;
}

export interface SimResult {
  state: CreatureState;
  /** Eventos del intervalo, recortados a MAX_EVENTS (los más recientes). */
  events: SimEvent[];
  /** Cuántos eventos hubo de cada tipo, sin recortar. */
  summary: Record<string, number>;
  /** Eventos descartados por el techo. Se informa, no se oculta. */
  omitted: number;
  ticks: number;
}

// ---------------------------------------------------------------------------
// Utilidades puras
// ---------------------------------------------------------------------------

function clamp01to100(value: number): number {
  return value < 0 ? 0 : value > 100 ? 100 : value;
}

/** Módulo que siempre devuelve un resultado no negativo. */
function mod(value: number, divisor: number): number {
  return ((value % divisor) + divisor) % divisor;
}

export function localHour(ms: number, tzOffsetMin: number): number {
  return Math.floor(mod(ms + tzOffsetMin * 60_000, DAY_MS) / HOUR_MS);
}

export function localDayIndex(ms: number, tzOffsetMin: number): number {
  return Math.floor((ms + tzOffsetMin * 60_000) / DAY_MS);
}

function isNight(hour: number): boolean {
  return hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR;
}

/** Crea el estado inicial de una criatura recién nacida. */
export function createCreature(seed: bigint, nowMs: number, tzOffsetMin: number): CreatureState {
  return {
    seed: seed.toString(),
    nacimientoMs: nowMs,
    lastTickMs: nowMs,
    ticksVividos: 0,
    tzOffsetMin,
    stats: { energia: 70, animo: 70, salud: 100, vinculo: 0 },
    vinculoHoy: 0,
    diaIndice: localDayIndex(nowMs, tzOffsetMin),
    ticksSinCuidado: 0,
    letargico: false,
    durmiendo: isNight(localHour(nowMs, tzOffsetMin)),
  };
}

function energyDrainFor(metabolism: number): number {
  return ENERGY_DRAIN_MIN + (metabolism / 7) * (ENERGY_DRAIN_MAX - ENERGY_DRAIN_MIN);
}

// ---------------------------------------------------------------------------
// Motor
// ---------------------------------------------------------------------------

/**
 * Avanza la simulación hasta `nowMs`.
 *
 * Devuelve un estado nuevo: no muta el que recibe.
 */
export function simulate(state: CreatureState, nowMs: number): SimResult {
  const events: SimEvent[] = [];
  const summary: Record<string, number> = {};

  const emit = (kind: SimEventKind, atMs: number, text: string): void => {
    summary[kind] = (summary[kind] ?? 0) + 1;
    events.push({ kind, atMs, text });
  };

  // Reloj hacia atrás. Es lo único que se puede detectar del lado del cliente:
  // un salto hacia ADELANTE es indistinguible de haber estado ausente de
  // verdad, y no se puede impedir sin un servidor. El letargo limita el
  // beneficio de intentarlo, porque estando ausente no se acumula nada bueno.
  if (nowMs < state.lastTickMs) {
    emit("reloj", state.lastTickMs, "El reloj del sistema retrocedió. No se pierde progreso.");
    return {
      state: { ...state, stats: { ...state.stats } },
      events,
      summary,
      omitted: 0,
      ticks: 0,
    };
  }

  const ticks = Math.floor((nowMs - state.lastTickMs) / TICK_MS);
  if (ticks === 0) {
    return {
      state: { ...state, stats: { ...state.stats } },
      events,
      summary,
      omitted: 0,
      ticks: 0,
    };
  }

  const genes = decodeGenome(BigInt(state.seed));
  const energyDrain = energyDrainFor(genes.metabolism);
  const eventBase = deriveSeed(BigInt(state.seed), "eventos");

  const next: CreatureState = { ...state, stats: { ...state.stats } };

  for (let i = 0; i < ticks; i++) {
    // Cada tick lee la hora de SU frontera, no de `nowMs`. Es lo que permite
    // que simular por pedazos dé el mismo resultado.
    const tickStart = next.lastTickMs;
    const hour = localHour(tickStart, next.tzOffsetMin);
    const night = isNight(hour);

    // Reinicio diario del tope de vínculo.
    const day = localDayIndex(tickStart, next.tzOffsetMin);
    if (day !== next.diaIndice) {
      next.diaIndice = day;
      next.vinculoHoy = 0;
    }

    // Ciclo de sueño.
    if (night && !next.durmiendo) {
      next.durmiendo = true;
      emit("durmio", tickStart, "Se acurrucó a dormir.");
    } else if (!night && next.durmiendo) {
      next.durmiendo = false;
      emit("desperto", tickStart, "Se despertó y estiró las patas.");
    }

    next.ticksSinCuidado++;

    if (!next.letargico && next.ticksSinCuidado >= LETHARGY_TICKS) {
      next.letargico = true;
      emit(
        "letargo",
        tickStart,
        "Entró en letargo. Su deterioro se detuvo, pero perdió parte del vínculo.",
      );
    }

    // En letargo TODO se congela. Es deliberado: la criatura nunca muere, y así
    // los ticks de una ausencia larga son operaciones nulas — se pueden recorrer
    // sin cambiar nada y sin costo real.
    if (!next.letargico) {
      const prev = { ...next.stats };

      let drain = energyDrain;
      if (night && next.durmiendo) drain *= 0.4;
      next.stats.energia = clamp01to100(next.stats.energia - drain);

      let moodDrain = MOOD_DRAIN;
      if (next.stats.energia < 20) moodDrain *= 2;
      if (night && next.durmiendo) moodDrain = 0;
      next.stats.animo = clamp01to100(next.stats.animo - moodDrain);

      if (next.stats.energia <= 0 || next.stats.animo <= 0) {
        next.stats.salud = clamp01to100(next.stats.salud - HEALTH_DECAY);
      } else if (next.stats.energia > 30 && next.stats.animo > 30) {
        next.stats.salud = clamp01to100(next.stats.salud + HEALTH_RECOVER);
      }

      // Eventos por cruce de umbral: se avisa al cruzar, no en cada tick por
      // debajo, para no inundar el registro.
      if (prev.energia >= LOW_ENERGY && next.stats.energia < LOW_ENERGY) {
        emit("hambre", tickStart, "Empezó a tener hambre.");
      }
      if (prev.animo >= LOW_MOOD && next.stats.animo < LOW_MOOD) {
        emit("animo", tickStart, "Se está aburriendo.");
      }
      if (prev.salud >= LOW_HEALTH && next.stats.salud < LOW_HEALTH) {
        emit("salud", tickStart, "No se lo ve bien.");
      }

      // Hallazgos: azar sembrado por índice de tick, nunca por un flujo
      // arrastrado. Con un PRNG compartido, simular en dos pedazos daría otra
      // secuencia que simular de corrido.
      const rng = mulberry32((eventBase ^ Math.imul(next.ticksVividos, 0x9e3779b1)) >>> 0);
      // ~1 hallazgo cada 4 horas despierta. Calibrado con `npm run simular`:
      // más raro que esto y una ausencia de una tarde vuelve con el registro
      // vacío, que es justo el momento en que el "mientras no estabas" tiene
      // que tener algo para contar.
      if (!next.durmiendo && rng.next() < 0.004) {
        emit("hallazgo", tickStart, rng.pick(FINDINGS));
      }
    }

    next.lastTickMs += TICK_MS;
    next.ticksVividos++;
  }

  const omitted = Math.max(0, events.length - MAX_EVENTS);
  return {
    state: next,
    events: omitted > 0 ? events.slice(-MAX_EVENTS) : events,
    summary,
    omitted,
    ticks,
  };
}

const FINDINGS: readonly string[] = [
  "Encontró algo brillante y lo escondió.",
  "Persiguió una sombra por un rato largo.",
  "Se quedó mirando la pared. Nadie sabe por qué.",
  "Descubrió un rincón nuevo.",
  "Estornudó tres veces seguidas.",
  "Ordenó sus cosas. A su manera.",
];

// ---------------------------------------------------------------------------
// Resumen de ausencia
// ---------------------------------------------------------------------------

export interface AbsenceDigest {
  /** Duración de la ausencia en milisegundos. */
  elapsedMs: number;
  /** Texto de encabezado, ya redactado. */
  headline: string;
  /** Lo más relevante que pasó, en orden cronológico. */
  highlights: SimEvent[];
  entroEnLetargo: boolean;
}

function formatDuration(ms: number): string {
  const minutes = Math.floor(ms / 60_000);
  if (minutes < 60) return `${minutes} minuto${minutes === 1 ? "" : "s"}`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours} hora${hours === 1 ? "" : "s"}`;
  const days = Math.floor(hours / 24);
  return `${days} día${days === 1 ? "" : "s"}`;
}

/**
 * Arma el "mientras no estabas".
 *
 * Es la pieza de retención más barata que tiene el juego: convierte una ausencia
 * en algo que se lee al volver, en vez de en un contador que bajó.
 */
export function buildAbsenceDigest(result: SimResult): AbsenceDigest | null {
  const elapsedMs = result.ticks * TICK_MS;
  // Por debajo de 10 minutos no hay nada que contar.
  if (result.ticks < 10) return null;

  const entroEnLetargo = (result.summary.letargo ?? 0) > 0;
  // Se incluyen dormir y despertar: en una ausencia de unas horas suelen ser lo
  // único que pasó, y un registro vacío no le sirve a nadie.
  const interesting: SimEventKind[] = [
    "letargo",
    "hambre",
    "animo",
    "salud",
    "hallazgo",
    "durmio",
    "desperto",
  ];
  const highlights = result.events.filter((event) => interesting.includes(event.kind)).slice(0, 6);

  const headline = entroEnLetargo
    ? `Pasaron ${formatDuration(elapsedMs)}. Te esperó hasta que pudo.`
    : `Pasaron ${formatDuration(elapsedMs)} desde tu última visita.`;

  return { elapsedMs, headline, highlights, entroEnLetargo };
}
