/**
 * Entrada/salida del guardado, sobre IndexedDB.
 *
 * Es la única capa de estado que toca el navegador. Toda la lógica de formato,
 * versión y validación vive en `save.ts`, que es puro y se testea en Node.
 *
 * Se usa IndexedDB y no localStorage porque localStorage es síncrono, bloquea
 * el hilo principal y tiene un techo de unos 5 MB. Para la Fase 5, con codex y
 * varias criaturas, se queda corto.
 */

import { del, get, set } from "idb-keyval";
import type { CreatureState } from "../core/simulation.ts";
import { type SaveData, createSave, parseSave } from "./save.ts";

const SAVE_KEY = "petbits:save";
const QUARANTINE_KEY = "petbits:save:corrupto";

export type LoadResult =
  | { status: "ok"; save: SaveData }
  | { status: "vacio" }
  | { status: "corrupto"; reason: string };

/**
 * Lee la partida guardada.
 *
 * Si el blob está corrupto NO se borra: se copia a una clave de cuarentena y se
 * devuelve `corrupto`. Es la diferencia entre poder recuperar la criatura de
 * alguien más adelante y haberla destruido por un bug de serialización nuestro.
 */
export async function loadGame(): Promise<LoadResult> {
  let raw: unknown;
  try {
    raw = await get(SAVE_KEY);
  } catch (error) {
    // Modo incógnito, permisos denegados, cuota agotada.
    return {
      status: "corrupto",
      reason: `No se pudo leer el almacenamiento: ${String(error)}`,
    };
  }

  if (raw === undefined) return { status: "vacio" };

  const outcome = parseSave(raw);
  if (!outcome.ok) {
    try {
      await set(QUARANTINE_KEY, { raw, cuarentenaMs: Date.now(), motivo: outcome.reason });
    } catch {
      // Si ni siquiera se puede escribir la cuarentena, se informa igual el
      // problema original en vez de enmascararlo con este.
    }
    return { status: "corrupto", reason: outcome.reason };
  }

  return { status: "ok", save: outcome.save };
}

export async function saveGame(criatura: CreatureState, nowMs: number): Promise<void> {
  await set(SAVE_KEY, createSave(criatura, nowMs));
}

/** Borra la partida. Deja intacta la cuarentena, por si hay algo que rescatar. */
export async function clearGame(): Promise<void> {
  await del(SAVE_KEY);
}
