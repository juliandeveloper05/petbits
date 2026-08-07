import { describe, expect, it } from "vitest";
import { BOND_DAILY_CAP, acariciar, alimentar, jugar } from "../src/core/actions.ts";
import {
  type CreatureState,
  LETHARGY_TICKS,
  TICK_MS,
  buildAbsenceDigest,
  createCreature,
  localHour,
  simulate,
} from "../src/core/simulation.ts";

const SEED = 0xa3f091c477be2d08n;
/** 2026-03-02 09:00 UTC. Fijo: la simulación jamás debe leer el reloj real. */
const T0 = Date.UTC(2026, 2, 2, 9, 0, 0);
const MINUTE = TICK_MS;
const HOUR = 60 * MINUTE;
const DAY = 24 * HOUR;

function fresh(tzOffsetMin = 0): CreatureState {
  return createCreature(SEED, T0, tzOffsetMin);
}

/** Suma los resúmenes de varias llamadas, para poder compararlos con uno solo. */
function mergeSummaries(summaries: Record<string, number>[]): Record<string, number> {
  const total: Record<string, number> = {};
  for (const summary of summaries) {
    for (const [kind, count] of Object.entries(summary)) {
      total[kind] = (total[kind] ?? 0) + count;
    }
  }
  return total;
}

describe("invariancia temporal", () => {
  /**
   * El test que define la Fase 2.
   *
   * Si simular de corrido difiere de simular en pedazos, entonces el estado de
   * la criatura depende de con qué frecuencia abriste la pestaña — que es
   * exactamente el defecto que este rediseño vino a arreglar.
   *
   * También caza el error clásico de acumulación en punto flotante: sumar el
   * mismo desgaste 10.080 veces tiene que dar idéntico por los dos caminos.
   */
  it("7 días de corrido dan lo mismo que 10.080 pasos de un minuto", () => {
    const start = fresh();
    const steps = 7 * 24 * 60;

    const oneShot = simulate(start, T0 + steps * MINUTE);

    let stepped = start;
    const summaries: Record<string, number>[] = [];
    for (let i = 1; i <= steps; i++) {
      const result = simulate(stepped, T0 + i * MINUTE);
      stepped = result.state;
      summaries.push(result.summary);
    }

    expect(stepped).toEqual(oneShot.state);
    expect(mergeSummaries(summaries)).toEqual(oneShot.summary);
  });

  it("da lo mismo con pedazos de tamaño irregular", () => {
    const start = fresh();
    const total = 3 * 24 * 60;
    const oneShot = simulate(start, T0 + total * MINUTE);

    // Tamaños coprimos entre sí, para no caer siempre en las mismas fronteras.
    const chunks = [7, 13, 1, 60, 137, 5];
    let stepped = start;
    let elapsed = 0;
    let index = 0;
    while (elapsed < total) {
      const size = Math.min(chunks[index % chunks.length] ?? 1, total - elapsed);
      elapsed += size;
      stepped = simulate(stepped, T0 + elapsed * MINUTE).state;
      index++;
    }

    expect(stepped).toEqual(oneShot.state);
  });

  it("el tiempo avanza solo en ticks enteros y el resto queda pendiente", () => {
    const start = fresh();

    // Medio minuto no alcanza para un tick.
    const partial = simulate(start, T0 + 30_000);
    expect(partial.ticks).toBe(0);
    expect(partial.state.lastTickMs).toBe(T0);
    expect(partial.state.stats).toEqual(start.stats);

    // El sobrante se acumula: a los 90 s ya hay un tick.
    const complete = simulate(partial.state, T0 + 90_000);
    expect(complete.ticks).toBe(1);
    expect(complete.state.lastTickMs).toBe(T0 + MINUTE);
  });

  it("no depende de la zona horaria de la máquina", () => {
    // La hora local sale del desfasaje guardado en el estado, no del sistema.
    // Si saliera del sistema, la misma partida daría distinto en otro país.
    const utc = simulate(fresh(0), T0 + 12 * HOUR);
    const buenosAires = simulate(fresh(-180), T0 + 12 * HOUR);
    expect(utc.state.stats).not.toEqual(buenosAires.state.stats);

    // Pero cada una es reproducible consigo misma.
    expect(simulate(fresh(-180), T0 + 12 * HOUR).state).toEqual(buenosAires.state);
  });
});

