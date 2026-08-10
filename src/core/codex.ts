/**
 * El codex: el registro de lo que descubriste.
 *
 * Hasta acá el juego generaba una variedad enorme y no quedaba ningún rastro de
 * ella. Cada criatura nueva pisaba a la anterior y los linajes, las formas y las
 * rarezas que hubieras visto se perdían apenas cerrabas.
 *
 * El codex existe para eso: guardar qué apareció alguna vez y, sobre todo,
 * mostrar qué falta. Sin la parte de "qué falta" no es una colección, es un
 * historial.
 *
 * Módulo puro: recibe un genoma y una forma, devuelve un codex nuevo. No muta
 * nada ni sabe de dónde salieron los datos.
 */

import { COLLECTIBLE_FORMS, type Form, formName } from "./evolution.ts";
import { LINEAGES, decodeGenome, lineageName } from "./genome.ts";
import { TRAIT_CATALOG, detectTraits } from "./traits.ts";

export interface Codex {
  /** Índices de linaje descubiertos, ordenados. */
  linajes: number[];
  /** Formas evolutivas alcanzadas alguna vez. */
  formas: Form[];
  /** Ids de rarezas encontradas. */
  rarezas: string[];
  /** Cuántas criaturas se registraron en total, incluidas las repetidas. */
  totalRegistradas: number;
}

export function codexInicial(): Codex {
  return { linajes: [], formas: [], rarezas: [], totalRegistradas: 0 };
}

export type TipoDescubrimiento = "linaje" | "forma" | "rareza";

export interface Descubrimiento {
  tipo: TipoDescubrimiento;
  /** Identificador interno, para la UI. */
  id: string;
  /** Nombre legible, ya redactado. */
  nombre: string;
}

/**
 * Registra una criatura en el codex.
 *
 * Devuelve además qué fue novedad. Es información que la UI necesita: descubrir
 * algo por primera vez tiene que sentirse distinto a volver a verlo, y sin esto
 * habría que comparar el codex viejo contra el nuevo desde afuera.
 */
export function registrar(
  codex: Codex,
  seed: bigint,
  forma: Form,
): { codex: Codex; nuevos: Descubrimiento[] } {
  const genes = decodeGenome(seed);
  const nuevos: Descubrimiento[] = [];

  const linajes = [...codex.linajes];
  if (!linajes.includes(genes.lineage)) {
    linajes.push(genes.lineage);
    nuevos.push({
      tipo: "linaje",
      id: String(genes.lineage),
      nombre: lineageName(genes),
    });
  }

  const formas = [...codex.formas];
  // "indefinida" no es un descubrimiento: es la ausencia de uno.
  if (forma !== "indefinida" && !formas.includes(forma)) {
    formas.push(forma);
    nuevos.push({ tipo: "forma", id: forma, nombre: formName(forma) });
  }

  const rarezas = [...codex.rarezas];
  for (const trait of detectTraits(seed)) {
    if (!rarezas.includes(trait.id)) {
      rarezas.push(trait.id);
      nuevos.push({ tipo: "rareza", id: trait.id, nombre: trait.name });
    }
  }

  return {
    codex: {
      // Ordenados para que el guardado sea estable: si no, dos partidas
      // equivalentes producen JSON distinto y cualquier comparación miente.
      linajes: linajes.sort((a, b) => a - b),
      formas: formas.slice().sort(),
      rarezas: rarezas.slice().sort(),
      totalRegistradas: codex.totalRegistradas + 1,
    },
    nuevos,
  };
}

export interface Avance {
  vistos: number;
  total: number;
}

export interface ProgresoCodex {
  linajes: Avance;
  formas: Avance;
  rarezas: Avance;
  /** Porcentaje global de completitud, 0-100. */
  porcentaje: number;
}

/**
 * Cuánto llevás descubierto.
 *
 * Las dos rarezas legendarias entran en el total aunque sean prácticamente
 * inalcanzables — Pangrama es 1 en 880.000. Es a propósito: un codex que se
 * completa del todo deja de dar motivo para seguir mirando.
 */
export function progresoCodex(codex: Codex): ProgresoCodex {
  const linajes: Avance = { vistos: codex.linajes.length, total: LINEAGES.length };
  const formas: Avance = { vistos: codex.formas.length, total: COLLECTIBLE_FORMS.length };
  const rarezas: Avance = { vistos: codex.rarezas.length, total: TRAIT_CATALOG.length };

  const vistos = linajes.vistos + formas.vistos + rarezas.vistos;
  const total = linajes.total + formas.total + rarezas.total;

  return { linajes, formas, rarezas, porcentaje: Math.round((vistos / total) * 100) };
}

/** ¿Ya se había visto este linaje? Para pintar la ficha sin recalcular todo. */
export function conoceLinaje(codex: Codex, lineage: number): boolean {
  return codex.linajes.includes(lineage);
}

export function conoceForma(codex: Codex, forma: Form): boolean {
  return codex.formas.includes(forma);
}

export function conoceRareza(codex: Codex, id: string): boolean {
  return codex.rarezas.includes(id);
}
