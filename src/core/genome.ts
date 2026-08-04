/**
 * El genoma: una semilla de 64 bits que ES la criatura.
 *
 * No hay tabla de mascotas ni lista de sprites. Cuerpo, color, carácter y
 * rarezas se derivan todos de este número. Mismo número → misma criatura,
 * siempre. Compartís el seed y la otra persona ve exactamente lo mismo.
 */

export const GENOME_BITS = 64n;
const MASK64 = (1n << GENOME_BITS) - 1n;

/** Extrae `length` bits del genoma a partir de `offset`. */
function bits(seed: bigint, offset: number, length: number): number {
  const mask = (1n << BigInt(length)) - 1n;
  return Number((seed >> BigInt(offset)) & mask);
}

export interface StatBias {
  vigor: number;
  animo: number;
  ingenio: number;
  vinculo: number;
}

export interface Genes {
  /** Linaje base (0-15). Define el "de qué familia es". */
  lineage: number;
  /** Arquetipo de silueta (0-15). */
  bodyShape: number;
  /** Estilo de ojos (0-15). */
  eyes: number;
  /** Estilo de boca (0-15). */
  mouth: number;
  /** Bitfield de apéndices: 1=orejas 2=cuernos 4=alas 8=cola. */
  appendages: number;
  /** Patrón sobre el cuerpo (0-15). */
  pattern: number;
  /** Tono base 0-255 → 0-360°. */
  hue: number;
  /** Modo de paleta (0-7). */
  paletteMode: number;
  /** Carácter (0-7). Afecta los eventos de la simulación. */
  temperament: number;
  /** Velocidad a la que gasta energía (0-7). */
  metabolism: number;
  /** Afinidad elemental (0-7). Sesga la rama evolutiva. */
  affinity: number;
  /** Proporciones de cabeza y cuerpo (0-15). */
  proportion: number;
  /** Sesgo de estadísticas, 2 bits cada una. */
  statBias: StatBias;
  /** Bits de mutación / reserva (0-255). */
  mutation: number;
}

/**
 * Mapa de bits del genoma. El orden importa: cambiarlo invalida todos los
 * seeds existentes, así que se versiona junto con el formato de guardado.
 */
export const GENOME_LAYOUT = [
  { key: "lineage", offset: 0, bits: 4 },
  { key: "bodyShape", offset: 4, bits: 4 },
  { key: "eyes", offset: 8, bits: 4 },
  { key: "mouth", offset: 12, bits: 4 },
  { key: "appendages", offset: 16, bits: 4 },
  { key: "pattern", offset: 20, bits: 4 },
  { key: "hue", offset: 24, bits: 8 },
  { key: "paletteMode", offset: 32, bits: 3 },
  { key: "temperament", offset: 35, bits: 3 },
  { key: "metabolism", offset: 38, bits: 3 },
  { key: "affinity", offset: 41, bits: 3 },
  { key: "proportion", offset: 44, bits: 4 },
  { key: "statBias", offset: 48, bits: 8 },
  { key: "mutation", offset: 56, bits: 8 },
] as const;

export function decodeGenome(seed: bigint): Genes {
  const normalized = seed & MASK64;
  const statRaw = bits(normalized, 48, 8);

  return {
    lineage: bits(normalized, 0, 4),
    bodyShape: bits(normalized, 4, 4),
    eyes: bits(normalized, 8, 4),
    mouth: bits(normalized, 12, 4),
    appendages: bits(normalized, 16, 4),
    pattern: bits(normalized, 20, 4),
    hue: bits(normalized, 24, 8),
    paletteMode: bits(normalized, 32, 3),
    temperament: bits(normalized, 35, 3),
    metabolism: bits(normalized, 38, 3),
    affinity: bits(normalized, 41, 3),
    proportion: bits(normalized, 44, 4),
    statBias: {
      vigor: statRaw & 0b11,
      animo: (statRaw >> 2) & 0b11,
      ingenio: (statRaw >> 4) & 0b11,
      vinculo: (statRaw >> 6) & 0b11,
    },
    mutation: bits(normalized, 56, 8),
  };
}

