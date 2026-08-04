/**
 * Rarezas emergentes.
 *
 * La decisión de diseño más importante del juego: la rareza NO es una tabla de
 * loot ni un tirón de dados oculto. Es una propiedad matemática del propio seed.
 *
 * Eso significa que cualquiera puede verificar por qué su criatura es rara —
 * no hay pity timer, no hay porcentajes escondidos, no hay nada que ajustar
 * desde el servidor. El número que te tocó o cumple la propiedad o no.
 */

const MASK64 = (1n << 64n) - 1n;

export type TraitTier = "raro" | "epico" | "legendario";

export interface Trait {
  id: string;
  name: string;
  /** La regla, en castellano. Tiene que ser verificable a mano. */
  rule: string;
  tier: TraitTier;
  /**
   * Frecuencia estimada analíticamente. TODO(Fase 1d): verificar por muestreo
   * y corregir estos valores con lo medido — no mostrarlos en la UI hasta entonces.
   */
  approxRate: number;
}

// ---------------------------------------------------------------------------
// Primitivas sobre bits
// ---------------------------------------------------------------------------

/** Cantidad de bits en 1 (peso de Hamming). */
export function popcount(value: bigint): number {
  let count = 0;
  let v = value & MASK64;
  while (v > 0n) {
    v &= v - 1n;
    count++;
  }
  return count;
}

/** Racha más larga de bits en 1 consecutivos. */
export function longestRunOfOnes(value: bigint): number {
  let longest = 0;
  let current = 0;
  for (let i = 0; i < 64; i++) {
    if (((value >> BigInt(i)) & 1n) === 1n) {
      current++;
      if (current > longest) longest = current;
    } else {
      current = 0;
    }
  }
  return longest;
}

/** Invierte el orden de 16 bits. */
export function reverseBits16(value: number): number {
  let reversed = 0;
  for (let i = 0; i < 16; i++) {
    reversed = (reversed << 1) | ((value >> i) & 1);
  }
  return reversed >>> 0;
}

/** ¿Los 16 nibbles del genoma son los 16 valores posibles, sin repetir? */
export function nibblesAllDistinct(value: bigint): boolean {
  let seen = 0;
  for (let i = 0; i < 16; i++) {
    const nibble = Number((value >> BigInt(i * 4)) & 0xfn);
    const bit = 1 << nibble;
    if ((seen & bit) !== 0) return false;
    seen |= bit;
  }
  return true;
}

function modPow(base: bigint, exponent: bigint, modulus: bigint): bigint {
  let result = 1n;
  let b = base % modulus;
  let e = exponent;
  while (e > 0n) {
    if ((e & 1n) === 1n) result = (result * b) % modulus;
    b = (b * b) % modulus;
    e >>= 1n;
  }
  return result;
}

/**
 * Miller-Rabin determinista.
 *
 * Estas 12 bases son suficientes para decidir primalidad con certeza para todo
 * n < 3.3·10^24, así que cubren los 64 bits del genoma sin margen de error.
 */
const MILLER_RABIN_BASES = [2n, 3n, 5n, 7n, 11n, 13n, 17n, 19n, 23n, 29n, 31n, 37n];

export function isPrime(value: bigint): boolean {
  if (value < 2n) return false;
  for (const base of MILLER_RABIN_BASES) {
    if (value === base) return true;
    if (value % base === 0n) return false;
  }

  let d = value - 1n;
  let r = 0;
  while ((d & 1n) === 0n) {
    d >>= 1n;
    r++;
  }

  for (const base of MILLER_RABIN_BASES) {
    let x = modPow(base, d, value);
    if (x === 1n || x === value - 1n) continue;

    let composite = true;
    for (let i = 1; i < r; i++) {
      x = (x * x) % value;
      if (x === value - 1n) {
        composite = false;
        break;
      }
    }
    if (composite) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Catálogo de rarezas
// ---------------------------------------------------------------------------

interface TraitRule extends Trait {
  test: (seed: bigint) => boolean;
}

const TRAIT_RULES: readonly TraitRule[] = [
  {
    id: "equilibrado",
    name: "Equilibrado",
    rule: "Exactamente 32 de sus 64 bits están en 1",
    tier: "raro",
    approxRate: 0.0993,
    test: (seed) => popcount(seed) === 32,
  },
  {
    id: "vacio",
    name: "Vacío",
    rule: "24 bits o menos en 1 — genoma escaso, pigmentación pálida",
    tier: "raro",
    approxRate: 0.0308,
    test: (seed) => popcount(seed) <= 24,
  },
  {
    id: "saturado",
    name: "Saturado",
    rule: "40 bits o más en 1 — genoma denso, pigmentación intensa",
    tier: "raro",
    approxRate: 0.0308,
    test: (seed) => popcount(seed) >= 40,
  },
  {
    id: "racha",
    name: "Racha",
    rule: "Tiene 9 o más bits en 1 consecutivos",
    tier: "raro",
    approxRate: 0.05,
    test: (seed) => longestRunOfOnes(seed) >= 9,
  },
  {
    id: "primordial",
    name: "Primordial",
    rule: "El genoma es un número primo",
    tier: "epico",
    approxRate: 0.0225,
    test: isPrime,
  },
  {
    id: "uroboros",
    name: "Uróboros",
    rule: "Su primer byte es idéntico al último",
    tier: "epico",
    approxRate: 0.0039,
    test: (seed) => Number((seed >> 56n) & 0xffn) === Number(seed & 0xffn),
  },
  {
    id: "espejo",
    name: "Espejo",
    rule: "Sus 16 bits más altos son el reflejo exacto de los 16 más bajos",
    tier: "legendario",
    approxRate: 0.0000153,
    test: (seed) => reverseBits16(Number(seed & 0xffffn)) === Number((seed >> 48n) & 0xffffn),
  },
  {
    id: "pangrama",
    name: "Pangrama",
    rule: "Sus 16 nibbles son los 16 valores posibles, sin repetir ninguno",
    tier: "legendario",
    approxRate: 0.00000113,
    test: nibblesAllDistinct,
  },
];

/** Solo los metadatos, sin la función de test. Útil para la UI y los tests. */
export const TRAIT_CATALOG: readonly Trait[] = TRAIT_RULES.map(({ test: _test, ...meta }) => meta);

/** Detecta todas las rarezas que cumple un genoma. */
export function detectTraits(seed: bigint): Trait[] {
  const normalized = seed & MASK64;
  const found: Trait[] = [];
  for (const { test, ...meta } of TRAIT_RULES) {
    if (test(normalized)) found.push(meta);
  }
  return found;
}

const TIER_WEIGHT: Record<TraitTier, number> = {
  raro: 1,
  epico: 3,
  legendario: 10,
};

export type RarityTier = "comun" | "raro" | "epico" | "legendario";

export const RARITY_LABELS: Record<RarityTier, string> = {
  comun: "Común",
  raro: "Raro",
  epico: "Épico",
  legendario: "Legendario",
};

/** Puntaje agregado de rareza de una criatura. */
export function rarityScore(traits: readonly Trait[]): number {
  return traits.reduce((total, trait) => total + TIER_WEIGHT[trait.tier], 0);
}

/** Categoría global de la criatura, derivada de sus rarezas. */
export function rarityTier(traits: readonly Trait[]): RarityTier {
  const score = rarityScore(traits);
  if (score >= 10) return "legendario";
  if (score >= 3) return "epico";
  if (score >= 1) return "raro";
  return "comun";
}