describe("ciclo día y noche", () => {
  it("duerme de noche y se despierta de día", () => {
    // A las 09:00 UTC está despierta.
    expect(fresh().durmiendo).toBe(false);

    // A las 23:00 se duerme.
    const night = simulate(fresh(), T0 + 14 * HOUR + MINUTE);
    expect(localHour(night.state.lastTickMs, 0)).toBeGreaterThanOrEqual(23);
    expect(night.state.durmiendo).toBe(true);
    expect(night.summary.durmio).toBeGreaterThan(0);

    // A las 08:00 del día siguiente ya se despertó.
    const morning = simulate(fresh(), T0 + 23 * HOUR);
    expect(morning.state.durmiendo).toBe(false);
    expect(morning.summary.desperto).toBeGreaterThan(0);
  });

  it("durmiendo gasta menos energía", () => {
    // Ocho horas de noche desgastan menos que ocho horas de día.
    const day = simulate(fresh(), T0 + 8 * HOUR).state.stats.energia;
    const startAtNight = createCreature(SEED, Date.UTC(2026, 2, 2, 23, 0, 0), 0);
    const night = simulate(startAtNight, Date.UTC(2026, 2, 3, 7, 0, 0)).state.stats.energia;
    expect(night).toBeGreaterThan(day);
  });
});

describe("letargo", () => {
  it("entra en letargo a las 48 horas sin cuidados", () => {
    const before = simulate(fresh(), T0 + 47 * HOUR);
    expect(before.state.letargico).toBe(false);

    const after = simulate(fresh(), T0 + 49 * HOUR);
    expect(after.state.letargico).toBe(true);
    expect(after.summary.letargo).toBe(1);
    expect(LETHARGY_TICKS).toBe(48 * 60);
  });

  it("la criatura nunca muere: en letargo todo se congela", () => {
    // Es la decisión de retención más importante del juego. Los Tamagotchi
    // digitales mueren porque castigan al jugador por tener vida.
    const twoDays = simulate(fresh(), T0 + 49 * HOUR).state;
    const thirtyDays = simulate(fresh(), T0 + 30 * DAY).state;
    const oneYear = simulate(fresh(), T0 + 365 * DAY).state;

    expect(thirtyDays.stats).toEqual(twoDays.stats);
    expect(oneYear.stats).toEqual(twoDays.stats);
    expect(oneYear.stats.salud).toBeGreaterThan(0);
  });

  it("cualquier cuidado la saca del letargo, con costo de vínculo", () => {
    let state = simulate(fresh(), T0 + 60 * HOUR).state;
    state = { ...state, stats: { ...state.stats, vinculo: 40 } };
    expect(state.letargico).toBe(true);

    const result = acariciar(state, T0 + 60 * HOUR);
    expect(result.ok).toBe(true);
    if (!result.ok) return;

    expect(result.state.letargico).toBe(false);
    expect(result.state.stats.vinculo).toBeLessThan(40);
    expect(result.state.stats.vinculo).toBeGreaterThan(0);
    expect(result.message).toContain("letargo");
  });
});

describe("reloj del sistema", () => {
  it("un reloj hacia atrás no revierte progreso", () => {
    const advanced = simulate(fresh(), T0 + 6 * HOUR).state;
    const rewound = simulate(advanced, T0 - 10 * DAY);

    expect(rewound.ticks).toBe(0);
    expect(rewound.state.stats).toEqual(advanced.stats);
    expect(rewound.state.ticksVividos).toBe(advanced.ticksVividos);
    expect(rewound.summary.reloj).toBe(1);
  });

  it("ticksVividos es monótono", () => {
    let state = fresh();
    let previous = 0;
    for (const hours of [3, 1, 12, 0, 30]) {
      state = simulate(state, state.lastTickMs + hours * HOUR).state;
      expect(state.ticksVividos).toBeGreaterThanOrEqual(previous);
      previous = state.ticksVividos;
    }
  });
});

