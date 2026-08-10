/**
 * Expediciones: la criatura sale a buscar cosas y vuelve más tarde.
 *
 * Son las dos mitades que le faltaban al juego:
 *
 * 1. El lado de la OFERTA de la economía. Con inventario pero sin forma de
 *    conseguir comida, quedarse sin nada sería un callejón sin salida.
 * 2. Un motivo concreto para volver mañana. Algo que corre mientras no estás y
 *    te espera cuando volvés.
 *
 * ---
 *
 * LA REGLA QUE EVITA EL BLOQUEO: el patio siempre está disponible.
 *
 * Es la trampa clásica de una economía cerrada: si la comida se acaba y para
 * conseguir más hace falta comida, el jugador queda trabado sin nada que hacer.
 * El patio no pide etapa, no cuesta energía y siempre trae algo. Es aburrido a
 * propósito — es un piso, no una estrategia. Los destinos que valen la pena
 * piden criatura crecida y energía.
 */

import type { Stage } from "./evolution.ts";
import type { Inventario } from "./inventory.ts";
import { deriveSeed, mulberry32 } from "./rng.ts";
// `Expedicion` se declara junto al estado de la criatura, no acá: si viviera en
// este módulo, simulation.ts tendría que importarlo y quedaría un ciclo.
import type { CreatureState, Expedicion } from "./simulation.ts";

export type { Expedicion };

const MINUTO = 60_000;
const HORA = 60 * MINUTO;

export interface Destino {
  id: string;
  nombre: string;
  /**
   * Cómo se dice "volvió de ___".
   *
   * Va escrito y no armado concatenando: "de" + "el patio" da "de el patio".
   * La contracción del castellano no sale de pegar strings.
   */
  desde: string;
  descripcion: string;
  duracionMs: number;
  costoEnergia: number;
  etapaMinima: Stage;
  /** Cuántos alimentos trae, mínimo y máximo. */
  items: readonly [number, number];
  /** Peso relativo de cada alimento en el sorteo. */
  pesos: Readonly<Record<string, number>>;
  /** Probabilidad de volver con una semilla. */
  chanceSemilla: number;
}

const ORDEN_ETAPA: Record<Stage, number> = { bebe: 0, juvenil: 1, adulto: 2 };

export const DESTINOS: readonly Destino[] = [
  {
    id: "patio",
    nombre: "El patio",
    desde: "del patio",
    descripcion: "Da una vuelta por acá nomás. Siempre trae algo.",
    duracionMs: 15 * MINUTO,
    // Sin costo y sin etapa mínima: es la salida que evita quedar trabado.
    costoEnergia: 0,
    etapaMinima: "bebe",
    items: [1, 2],
    pesos: { baya: 3, raiz: 2 },
    chanceSemilla: 0,
  },
  {
    id: "bosque",
    nombre: "El bosque",
    desde: "del bosque",
    descripcion: "Un rato largo entre los árboles. A veces encuentra semillas.",
    duracionMs: 90 * MINUTO,
    costoEnergia: 15,
    etapaMinima: "juvenil",
    items: [2, 3],
    pesos: { larva: 3, raiz: 3, baya: 2 },
    chanceSemilla: 0.18,
  },
  {
    id: "ruinas",
    nombre: "Las ruinas",
    desde: "de las ruinas",
    descripcion: "Lejos y pesado. Vuelve con cosas que no se ven en otro lado.",
    duracionMs: 4 * HORA,
    costoEnergia: 30,
    etapaMinima: "adulto",
    items: [3, 5],
    pesos: { cristal: 2, larva: 3, raiz: 2, baya: 1 },
    chanceSemilla: 0.55,
  },
];

export function destinoPorId(id: string): Destino | undefined {
  return DESTINOS.find((d) => d.id === id);
}

export interface Botin {
  alimentos: Inventario;
  /** Genoma encontrado, para incubar. `null` si no trajo ninguno. */
  semilla: bigint | null;
}

export type PuedeSalir = { puede: true } | { puede: false; motivo: string };

export function estaFuera(criatura: CreatureState): boolean {
  return criatura.expedicion !== null;
}

