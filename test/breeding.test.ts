import { describe, expect, it } from "vitest";
import {
  CRUZA_COOLDOWN_MS,
  CRUZA_MIN_SALUD,
  CRUZA_MIN_VINCULO,
  cruzar,
  describirHerencia,
  elegibilidad,
  marcarCruzada,
  puedenCruzar,
} from "../src/core/breeding.ts";
import { GENOME_LAYOUT, decodeGenome } from "../src/core/genome.ts";
import { splitmix64 } from "../src/core/rng.ts";
import { type CreatureState, createCreature } from "../src/core/simulation.ts";

const PADRE_A = 0xa3f091c477be2d08n;
const PADRE_B = 0x5c2e7b1049fa6d33n;
const T0 = Date.UTC(2026, 2, 2, 9, 0, 0);
const HORA = 3_600_000;

function sample(count: number): bigint[] {
  const seeds: bigint[] = [];
  let cursor = 0xc02a_0000_0001n;
  for (let i = 0; i < count; i++) {
    cursor = splitmix64(cursor);
    seeds.push(cursor);
  }
  return seeds;
}

/** Una criatura lista para cruzar. */
function adulta(seed: bigint, extra: Partial<CreatureState> = {}): CreatureState {
  const base = createCreature(seed, T0, 0);
  return {
    ...base,
    etapa: "adulto",
    forma: "coloso",
    stats: { ...base.stats, salud: 90, vinculo: 40 },
    ...extra,
  };
}

describe("determinismo", () => {
  it("los mismos padres y el mismo nonce dan el mismo hijo", () => {
    const a = cruzar(PADRE_A, PADRE_B, 7);
    const b = cruzar(PADRE_A, PADRE_B, 7);
    expect(b.seed).toBe(a.seed);
    expect(b.mutaciones).toBe(a.mutaciones);
    expect(b.herencia).toEqual(a.herencia);
  });

  it("cambiar el nonce da otro hijo", () => {
    // Si no, cruzar la misma pareja dos veces no tendría sentido.
    const hijos = new Set([1, 2, 3, 4, 5].map((n) => cruzar(PADRE_A, PADRE_B, n).seed));
    expect(hijos.size).toBe(5);
  });

  it("es simétrica: el par decide, no el orden", () => {
    const ab = cruzar(PADRE_A, PADRE_B, 42);
    const ba = cruzar(PADRE_B, PADRE_A, 42);
    expect(ba.seed).toBe(ab.seed);
  });
});

describe("herencia por gen", () => {
  /**
   * El test que justifica la decisión de diseño del módulo.
   *
   * Cruzando bit a bit, los ocho bits de `hue` vendrían mezclados de los dos
   * padres y darían un tono que no es el de ninguno. Cruzando por gen, cada
   * campo llega entero, y por eso el hijo se PARECE a sus padres.
   */
  it("cada gen del hijo viene entero de alguno de los padres", () => {
    const genesA = decodeGenome(PADRE_A);
    const genesB = decodeGenome(PADRE_B);

    let revisados = 0;
    for (let nonce = 0; nonce < 40; nonce++) {
      const cruza = cruzar(PADRE_A, PADRE_B, nonce);
      const hijo = decodeGenome(cruza.seed);

      for (const field of GENOME_LAYOUT) {
        if (cruza.herencia[field.key] === "mutado") continue;
        if (field.key === "statBias") continue; // objeto anidado, se chequea aparte

        const valor = hijo[field.key as keyof typeof hijo];
        const deA = genesA[field.key as keyof typeof genesA];
        const deB = genesB[field.key as keyof typeof genesB];
        expect([deA, deB], `gen ${field.key} inventado en nonce ${nonce}`).toContain(valor);
        revisados++;
      }
    }
    expect(revisados).toBeGreaterThan(400);
  });

  it("cada bit del hijo sale de un padre, salvo los mutados", () => {
    for (let nonce = 0; nonce < 30; nonce++) {
      const { seed, mutaciones } = cruzar(PADRE_A, PADRE_B, nonce);
      let ajenos = 0;
      for (let bit = 0n; bit < 64n; bit++) {
        const b = (seed >> bit) & 1n;
        if (b !== ((PADRE_A >> bit) & 1n) && b !== ((PADRE_B >> bit) & 1n)) ajenos++;
      }
      // Un bit "ajeno" solo puede venir de una mutación, y solo cuenta cuando
      // los dos padres coincidían en ese bit.
      expect(ajenos).toBeLessThanOrEqual(mutaciones);
    }
  });

  it("informa de dónde vino cada gen", () => {
    const cruza = cruzar(PADRE_A, PADRE_B, 3);
    expect(Object.keys(cruza.herencia).sort()).toEqual(GENOME_LAYOUT.map((f) => f.key).sort());
    for (const origen of Object.values(cruza.herencia)) {
      expect(["A", "B", "mutado"]).toContain(origen);
    }
  });

  it("cruzar una criatura consigo misma se clona, salvo mutaciones", () => {
    const { seed, mutaciones } = cruzar(PADRE_A, PADRE_A, 11);
    let distintos = 0;
    for (let bit = 0n; bit < 64n; bit++) {
      if (((seed >> bit) & 1n) !== ((PADRE_A >> bit) & 1n)) distintos++;
    }
    expect(distintos).toBe(mutaciones);
  });
});

