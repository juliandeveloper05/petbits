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

import {
  ADULTO_TICKS,
  type Crianza,
  type Form,
  JUVENIL_TICKS,
  MIN_SALUD_EVOLUCION,
  type Stage,
  crianzaInicial,
  formName,
  resolverAdulto,
  resolverJuvenil,
} from "./evolution.ts";
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
  /**
   * Identificador único dentro de la partida.
   *
   * La semilla no alcanza: dos criaturas pueden compartir genoma —al cruzar
   * puede repetirse, o podés adoptar la misma semilla dos veces— y con una
   * colección hay que poder distinguirlas.
   *
   * Se deriva de semilla + momento de nacimiento en vez de sortearse, para que
   * `createCreature` siga siendo pura respecto de sus argumentos.
   */
  id: string;
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
  /**
   * Ticks en los que estuvo activa, sin contar el letargo. Es el reloj que rige
   * la evolución: una criatura abandonada no crece sola.
   */
  ticksActivos: number;
  etapa: Stage;
  forma: Form;
  /** Memoria de cómo la criaste. Decide en qué se convierte. */
  crianza: Crianza;
  /** Cuándo cruzó por última vez. `null` si nunca lo hizo. */
  ultimaCruzaMs: number | null;
}

export type SimEventKind =
  | "hambre"
  | "animo"
  | "salud"
  | "durmio"
  | "desperto"
  | "letargo"
  | "hallazgo"
  | "evolucion"
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

/** Arma el identificador de una criatura. Estable para los mismos argumentos. */
export function creatureId(seed: string, nacimientoMs: number): string {
  return `${seed}-${nacimientoMs}`;
}

/** Crea el estado inicial de una criatura recién nacida. */
export function createCreature(seed: bigint, nowMs: number, tzOffsetMin: number): CreatureState {
  return {
    id: creatureId(seed.toString(), nowMs),
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
    ticksActivos: 0,
    etapa: "bebe",
    forma: "indefinida",
    crianza: crianzaInicial(),
    ultimaCruzaMs: null,
  };
}

/**
 * Copia profunda del estado.
 *
 * `{ ...state }` no alcanza: `stats` y `crianza` son objetos anidados y se
 * compartirían por referencia. La simulación mutaría el estado que recibió, y
 * `simulate` dejaría de ser pura sin que ningún test lo note hasta mucho después.
 */