describe("acciones con costo", () => {
  it("comer de más rinde poco y hace mal", () => {
    const hungry = simulate(fresh(), T0 + 20 * HOUR).state;
    const fed = alimentar(hungry, "larva", T0 + 20 * HOUR);
    expect(fed.ok).toBe(true);
    if (!fed.ok) return;

    // Ya llena, una segunda comida rinde una fracción y cuesta salud.
    const full = { ...fed.state, stats: { ...fed.state.stats, energia: 95, salud: 80 } };
    const again = alimentar(full, "larva", T0 + 20 * HOUR);
    expect(again.ok).toBe(true);
    if (!again.ok) return;

    expect(again.state.stats.energia - 95).toBeLessThan(34 * 0.5);
    expect(again.state.stats.salud).toBeLessThan(80);
    expect(again.message).toContain("llena");
  });

  it("jugar sube el ánimo pero gasta energía", () => {
    const state = fresh();
    const played = jugar(state, T0);
    expect(played.ok).toBe(true);
    if (!played.ok) return;

    expect(played.state.stats.animo).toBeGreaterThan(state.stats.animo);
    expect(played.state.stats.energia).toBeLessThan(state.stats.energia);
  });

  it("sin energía no se puede jugar", () => {
    const exhausted = { ...fresh(), stats: { ...fresh().stats, energia: 5 } };
    const result = jugar(exhausted, T0);
    expect(result.ok).toBe(false);
    if (result.ok) return;
    expect(result.reason).toContain("comer");
  });

  it("el vínculo tiene tope diario: el spam no sirve", () => {
    // La regla que hace que valga más volver todos los días que clickear cien
    // veces hoy.
    let state = fresh();
    for (let i = 0; i < 50; i++) {
      const result = acariciar(state, T0);
      if (result.ok) state = result.state;
    }
    expect(state.stats.vinculo).toBe(BOND_DAILY_CAP);

    // Al día siguiente vuelve a haber margen.
    const nextDay = acariciar(state, T0 + DAY);
    expect(nextDay.ok).toBe(true);
    if (!nextDay.ok) return;
    expect(nextDay.state.stats.vinculo).toBeGreaterThan(BOND_DAILY_CAP);
  });

  it("cuidar reinicia el contador de abandono", () => {
    const neglected = simulate(fresh(), T0 + 40 * HOUR).state;
    expect(neglected.ticksSinCuidado).toBeGreaterThan(0);

    const cared = acariciar(neglected, T0 + 40 * HOUR);
    expect(cared.ok).toBe(true);
    if (!cared.ok) return;
    expect(cared.state.ticksSinCuidado).toBe(0);
  });
});

describe("resumen de ausencia", () => {
  it("no se arma por ausencias cortas", () => {
    expect(buildAbsenceDigest(simulate(fresh(), T0 + 5 * MINUTE))).toBeNull();
  });

  it("cuenta lo que pasó mientras no estabas", () => {
    const digest = buildAbsenceDigest(simulate(fresh(), T0 + 20 * HOUR));
    expect(digest).not.toBeNull();
    if (!digest) return;

    expect(digest.headline).toContain("horas");
    expect(digest.entroEnLetargo).toBe(false);
    expect(digest.highlights.length).toBeGreaterThan(0);
  });

  it("avisa cuando la ausencia terminó en letargo", () => {
    const digest = buildAbsenceDigest(simulate(fresh(), T0 + 5 * DAY));
    expect(digest).not.toBeNull();
    if (!digest) return;

    expect(digest.entroEnLetargo).toBe(true);
    expect(digest.headline).toContain("días");
  });

  it("los eventos recortados se informan, no se ocultan", () => {
    const result = simulate(fresh(), T0 + 40 * HOUR);
    const totalInSummary = Object.values(result.summary).reduce((a, b) => a + b, 0);
    expect(result.events.length + result.omitted).toBe(totalInSummary);
  });
});