describe("mutación", () => {
  it("promedia unas pocas mutaciones por cruza", () => {
    let total = 0;
    const CRUZAS = 500;
    for (let nonce = 0; nonce < CRUZAS; nonce++) {
      total += cruzar(PADRE_A, PADRE_B, nonce).mutaciones;
    }
    const promedio = total / CRUZAS;
    // Se buscan ~2.5 sobre 64 bits: suficiente para que aparezca algo nuevo,
    // poco para que el hijo deje de parecerse a sus padres.
    expect(promedio).toBeGreaterThan(1.5);
    expect(promedio).toBeLessThan(4);
  });

  it("los hijos de padres distintos son variados", () => {
    const seeds = sample(60);
    const hijos = new Set<bigint>();
    for (let i = 0; i + 1 < seeds.length; i += 2) {
      hijos.add(cruzar(seeds[i] ?? 0n, seeds[i + 1] ?? 0n, i).seed);
    }
    expect(hijos.size).toBe(30);
  });
});

describe("quién puede cruzar", () => {
  it("una pareja sana y adulta puede", () => {
    expect(puedenCruzar(adulta(PADRE_A), adulta(PADRE_B), T0)).toEqual({ puede: true });
  });

  it("una criatura no puede cruzar consigo misma", () => {
    const una = adulta(PADRE_A);
    const resultado = puedenCruzar(una, una, T0);
    expect(resultado.puede).toBe(false);
    if (resultado.puede) return;
    expect(resultado.motivo).toContain("otra criatura");
  });

  it("los que no son adultos no pueden", () => {
    const cria = adulta(PADRE_A, { etapa: "juvenil" });
    const resultado = elegibilidad(cria, T0);
    expect(resultado.puede).toBe(false);
    if (resultado.puede) return;
    expect(resultado.motivo).toContain("crecer");
  });

  it("hace falta vínculo, no solo tiempo", () => {
    // Es lo que ata la cruza al bucle de cuidado en vez de volverla un atajo.
    const base = adulta(PADRE_A);
    const distante = {
      ...base,
      stats: { ...base.stats, vinculo: CRUZA_MIN_VINCULO - 1 },
    };
    const resultado = elegibilidad(distante, T0);
    expect(resultado.puede).toBe(false);
    if (resultado.puede) return;
    expect(resultado.motivo).toContain("vínculo");
  });

  it("hace falta salud", () => {
    const base = adulta(PADRE_A);
    const enferma = { ...base, stats: { ...base.stats, salud: CRUZA_MIN_SALUD - 1 } };
    expect(elegibilidad(enferma, T0).puede).toBe(false);
  });

  it("en letargo no se cruza", () => {
    const dormida = adulta(PADRE_A, { letargico: true });
    const resultado = elegibilidad(dormida, T0);
    expect(resultado.puede).toBe(false);
    if (resultado.puede) return;
    expect(resultado.motivo).toContain("letargo");
  });
});

describe("descanso entre cruzas", () => {
  it("después de cruzar hay que esperar", () => {
    const recien = marcarCruzada(adulta(PADRE_A), T0);
    expect(recien.ultimaCruzaMs).toBe(T0);

    const alRato = elegibilidad(recien, T0 + 2 * HORA);
    expect(alRato.puede).toBe(false);
    if (alRato.puede) return;
    expect(alRato.motivo).toContain("6 h");

    expect(elegibilidad(recien, T0 + CRUZA_COOLDOWN_MS).puede).toBe(true);
  });

  it("marcarCruzada no muta la criatura original", () => {
    const original = adulta(PADRE_A);
    const antes = JSON.stringify(original);
    marcarCruzada(original, T0);
    expect(JSON.stringify(original)).toBe(antes);
  });

  it("una criatura nueva nunca cruzó", () => {
    expect(createCreature(PADRE_A, T0, 0).ultimaCruzaMs).toBeNull();
  });
});

describe("descripción para el jugador", () => {
  it("resume a quién salió el hijo", () => {
    const textos = new Set<string>();
    for (let nonce = 0; nonce < 60; nonce++) {
      textos.add(describirHerencia(cruzar(PADRE_A, PADRE_B, nonce)));
    }
    // Con sesenta cruzas tienen que aparecer varios resultados distintos.
    expect(textos.size).toBeGreaterThan(1);
    for (const texto of textos) expect(texto.length).toBeGreaterThan(10);
  });
});