function cloneState(state: CreatureState): CreatureState {
  return {
    ...state,
    stats: { ...state.stats },
    crianza: { ...state.crianza, dieta: { ...state.crianza.dieta } },
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
    emit("reloj", state.lastTickMs, "El reloj del sistema fue para atrás. No perdiste nada.");
    return {
      state: cloneState(state),
      events,
      summary,
      omitted: 0,
      ticks: 0,
    };
  }

  const ticks = Math.floor((nowMs - state.lastTickMs) / TICK_MS);
  if (ticks === 0) {
    return {
      state: cloneState(state),
      events,
      summary,
      omitted: 0,
      ticks: 0,
    };
  }

  const genes = decodeGenome(BigInt(state.seed));
  const energyDrain = energyDrainFor(genes.metabolism);
  const eventBase = deriveSeed(BigInt(state.seed), "eventos");

  const next = cloneState(state);

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
      emit("durmio", tickStart, "Se hizo un rollito y se durmió.");
    } else if (!night && next.durmiendo) {
      next.durmiendo = false;
      emit("desperto", tickStart, "Se despertó, estiró las patas y bostezó.");
    }

    next.ticksSinCuidado++;

    if (!next.letargico && next.ticksSinCuidado >= LETHARGY_TICKS) {
      next.letargico = true;
      emit(
        "letargo",
        tickStart,
        "Se apagó un poco y entró en letargo. Dejó de empeorar, pero el vínculo aflojó.",
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
        emit("hambre", tickStart, "Le empezó a sonar la panza.");
      }
      if (prev.animo >= LOW_MOOD && next.stats.animo < LOW_MOOD) {
        emit("animo", tickStart, "Se está aburriendo mal.");
      }
      if (prev.salud >= LOW_HEALTH && next.stats.salud < LOW_HEALTH) {
        // Femenino, como el resto del juego: "la criatura".
        emit("salud", tickStart, "No se la ve nada bien.");
      }

      // Crianza: se acumula solo con la criatura activa. Sumar durante el
      // letargo promediaría valores congelados y ensuciaría la rama evolutiva
      // con tiempo en el que no pasó nada.
      next.ticksActivos++;
      next.crianza.sumaAnimo += next.stats.animo;
      next.crianza.sumaSalud += next.stats.salud;
      next.crianza.ticksMedidos++;

      // Crecimiento. Requiere salud mínima: una criatura hecha pelota no
      // evoluciona, primero hay que levantarla.
      if (next.stats.salud >= MIN_SALUD_EVOLUCION) {
        if (next.etapa === "bebe" && next.ticksActivos >= JUVENIL_TICKS) {
          next.etapa = "juvenil";
          next.forma = resolverJuvenil(next.crianza, genes);
          emit("evolucion", tickStart, `Pegó el estirón. Ahora es ${formName(next.forma)}.`);
        } else if (next.etapa === "juvenil" && next.ticksActivos >= ADULTO_TICKS) {
          next.etapa = "adulto";
          next.forma = resolverAdulto(next.crianza, genes, next.forma);
          emit("evolucion", tickStart, `Terminó de crecer. Quedó ${formName(next.forma)}.`);
        }
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

/**
 * Texto de color de los hallazgos.
 *
 * Va en rioplatense, no en español neutro. Un juego hecho acá que habla como
 * un manual traducido suena a producto de nadie; el voseo y el vocabulario
 * propio son parte de la identidad, igual que la consola verde fósforo.
 *
 * La criatura se trata en femenino en todo el juego ("quedó dura", "llena"):
 * si se agrega texto nuevo, mantener la concordancia.
 */
const FINDINGS: readonly string[] = [
  "Encontró algo que brillaba y lo escondió al toque.",
  "Se pasó un rato largo correteando una sombra.",
  "Se quedó dura mirando la pared. Andá a saber qué vio.",
  "Chusmeó un rincón nuevo de arriba a abajo.",
  "Estornudó tres veces seguidas y quedó ofendida.",
  "Acomodó sus cosas. A su manera, digamos.",
  "Le agarró la fiaca y no hizo nada por un buen rato.",
  "Se puso a hablar sola. O eso pareció.",
  "Se peleó con su propia sombra y salió empatada.",
  "Se quedó escuchando algo que solo ella escuchaba.",
  "Movió todo de lugar y después lo dejó igual que antes.",
  "Se metió en un rincón y no quiso salir por un rato.",
];

// ---------------------------------------------------------------------------
// Resumen de ausencia
// ---------------------------------------------------------------------------

/**
 * Cuánto pesa cada tipo de evento en el resumen. Cero significa que no se
 * muestra nunca: el aviso de reloj es información de sistema, no una anécdota.
 */
const EVENT_PRIORITY: Record<SimEventKind, number> = {
  // Evolucionar es lo más importante que le puede pasar: va primero de todo.
  evolucion: 120,
  letargo: 100,
  salud: 80,
  hambre: 60,
  animo: 50,
  hallazgo: 20,
  desperto: 10,
  durmio: 10,
  reloj: 0,
};

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

  // Se selecciona por importancia y recién después se ordena por hora.
  //
  // Tomar los primeros seis en orden cronológico parecía razonable y estaba
  // mal: en una ausencia de varios días, el color del primer día tapaba las
  // noticias del último. Se volvía con "acomodó sus cosas" y sin enterarte de
  // que había entrado en letargo.
  //
  // Dormir y despertar puntúan bajo pero no en cero: cuando no pasó nada más,
  // son lo único que hay para contar, y un registro vacío no le sirve a nadie.
  const seen = new Set<string>();
  const highlights = result.events
    .filter((event) => EVENT_PRIORITY[event.kind] > 0)
    // Los hallazgos salen de una lista corta y se repiten; repetidos en el
    // mismo resumen parecen un bug aunque no lo sean. Se deduplica acá, en la
    // presentación, y no en la simulación: recordar el último hallazgo entre
    // ticks metería estado que rompe la composición por pedazos.
    .filter((event) => {
      if (seen.has(event.text)) return false;
      seen.add(event.text);
      return true;
    })
    .sort((a, b) => EVENT_PRIORITY[b.kind] - EVENT_PRIORITY[a.kind] || a.atMs - b.atMs)
    .slice(0, 6)
    .sort((a, b) => a.atMs - b.atMs);

  // Acá se le habla al jugador, así que va con voseo.
  const headline = entroEnLetargo
    ? `Pasaron ${formatDuration(elapsedMs)}. Te bancó todo lo que pudo.`
    : `Pasaron ${formatDuration(elapsedMs)} desde que te fuiste.`;

  return { elapsedMs, headline, highlights, entroEnLetargo };
}
