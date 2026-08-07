/**
 * Generadores pseudoaleatorios deterministas.
 *
 * Regla del proyecto: `Math.random()` NO se usa jamás en generación de criaturas.
 * El mismo genoma tiene que producir siempre exactamente la misma criatura, en
 * cualquier máquina y en cualquier momento. Todo lo "aleatorio" sale de acá,
 * sembrado desde el genoma.
 */

const MASK64 = (1n << 64n) - 1n;

/**
 * splitmix64: mezclador de 64 bits. Dado un entero cualquiera devuelve otro
 * bien distribuido. Se usa para derivar sub-semillas a partir del genoma.
 */
export function splitmix64(seed: bigint): bigint {
  let z = (seed + 0x9e3779b97f4a7c15n) & MASK64;
  z = ((z ^ (z >> 30n)) * 0xbf58476d1ce4e5b9n) & MASK64;
  z = ((z ^ (z >> 27n)) * 0x94d049bb133111ebn) & MASK64;
  return (z ^ (z >> 31n)) & MASK64;
}

/**
 * Deriva una sub-semilla de 32 bits a partir del genoma y una etiqueta.
 *
 * Cada subsistema pide la suya (`"manchas"`, `"eventos"`, …) para tener su propio
 * flujo independiente. Así agregar un consumo de azar en un subsistema no corre
 * la secuencia de los demás — que es el bug clásico que rompe el determinismo
 * entre versiones.
 */
export function deriveSeed(seed: bigint, label: string): number {
  let h = splitmix64(seed);
  for (let i = 0; i < label.length; i++) {
    h = splitmix64(h ^ BigInt(label.charCodeAt(i)));
  }
  return Number(h & 0xffffffffn) >>> 0;
}

export interface Rng {
  /** Flotante en [0, 1). */
  next(): number;
  /** Entero en [0, maxExclusive). */
  int(maxExclusive: number): number;
  /** Entero en [min, max], ambos incluidos. */
  range(min: number, max: number): number;
  /** `true` con probabilidad `p` (0..1). */
  bool(p?: number): boolean;
  /** Elige un elemento del array. Lanza si está vacío. */
  pick<T>(items: readonly T[]): T;
}

/** mulberry32: PRNG de 32 bits, rápido y con estado mínimo. */
export function mulberry32(seed: number): Rng {
  let state = seed >>> 0;

  const next = (): number => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = Math.imul(state ^ (state >>> 15), 1 | state);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };

  return {
    next,
    int(maxExclusive) {
      return Math.floor(next() * maxExclusive);
    },
    range(min, max) {
      return min + Math.floor(next() * (max - min + 1));
    },
    bool(p = 0.5) {
      return next() < p;
    },
    pick<T>(items: readonly T[]): T {
      if (items.length === 0) {
        throw new Error("rng.pick() recibió un array vacío");
      }
      const value = items[Math.floor(next() * items.length)];
      if (value === undefined) {
        throw new Error("rng.pick() salió de rango");
      }
      return value;
    },
  };
}

/** Atajo: crea un Rng derivado de un genoma y una etiqueta. */
export function rngFor(seed: bigint, label: string): Rng {
  return mulberry32(deriveSeed(seed, label));
}