// ---------------------------------------------------------------------------
// Nombres. Nada de "perrito" o "gatito": estas criaturas no son animales.
// ---------------------------------------------------------------------------

export const LINEAGES = [
  "Nébula",
  "Fungo",
  "Cristal",
  "Limo",
  "Pluma",
  "Escama",
  "Musgo",
  "Brasa",
  "Vapor",
  "Óxido",
  "Coral",
  "Cirro",
  "Duna",
  "Eco",
  "Prisma",
  "Raíz",
] as const;

export const TEMPERAMENTS = [
  "Plácido",
  "Curioso",
  "Arisco",
  "Leal",
  "Errante",
  "Voraz",
  "Tímido",
  "Feroz",
] as const;

export const AFFINITIES = [
  "Brasa",
  "Marea",
  "Raíz",
  "Chispa",
  "Escarcha",
  "Polvo",
  "Eco",
  "Vacío",
] as const;

export const METABOLISMS = [
  "Aletargado",
  "Lento",
  "Sereno",
  "Regular",
  "Activo",
  "Inquieto",
  "Ávido",
  "Frenético",
] as const;

/** Devuelve un elemento de una tabla de nombres sin salirse del rango. */
function nameFrom(table: readonly string[], index: number): string {
  return table[index % table.length] ?? table[0] ?? "?";
}

export function lineageName(genes: Genes): string {
  return nameFrom(LINEAGES, genes.lineage);
}

export function temperamentName(genes: Genes): string {
  return nameFrom(TEMPERAMENTS, genes.temperament);
}

export function affinityName(genes: Genes): string {
  return nameFrom(AFFINITIES, genes.affinity);
}

export function metabolismName(genes: Genes): string {
  return nameFrom(METABOLISMS, genes.metabolism);
}

// ---------------------------------------------------------------------------
// Serialización del seed
// ---------------------------------------------------------------------------

/** Formatea el seed como hex agrupado: `A3F0-91C4-77BE-2D08`. */
export function formatSeed(seed: bigint): string {
  const hex = (seed & MASK64).toString(16).toUpperCase().padStart(16, "0");
  return (hex.match(/.{4}/g) ?? [hex]).join("-");
}

/**
 * Interpreta la entrada del usuario como seed.
 *
 * Acepta hex con o sin guiones y con o sin `0x`, o un decimal. Cualquier otra
 * cosa se hashea a un seed válido, así que escribir tu nombre también funciona
 * y siempre da la misma criatura.
 */
export function parseSeed(input: string): bigint {
  const trimmed = input.trim();
  if (trimmed === "") {
    throw new Error("El seed no puede estar vacío");
  }

  const compact = trimmed.replace(/[\s-]/g, "");

  if (/^0x[0-9a-f]+$/i.test(compact)) {
    return BigInt(compact) & MASK64;
  }
  if (/^[0-9a-f]{1,16}$/i.test(compact) && /[a-f]/i.test(compact)) {
    return BigInt(`0x${compact}`) & MASK64;
  }
  if (/^\d+$/.test(compact)) {
    return BigInt(compact) & MASK64;
  }
  if (/^[0-9a-f]{1,16}$/i.test(compact)) {
    return BigInt(`0x${compact}`) & MASK64;
  }

  return hashString(trimmed);
}

/** Convierte texto arbitrario en un seed de 64 bits (FNV-1a de 64 bits). */
export function hashString(text: string): bigint {
  let hash = 0xcbf29ce484222325n;
  const prime = 0x100000001b3n;
  for (let i = 0; i < text.length; i++) {
    hash = ((hash ^ BigInt(text.charCodeAt(i))) * prime) & MASK64;
  }
  return hash;
}

/** Genera un seed nuevo con entropía criptográfica. */
export function randomSeed(): bigint {
  const bytes = new Uint8Array(8);
  globalThis.crypto.getRandomValues(bytes);
  let seed = 0n;
  for (const byte of bytes) {
    seed = (seed << 8n) | BigInt(byte);
  }
  return seed & MASK64;
}

/** Normaliza cualquier bigint al rango de 64 bits sin signo. */
export function normalizeSeed(seed: bigint): bigint {
  return ((seed % (MASK64 + 1n)) + MASK64 + 1n) & MASK64;
}
