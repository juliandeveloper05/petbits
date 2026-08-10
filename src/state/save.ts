/**
 * Formato de guardado: esquema, validación y lectura tolerante a fallos.
 *
 * Todo lo de este archivo es puro, sin IndexedDB, así que se puede testear en
 * Node. La entrada/salida vive aparte, en `persistence.ts`.
 */

import { z } from "zod";
import { type Codex, codexInicial } from "../core/codex.ts";
import { type Inventario, inventarioInicial } from "../core/inventory.ts";
import type { CreatureState } from "../core/simulation.ts";
import { type Migration, type RawSave, SaveVersionError, applyMigrations } from "./migrations.ts";

/**
 * v2 agregó crianza, etapa y forma evolutiva.
 * v3 pasó de una criatura suelta a una colección, con codex.
 * v4 agregó el descanso entre cruzas.
 * v5 agregó despensa, semillas encontradas y expediciones.
 */
export const SAVE_VERSION = 5;

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
  id: z.string().min(1),
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
  ultimaCruzaMs: z.number().int().nullable(),
  expedicion: z
    .object({
      destinoId: z.string().min(1),
      salidaMs: z.number().int(),
      regresoMs: z.number().int(),
    })
    .nullable(),
});

const CodexSchema = z.object({
  linajes: z.array(z.number().int().min(0)),
  formas: z.array(FormSchema),
  rarezas: z.array(z.string()),
  totalRegistradas: z.number().int().min(0),
});

export const SaveSchema = z
  .object({
    version: z.number().int(),
    guardadoMs: z.number().int(),
    criaturas: z.array(CreatureStateSchema).min(1),
    activaId: z.string().min(1),
    codex: CodexSchema,
    inventario: z.record(z.string(), z.number().int().min(0)),
    /** Genomas encontrados en expediciones, en decimal y sin incubar. */
    semillas: z.array(z.string().regex(/^\d+$/)),
  })
  // Que `activaId` apunte a una criatura que no existe dejaría la partida sin
  // nada que mostrar. Se valida acá y no en el juego, donde ya sería tarde.
  .refine((save) => save.criaturas.some((c) => c.id === save.activaId), {
    message: "activaId no corresponde a ninguna criatura",
    path: ["activaId"],
  });

export type SaveData = z.infer<typeof SaveSchema>;

/** El estado completo de una partida. */
export interface GameState {
  criaturas: CreatureState[];
  activaId: string;
  codex: Codex;
  /** La despensa. Es del jugador, no de una criatura. */
  inventario: Inventario;
  /** Genomas encontrados en expediciones, todavía sin incubar. */
  semillas: string[];
}

export function createSave(state: GameState, nowMs: number): SaveData {
  return {
    version: SAVE_VERSION,
    guardadoMs: nowMs,
    criaturas: state.criaturas,
    activaId: state.activaId,
    codex: state.codex,
    inventario: state.inventario,
    semillas: state.semillas,
  };
}

/** Arranca una partida nueva con una sola criatura. */
export function partidaInicial(criatura: CreatureState): GameState {
  return {
    criaturas: [criatura],
    activaId: criatura.id,
    codex: codexInicial(),
    inventario: inventarioInicial(),
    semillas: [],
  };
}

/** La criatura que se está cuidando. */
export function criaturaActiva(state: GameState): CreatureState | undefined {
  return state.criaturas.find((c) => c.id === state.activaId);
}

/** Reemplaza una criatura por su versión actualizada, sin mutar el estado. */
export function reemplazarCriatura(state: GameState, criatura: CreatureState): GameState {
  return {
    ...state,
    criaturas: state.criaturas.map((c) => (c.id === criatura.id ? criatura : c)),
  };
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
