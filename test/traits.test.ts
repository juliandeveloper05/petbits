import { describe, expect, it } from "vitest";
import { splitmix64 } from "../src/core/rng.ts";
import {
  TRAIT_CATALOG,
  detectTraits,
  isPrime,
  longestRunOfOnes,
  nibblesAllDistinct,
  popcount,
  rarityTier,
  reverseBits16,
} from "../src/core/traits.ts";

describe("primitivas de bits", () => {
  it("popcount cuenta los bits en 1", () => {
    expect(popcount(0n)).toBe(0);
    expect(popcount(0xffffffffffffffffn)).toBe(64);
    expect(popcount(0b1011n)).toBe(3);
    expect(popcount(0x8000000000000000n)).toBe(1);
  });

  it("longestRunOfOnes encuentra la racha más larga", () => {
    expect(longestRunOfOnes(0n)).toBe(0);
    expect(longestRunOfOnes(0b1011101111n)).toBe(4);
    expect(longestRunOfOnes(0xffffffffffffffffn)).toBe(64);
    // Las rachas no se enganchan a través de un 0.
    expect(longestRunOfOnes(0b111011101110n)).toBe(3);
  });

  it("reverseBits16 invierte 16 bits", () => {
    expect(reverseBits16(0b0000000000000001)).toBe(0b1000000000000000);
    expect(reverseBits16(0xffff)).toBe(0xffff);
    expect(reverseBits16(0)).toBe(0);
    // Involutiva: aplicarla dos veces devuelve el original.
    expect(reverseBits16(reverseBits16(0x1234))).toBe(0x1234);
  });

  it("nibblesAllDistinct detecta permutaciones de los 16 nibbles", () => {
    expect(nibblesAllDistinct(0xfedcba9876543210n)).toBe(true);
    expect(nibblesAllDistinct(0x0123456789abcdefn)).toBe(true);
    expect(nibblesAllDistinct(0n)).toBe(false);
    expect(nibblesAllDistinct(0xfedcba9876543211n)).toBe(false);
  });
});

describe("primalidad (Miller-Rabin determinista)", () => {
  it("acierta en casos chicos", () => {
    const primes = [2n, 3n, 5n, 7n, 11n, 13n, 97n, 7919n];
    for (const p of primes) expect(isPrime(p), `${p} es primo`).toBe(true);

    const composites = [0n, 1n, 4n, 9n, 25n, 100n, 7917n];
    for (const c of composites) expect(isPrime(c), `${c} no es primo`).toBe(false);
  });

  it("acierta cerca del techo de 64 bits", () => {
    // 2^61 - 1 es primo de Mersenne; 2^64 - 1 = 3·5·17·257·641·65537·6700417.
    expect(isPrime(2n ** 61n - 1n)).toBe(true);
    expect(isPrime(2n ** 64n - 1n)).toBe(false);
    expect(isPrime(18446744073709551557n)).toBe(true); // mayor primo < 2^64
  });

  it("no se deja engañar por pseudoprimos fuertes de base 2", () => {
    // 2047 = 23·89 pasa el test de Miller-Rabin solo con base 2.
    expect(isPrime(2047n)).toBe(false);
    expect(isPrime(3215031751n)).toBe(false); // pseudoprimo para las bases 2,3,5,7
  });
});

