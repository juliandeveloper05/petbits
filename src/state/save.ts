/**
 * Formato de guardado: esquema, validación y lectura tolerante a fallos.
 *
 * Todo lo de este archivo es puro, sin IndexedDB, así que se puede testear en
 * Node. La entrada/salida vive aparte, en `persistence.ts`.
 */

import { z } from "zod";
import type { CreatureState } from "../core/simulation.ts";
import { type Migration, type RawSave, SaveVersionError, applyMigrations } from "./migrations.ts";

/** v2 agregó crianza, etapa y forma evolutiva. */
export const SAVE_VERSION = 2;

const StatsSchema = z.object({
  energia: z.number().min(0).max(100),
  animo: z.number().min(0).max(100),
  salud: z.number().min(0).max(100),
  vinculo: z.number().min(0),
});

const CrianzaSchema = z.object({
  dieta: z.object({
    proteina: z.number().min(0),
    dulce: z.number().min(0),
    mineral: z.number().min(0),
    raro: z.number().min(0),
  }),
  juego: z.number().min(0),
  calma: z.number().min(0),
  sumaAnimo: z.number().min(0),
  sumaSalud: z.number().min(0),
  ticksMedidos: z.number().int().min(0),
});

const StageSchema = z.enum(["bebe", "juvenil", "adulto"]);

const FormSchema = z.enum([
  "indefinida",
  "petreo",
  "vaporoso",
  "coloso",
  "guardian",
  "errante",
  "oraculo",
]);

const CreatureStateSchema = z.object({
  // El genoma viaja como decimal en texto: JSON no sabe representar un bigint.
  seed: z.string().regex(/^\d+$/, "El genoma tiene que ser un entero en texto"),
  nacimientoMs: z.number().int(),
  lastTickMs: z.number().int(),
  ticksVividos: z.number().int().min(0),
  tzOffsetMin: z.number().int(),
  stats: StatsSchema,
  vinculoHoy: z.number().min(0),
  diaIndice: z.number().int(),
  ticksSinCuidado: z.number().int().min(0),
  letargico: z.boolean(),
  durmiendo: z.boolean(),
  ticksActivos: z.number().int().min(0),
  etapa: StageSchema,
  forma: FormSchema,
  crianza: CrianzaSchema,
});

export const SaveSchema = z.object({
  version: z.number().int(),
  guardadoMs: z.number().int(),
  criatura: CreatureStateSchema,
});

export type SaveData = z.infer<typeof SaveSchema>;

export function createSave(criatura: CreatureState, nowMs: number): SaveData {
  return { version: SAVE_VERSION, guardadoMs: nowMs, criatura };
}

export type LoadOutcome =
  | { ok: true; save: SaveData }
  | { ok: false; reason: string; recoverable: false };

/**
 * Interpreta un blob guardado.
 *
 * Nunca lanza: un save corrupto tiene que degradar a "empezá de nuevo", no
 * romper la aplicación con una excepción sin capturar en el arranque.
 */
export function parseSave(raw: unknown, chain?: readonly Migration[]): LoadOutcome {
  if (typeof raw !== "object" || raw === null || Array.isArray(raw)) {
    return { ok: false, reason: "El guardado no es un objeto", recoverable: false };
  }

  const candidate = raw as RawSave;
  const version = candidate.version;
  if (typeof version !== "number") {
    return { ok: false, reason: "El guardado no declara versión", recoverable: false };
  }

  let migrated: RawSave;
  try {
    migrated = applyMigrations(candidate, version, SAVE_VERSION, chain);
  } catch (error) {
    const reason = error instanceof SaveVersionError ? error.message : "No se pudo migrar";
    return { ok: false, reason, recoverable: false };
  }

  const parsed = SaveSchema.safeParse(migrated);
  if (!parsed.success) {
    const first = parsed.error.issues[0];
    const where = first?.path.join(".") ?? "?";
    return {
      ok: false,
      reason: `Guardado inválido en "${where}": ${first?.message ?? "desconocido"}`,
      recoverable: false,
    };
  }

  return { ok: true, save: parsed.data };
}
