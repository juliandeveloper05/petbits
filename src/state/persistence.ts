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
import { type GameState, type SaveData, SaveSchema, createSave, parseSave } from "./save.ts";

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

/**
 * Guarda la partida, validando ANTES de escribir.
 *
 * Hasta acá solo se validaba al leer, y eso deja pasar el peor caso: escribir
 * un guardado roto y descubrirlo recién la próxima vez que abrís. Apareció de
 * verdad durante el desarrollo — el hot-reload actualizó el número de versión
 * antes que la cadena de migraciones, y el juego escribió un save marcado como
 * nuevo a partir de un estado viejo al que le faltaban campos.
 *
 * TypeScript no puede atrapar eso: el objeto en memoria viene de un guardado
 * anterior y su forma real no la conoce el compilador. Validar al escribir
 * convierte un save silenciosamente corrupto en un error inmediato.
 */
export async function saveGame(state: GameState, nowMs: number): Promise<void> {
  const save = createSave(state, nowMs);

  const check = SaveSchema.safeParse(save);
  if (!check.success) {
    const first = check.error.issues[0];
    throw new Error(
      `Se intentó guardar una partida inválida en "${first?.path.join(".") ?? "?"}": ` +
        `${first?.message ?? "desconocido"}`,
    );
  }

  await set(SAVE_KEY, save);
}

/** Borra la partida. Deja intacta la cuarentena, por si hay algo que rescatar. */
export async function clearGame(): Promise<void> {
  await del(SAVE_KEY);
}
