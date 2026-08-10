/**
 * Evolución ramificada.
 *
 * En la versión anterior había tres etapas lineales y después nada: la mascota
 * llegaba a "adulto" y el juego se terminaba sin decirlo. Peor todavía, el
 * camino era siempre el mismo, así que el jugador no decidía nada.
 *
 * Acá el árbol se abre: bebé → 2 juveniles → 4 adultos. Y la rama NO la decide
 * el azar, la decide cómo la criaste. Con el mismo genoma, una criatura
 * alimentada a proteína y mineral termina distinta a una criada con dulce y
 * cristal.
 *
 * El genoma sigue pesando: el gen de afinidad sesga la balanza, así que la
 * misma crianza puede inclinarse para lados distintos según la criatura. El
 * jugador es coautor, no dueño.
 *
 * Módulo puro: no importa el estado de la criatura, solo recibe el acumulado de
 * crianza y los genes. Así se evita el ciclo simulation → evolution → actions →
 * simulation.
 */

import type { Genes } from "./genome.ts";

export type Stage = "bebe" | "juvenil" | "adulto";

export type Form =
  | "indefinida"
  | "petreo"
  | "vaporoso"
  | "coloso"
  | "guardian"
  | "errante"
  | "oraculo";

export type FoodKind = "proteina" | "dulce" | "mineral" | "raro";

/**
 * Acumulado de crianza.
 *
 * Es la memoria de qué le fuiste haciendo a la criatura. Se llena desde las
 * acciones (dieta, juego, calma) y desde el tick (promedios de ánimo y salud).
 */
export interface Crianza {
  dieta: Record<FoodKind, number>;
  /** Veces que jugaste con ella. */
  juego: number;
  /** Veces que la acariciaste. */
  calma: number;
  /** Suma de ánimo por tick activo, para poder promediar. */
  sumaAnimo: number;
  /** Suma de salud por tick activo. */
  sumaSalud: number;
  /** Ticks contados. En letargo no se cuenta: los valores están congelados. */
  ticksMedidos: number;
}

export function crianzaInicial(): Crianza {
  return {
    dieta: { proteina: 0, dulce: 0, mineral: 0, raro: 0 },
    juego: 0,
    calma: 0,
    sumaAnimo: 0,
    sumaSalud: 0,
    ticksMedidos: 0,
  };
}

/**
 * Umbrales de evolución, en ticks ACTIVOS.
 *
 * Se cuentan activos y no vividos a propósito: una criatura en letargo no
 * crece. Si contáramos el tiempo total, abandonarla una semana la haría
 * evolucionar sola, que es exactamente el mensaje contrario al del juego.
 */
export const JUVENIL_TICKS = 24 * 60;
export const ADULTO_TICKS = 4 * 24 * 60;

/** Con la salud por el piso no evoluciona: primero hay que recuperarla. */
export const MIN_SALUD_EVOLUCION = 40;

/**
 * Sesgo por afinidad, en el eje cuerpo (positivo) ↔ etéreo (negativo).
 *
 * Es lo que hace que el genoma siga importando: dos criaturas criadas igual
 * pueden ramificar distinto. El índice es el gen `affinity` (0-7), en el mismo
 * orden que AFFINITIES en genome.ts:
 * Brasa, Marea, Raíz, Chispa, Escarcha, Polvo, Eco, Vacío.
 */
const AFFINITY_BIAS: readonly number[] = [2, -1, 3, 0, 1, 2, -3, -2];

function affinityBias(genes: Genes): number {
  return AFFINITY_BIAS[genes.affinity % AFFINITY_BIAS.length] ?? 0;
}

/** Promedio de ánimo a lo largo de la vida activa. 50 si todavía no hay datos. */
export function animoPromedio(crianza: Crianza): number {
  return crianza.ticksMedidos === 0 ? 50 : crianza.sumaAnimo / crianza.ticksMedidos;
}

export function saludPromedio(crianza: Crianza): number {
  return crianza.ticksMedidos === 0 ? 100 : crianza.sumaSalud / crianza.ticksMedidos;
}

/**
 * Eje somático: qué tan "de cuerpo" es la crianza.
 *
 * Positivo tira a pétreo, negativo a vaporoso. Proteína y mineral construyen
 * cuerpo; dulce y cristal, otra cosa.
 */
export function ejeSomatico(crianza: Crianza, genes: Genes): number {
  const cuerpo = crianza.dieta.proteina + crianza.dieta.mineral;
  const etereo = crianza.dieta.dulce + crianza.dieta.raro;
  return cuerpo - etereo + affinityBias(genes);
}

/**
 * Eje de actividad: qué tan movida fue la crianza.
 *
 * Jugar tira a activo, acariciar a calmo. El ánimo promedio también cuenta, con
 * menos peso: una criatura que vivió contenta tiende a lo activo aunque no la
 * hayas hecho correr.
 */
export function ejeActividad(crianza: Crianza): number {
  return crianza.juego - crianza.calma + (animoPromedio(crianza) - 50) / 12;
}

export function resolverJuvenil(crianza: Crianza, genes: Genes): Form {
  return ejeSomatico(crianza, genes) >= 0 ? "petreo" : "vaporoso";
}

export function resolverAdulto(crianza: Crianza, genes: Genes, juvenil: Form): Form {
  // Si por lo que sea no hay juvenil registrado, se recalcula.
  const rama =
    juvenil === "petreo" || juvenil === "vaporoso" ? juvenil : resolverJuvenil(crianza, genes);
  const activo = ejeActividad(crianza) >= 0;

  if (rama === "petreo") return activo ? "coloso" : "guardian";
  return activo ? "errante" : "oraculo";
}

// ---------------------------------------------------------------------------
// Presentación
// ---------------------------------------------------------------------------

const FORM_NAMES: Record<Form, string> = {
  indefinida: "Sin definir",
  petreo: "Pétreo",
  vaporoso: "Vaporoso",
  coloso: "Coloso",
  guardian: "Guardián",
  errante: "Errante",
  oraculo: "Oráculo",
};

const FORM_DESCRIPTIONS: Record<Form, string> = {
  indefinida: "Todavía no muestra por dónde va a crecer.",
  petreo: "Se le puso el cuerpo denso. Comió para durar.",
  vaporoso: "Se le afinó el cuerpo. Comió para otra cosa.",
  coloso: "Cuerpo de sobra y ganas de usarlo.",
  guardian: "Se plantó y no se mueve de ahí. Le gusta el lugar.",
  errante: "Liviana y sin quedarse quieta un segundo.",
  oraculo: "Callada, mirando cosas que nadie más mira.",
};

export function formName(form: Form): string {
  return FORM_NAMES[form];
}

export function formDescription(form: Form): string {
  return FORM_DESCRIPTIONS[form];
}

export const STAGE_NAMES: Record<Stage, string> = {
  bebe: "Bebé",
  juvenil: "Juvenil",
  adulto: "Adulto",
};

/** Las cuatro formas adultas posibles. */
export const ADULT_FORMS: readonly Form[] = ["coloso", "guardian", "errante", "oraculo"];

/**
 * Formas que cuentan para el codex: las dos juveniles y las cuatro adultas.
 *
 * "indefinida" queda afuera a propósito: no es una forma alcanzada, es la
 * ausencia de una. Registrarla haría que el codex arrancara con algo hecho.
 */
export const COLLECTIBLE_FORMS: readonly Form[] = ["petreo", "vaporoso", ...ADULT_FORMS];
