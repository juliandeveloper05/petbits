import { describe, expect, it } from "vitest";
import {
  GENOME_LAYOUT,
  decodeGenome,
  formatSeed,
  hashString,
  normalizeSeed,
  parseSeed,
} from "../src/core/genome.ts";
import { deriveSeed, mulberry32, splitmix64 } from "../src/core/rng.ts";

function sample(count: number): bigint[] {
  const seeds: bigint[] = [];
  let cursor = 0xc0ffee_0000_0001n;
  for (let i = 0; i < count; i++) {
    cursor = splitmix64(cursor);
    seeds.push(cursor);
  }
  return seeds;
}

describe("mapa de bits del genoma", () => {
  it("cubre los 64 bits exactos, sin huecos ni solapamientos", () => {
    // Este es el test estructural más importante del archivo: si el mapa se
    // desalinea, todos los seeds existentes pasan a describir otra criatura.
    const covered = new Array<string | null>(64).fill(null);

    for (const field of GENOME_LAYOUT) {
      for (let i = field.offset; i < field.offset + field.bits; i++) {
        expect(i, `${field.key} se sale de los 64 bits`).toBeLessThan(64);
        expect(covered[i], `los bits de ${field.key} pisan a ${covered[i]}`).toBeNull();
        covered[i] = field.key;
      }
    }

    const gaps = covered.map((key, i) => (key === null ? i : -1)).filter((i) => i >= 0);
    expect(gaps, `bits sin asignar: ${gaps.join(", ")}`).toHaveLength(0);
  });
});

describe("serialización de semillas", () => {
  it("formatSeed y parseSeed son inversas", () => {
    for (const seed of sample(500)) {
      expect(parseSeed(formatSeed(seed))).toBe(seed);
    }
  });

  it("formatSeed siempre da 16 dígitos en 4 grupos", () => {
    expect(formatSeed(0n)).toBe("0000-0000-0000-0000");
    expect(formatSeed(1n)).toBe("0000-0000-0000-0001");
    expect(formatSeed(2n ** 64n - 1n)).toBe("FFFF-FFFF-FFFF-FFFF");
    for (const seed of sample(100)) {
      expect(formatSeed(seed)).toMatch(/^[0-9A-F]{4}(-[0-9A-F]{4}){3}$/);
    }
  });

  it("parseSeed acepta las formas que puede tipear una persona", () => {
    const expected = 0xdeadbeef12345678n;
    expect(parseSeed("DEAD-BEEF-1234-5678")).toBe(expected);
    expect(parseSeed("deadbeef12345678")).toBe(expected);
    expect(parseSeed("0xDEADBEEF12345678")).toBe(expected);
    expect(parseSeed("  DEAD BEEF 1234 5678  ")).toBe(expected);
    expect(parseSeed("42")).toBe(42n);
  });

  it("cualquier texto sirve como semilla, y siempre da la misma", () => {
    const first = parseSeed("julian");
    expect(parseSeed("julian")).toBe(first);
    expect(parseSeed("Julian")).not.toBe(first);
    expect(first).toBeLessThan(2n ** 64n);
    expect(hashString("")).toBeGreaterThan(0n);
  });

  it("rechaza la entrada vacía", () => {
    expect(() => parseSeed("")).toThrow();
    expect(() => parseSeed("   ")).toThrow();
  });

  it("normalizeSeed deja todo dentro de 64 bits sin signo", () => {
    expect(normalizeSeed(2n ** 64n)).toBe(0n);
    expect(normalizeSeed(-1n)).toBe(2n ** 64n - 1n);
    expect(normalizeSeed(5n)).toBe(5n);
  });
});

describe("decodificación", () => {
  it("es determinista y respeta los rangos de cada gen", () => {
    for (const seed of sample(1000)) {
      const genes = decodeGenome(seed);
      expect(decodeGenome(seed)).toEqual(genes);

      expect(genes.lineage).toBeGreaterThanOrEqual(0);
      expect(genes.lineage).toBeLessThan(16);
      expect(genes.hue).toBeLessThan(256);
      expect(genes.paletteMode).toBeLessThan(8);
      expect(genes.temperament).toBeLessThan(8);
      expect(genes.affinity).toBeLessThan(8);
      expect(genes.metabolism).toBeLessThan(8);
      expect(genes.mutation).toBeLessThan(256);

      for (const value of Object.values(genes.statBias)) {
        expect(value).toBeGreaterThanOrEqual(0);
        expect(value).toBeLessThan(4);
      }
    }
  });

  it("cada gen lee sus propios bits", () => {
    // Prender solo los bits de `hue` (24-31) no debe afectar a ningún otro gen.
    const base = decodeGenome(0n);
    const onlyHue = decodeGenome(0xffn << 24n);
    expect(onlyHue.hue).toBe(255);
    expect({ ...onlyHue, hue: 0 }).toEqual({ ...base, hue: 0 });
  });
});

describe("PRNG", () => {
  it("mulberry32 es determinista y se mantiene en [0,1)", () => {
    const a = mulberry32(12345);
    const b = mulberry32(12345);
    for (let i = 0; i < 1000; i++) {
      const value = a.next();
      expect(value).toBe(b.next());
      expect(value).toBeGreaterThanOrEqual(0);
      expect(value).toBeLessThan(1);
    }
  });

  it("las sub-semillas por etiqueta son independientes", () => {
    // Es la razón de ser de deriveSeed: que agregar consumo de azar en un
    // subsistema no corra la secuencia de los demás.
    const seed = 0xabcdef123456789n;
    expect(deriveSeed(seed, "manchas")).not.toBe(deriveSeed(seed, "eventos"));
    expect(deriveSeed(seed, "manchas")).toBe(deriveSeed(seed, "manchas"));
  });

  it("rng.pick no acepta arrays vacíos", () => {
    expect(() => mulberry32(1).pick([])).toThrow();
  });

  it("splitmix64 se queda dentro de 64 bits", () => {
    let cursor = 1n;
    for (let i = 0; i < 500; i++) {
      cursor = splitmix64(cursor);
      expect(cursor).toBeGreaterThanOrEqual(0n);
      expect(cursor).toBeLessThan(2n ** 64n);
    }
  });
});