export function puedeSalir(criatura: CreatureState, destino: Destino, _nowMs: number): PuedeSalir {
  if (estaFuera(criatura)) return { puede: false, motivo: "Ya está afuera." };
  if (criatura.letargico) return { puede: false, motivo: "Está en letargo." };

  if (ORDEN_ETAPA[criatura.etapa] < ORDEN_ETAPA[destino.etapaMinima]) {
    return { puede: false, motivo: "Todavía está muy chica para ir tan lejos." };
  }
  if (criatura.stats.energia < destino.costoEnergia) {
    return { puede: false, motivo: "No le da la energía. Primero que coma algo." };
  }
  return { puede: true };
}

/** Manda a la criatura. No muta la que recibe. */
export function enviar(criatura: CreatureState, destino: Destino, nowMs: number): CreatureState {
  return {
    ...criatura,
    stats: {
      ...criatura.stats,
      energia: Math.max(0, criatura.stats.energia - destino.costoEnergia),
    },
    expedicion: {
      destinoId: destino.id,
      salidaMs: nowMs,
      regresoMs: nowMs + destino.duracionMs,
    },
  };
}

export function yaVolvio(criatura: CreatureState, nowMs: number): boolean {
  return criatura.expedicion !== null && nowMs >= criatura.expedicion.regresoMs;
}

/** Cuánto falta para que vuelva, en milisegundos. */
export function faltaParaVolver(criatura: CreatureState, nowMs: number): number {
  if (!criatura.expedicion) return 0;
  return Math.max(0, criatura.expedicion.regresoMs - nowMs);
}

/**
 * Qué trae de vuelta.
 *
 * Determinista a partir del genoma y del momento de salida, así que el botín
 * queda decidido cuando sale, no cuando volvés a mirar. Sin eso, cerrar y
 * reabrir hasta que salga un cristal sería la estrategia óptima.
 */
export function resolverBotin(seed: bigint, destino: Destino, salidaMs: number): Botin {
  const rng = mulberry32(deriveSeed(seed, `expedicion:${destino.id}:${salidaMs}`));

  const opciones = Object.entries(destino.pesos);
  const pesoTotal = opciones.reduce((suma, [, peso]) => suma + peso, 0);

  const [minimo, maximo] = destino.items;
  const cantidad = rng.range(minimo, maximo);

  const alimentos: Inventario = {};
  for (let i = 0; i < cantidad; i++) {
    let tirada = rng.next() * pesoTotal;
    for (const [id, peso] of opciones) {
      tirada -= peso;
      if (tirada <= 0) {
        alimentos[id] = (alimentos[id] ?? 0) + 1;
        break;
      }
    }
  }

  // Semilla de 64 bits armada con dos tiradas de 32.
  const semilla =
    rng.next() < destino.chanceSemilla
      ? (BigInt(rng.int(0x100000000)) << 32n) | BigInt(rng.int(0x100000000))
      : null;

  return { alimentos, semilla };
}

export interface Regreso {
  criatura: CreatureState;
  destino: Destino;
  botin: Botin;
}

/**
 * Recibe a la criatura si ya volvió.
 *
 * Devuelve `null` si sigue afuera o si nunca salió. Volver cuenta como
 * atención: estuvo trabajando para vos, no abandonada.
 */
export function recibir(criatura: CreatureState, nowMs: number): Regreso | null {
  const expedicion = criatura.expedicion;
  if (!expedicion || nowMs < expedicion.regresoMs) return null;

  const destino = destinoPorId(expedicion.destinoId);
  if (!destino) {
    // Un destino que ya no existe (por ejemplo, tras una actualización del
    // juego). Se la trae de vuelta con las manos vacías en vez de dejarla
    // atrapada afuera para siempre.
    return {
      criatura: { ...criatura, expedicion: null, ticksSinCuidado: 0 },
      destino: DESTINOS[0] as Destino,
      botin: { alimentos: {}, semilla: null },
    };
  }

  return {
    criatura: { ...criatura, expedicion: null, ticksSinCuidado: 0 },
    destino,
    botin: resolverBotin(BigInt(criatura.seed), destino, expedicion.salidaMs),
  };
}

/** Resume el botín en una frase. */
export function describirBotin(botin: Botin, nombres: Record<string, string>): string {
  const partes = Object.entries(botin.alimentos).map(
    ([id, cantidad]) => `${cantidad} ${nombres[id]?.toLowerCase() ?? id}`,
  );
  if (partes.length === 0 && !botin.semilla) return "Volvió con las manos vacías.";

  const comida = partes.length > 0 ? `Trajo ${partes.join(", ")}.` : "No trajo comida.";
  return botin.semilla ? `${comida} Y una semilla desconocida.` : comida;
}