describe("detección de rarezas", () => {
  it("reconoce genomas construidos a propósito", () => {
    const ids = (seed: bigint) => detectTraits(seed).map((t) => t.id);

    // 32 bits bajos en 1 → popcount exacto 32.
    expect(ids(0x00000000ffffffffn)).toContain("equilibrado");
    // Pangrama: los 16 nibbles, sin repetir.
    expect(ids(0xfedcba9876543210n)).toContain("pangrama");
    // Uróboros: primer byte igual al último.
    expect(ids(0xab000000000000abn)).toContain("uroboros");
    // Espejo: los 16 bits altos son el reflejo de los 16 bajos.
    const low = 0x1234;
    const mirror = (BigInt(reverseBits16(low)) << 48n) | BigInt(low);
    expect(ids(mirror)).toContain("espejo");
  });

  it("un genoma sin rarezas queda como común", () => {
    // A propósito NO se usa una constante escrita a mano: es demasiado fácil
    // elegir sin querer un genoma que sí cumple alguna regla. 0x0f1e2d3c4b5a6978
    // parece anodino y en realidad tiene popcount 32 y los 16 nibbles distintos,
    // o sea Equilibrado + Pangrama. Se busca uno en el muestreo determinista.
    const plain = sample(200).find((seed) => detectTraits(seed).length === 0);
    expect(plain, "el muestreo debería contener genomas sin rarezas").toBeDefined();
    expect(rarityTier(detectTraits(plain ?? 0n))).toBe("comun");
  });

  it("la mayoría de los genomas son comunes", () => {
    // Si esto se rompe, alguna regla se volvió demasiado laxa y la rareza
    // dejó de ser rara.
    const seeds = sample(2000);
    const common = seeds.filter((seed) => detectTraits(seed).length === 0).length;
    expect(common / seeds.length).toBeGreaterThan(0.7);
  });

  it("todo el catálogo tiene reglas e ids únicos", () => {
    const ids = TRAIT_CATALOG.map((t) => t.id);
    expect(new Set(ids).size).toBe(ids.length);
    for (const trait of TRAIT_CATALOG) {
      expect(trait.rule.length, `${trait.id} necesita una regla legible`).toBeGreaterThan(10);
      expect(trait.approxRate).toBeGreaterThan(0);
    }
  });
});

/**
 * Muestreo determinista: la cadena de splitmix64 arranca siempre de la misma
 * base, así que estos números no cambian entre corridas. Un muestreo con
 * Math.random() haría el test intermitente.
 */
function sample(count: number): bigint[] {
  const seeds: bigint[] = [];
  let cursor = 0x5eed_0000_0000_0001n;
  for (let i = 0; i < count; i++) {
    cursor = splitmix64(cursor);
    seeds.push(cursor);
  }
  return seeds;
}

describe("distribución de rarezas", () => {
  // Las rarezas basadas en bits son baratas de calcular, así que van con
  // muestra grande. La primalidad cuesta ~800 multiplicaciones de BigInt por
  // genoma y va aparte, con muestra chica.
  it("las tasas declaradas coinciden con lo medido", () => {
    const SAMPLES = 200_000;
    const counts = new Map<string, number>();

    for (const seed of sample(SAMPLES)) {
      for (const trait of detectTraits(seed)) {
        if (trait.id === "primordial") continue;
        counts.set(trait.id, (counts.get(trait.id) ?? 0) + 1);
      }
    }

    const measured: Record<string, string> = {};
    for (const trait of TRAIT_CATALOG) {
      if (trait.id === "primordial") continue;
      const rate = (counts.get(trait.id) ?? 0) / SAMPLES;
      measured[trait.id] =
        `${(rate * 100).toFixed(4)}% (declarado ${(trait.approxRate * 100).toFixed(4)}%)`;

      // Los legendarios son demasiado raros para medirlos con esta muestra:
      // se verifican por construcción más arriba, no por frecuencia.
      if (trait.tier === "legendario") continue;

      // Tolerancia amplia a propósito: esto detecta que una regla se rompió o
      // cambió de orden de magnitud, no fluctuación estadística.
      expect(rate, `${trait.id}: medido ${rate}, declarado ${trait.approxRate}`).toBeGreaterThan(
        trait.approxRate * 0.5,
      );
      expect(rate, `${trait.id}: medido ${rate}, declarado ${trait.approxRate}`).toBeLessThan(
        trait.approxRate * 1.5,
      );
    }

    console.table(measured);
  });

  it("la tasa de genomas primos ronda 1/ln(2^64)", () => {
    const SAMPLES = 4000;
    let primes = 0;
    for (const seed of sample(SAMPLES)) {
      if (isPrime(seed)) primes++;
    }
    const rate = primes / SAMPLES;
    // El teorema de los números primos da ~1/44.4 ≈ 2.25% cerca de 2^64.
    console.log(`primordial: ${(rate * 100).toFixed(3)}% sobre ${SAMPLES} genomas`);
    expect(rate).toBeGreaterThan(0.012);
    expect(rate).toBeLessThan(0.035);
  });
});
