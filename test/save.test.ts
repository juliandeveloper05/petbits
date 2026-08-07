import { describe, expect, it } from "vitest";
import { createCreature } from "../src/core/simulation.ts";
import {
  type Migration,
  type RawSave,
  SaveVersionError,
  applyMigrations,
} from "../src/state/migrations.ts";
import { SAVE_VERSION, createSave, parseSave } from "../src/state/save.ts";

const T0 = Date.UTC(2026, 2, 2, 9, 0, 0);
const creature = createCreature(0xa3f091c477be2d08n, T0, -180);

describe("ida y vuelta del guardado", () => {
  it("un save válido sobrevive a JSON", () => {
    const save = createSave(creature, T0);
    // El viaje real: IndexedDB estructura el objeto, pero JSON es el caso
    // más hostil y si pasa por acá pasa por cualquier lado.
    const outcome = parseSave(JSON.parse(JSON.stringify(save)));

    expect(outcome.ok).toBe(true);
    if (!outcome.ok) return;
    expect(outcome.save.criatura).toEqual(creature);
    expect(outcome.save.version).toBe(SAVE_VERSION);
  });

  it("el genoma viaja como texto porque JSON no sabe de bigint", () => {
    const save = createSave(creature, T0);
    expect(typeof save.criatura.seed).toBe("string");
    // Y se puede reconstruir sin pérdida, que es el punto.
    expect(BigInt(save.criatura.seed)).toBe(0xa3f091c477be2d08n);
  });
});

describe("guardados rotos", () => {
  /**
   * Ninguno de estos debe lanzar.
   *
   * Un save corrupto tiene que degradar a "empezá de nuevo", no romper la
   * aplicación con una excepción sin capturar durante el arranque.
   */
  const broken: [string, unknown][] = [
    ["null", null],
    ["texto suelto", "no soy un save"],
    ["número", 42],
    ["array", []],
    ["objeto vacío", {}],
    ["sin versión", { criatura: creature, guardadoMs: T0 }],
    ["versión en texto", { version: "1", criatura: creature, guardadoMs: T0 }],
    ["sin criatura", { version: 1, guardadoMs: T0 }],
    ["criatura incompleta", { version: 1, guardadoMs: T0, criatura: { seed: "1" } }],
    ["genoma no numérico", { version: 1, guardadoMs: T0, criatura: { ...creature, seed: "abc" } }],
    [
      "estadística fuera de rango",
      {
        version: 1,
        guardadoMs: T0,
        criatura: { ...creature, stats: { ...creature.stats, energia: 9999 } },
      },
    ],
  ];

  for (const [name, raw] of broken) {
    it(`rechaza sin lanzar: ${name}`, () => {
      let outcome: ReturnType<typeof parseSave> | undefined;
      expect(() => {
        outcome = parseSave(raw);
      }).not.toThrow();
      expect(outcome?.ok).toBe(false);
      if (outcome?.ok === false) {
        expect(outcome.reason.length).toBeGreaterThan(0);
      }
    });
  }

  it("el motivo del rechazo dice dónde estaba el problema", () => {
    const outcome = parseSave({
      version: 1,
      guardadoMs: T0,
      criatura: { ...creature, stats: { ...creature.stats, salud: -5 } },
    });
    expect(outcome.ok).toBe(false);
    if (outcome.ok) return;
    expect(outcome.reason).toContain("salud");
  });
});

describe("migraciones", () => {
  it("un save de una versión futura se rechaza en vez de degradarse", () => {
    // Pasa si alguien abrió una versión más nueva del juego y volvió a una
    // vieja. Interpretarlo a ciegas rompería datos.
    const outcome = parseSave({ version: 99, guardadoMs: T0, criatura: creature });
    expect(outcome.ok).toBe(false);
    if (outcome.ok) return;
    expect(outcome.reason).toContain("más nueva");
  });

  it("una versión inválida se rechaza", () => {
    expect(() => applyMigrations({}, 0, 1)).toThrow(SaveVersionError);
    expect(() => applyMigrations({}, -3, 1)).toThrow(SaveVersionError);
  });

  it("sin migración disponible falla de forma explícita", () => {
    // Prefiero un error claro a un save silenciosamente mal interpretado.
    expect(() => applyMigrations({ version: 1 }, 1, 3, [])).toThrow(/Falta la migración/);
  });

  /**
   * La cadena se inyecta en vez de importarse para poder verificar el mecanismo
   * de verdad. Hoy `MIGRATIONS` está vacía porque la versión actual es la 1;
   * testear contra la constante real no probaría nada.
   */
  it("aplica la cadena en orden, una versión por vez", () => {
    const trace: number[] = [];
    const chain: Migration[] = [
      (save: RawSave) => {
        trace.push(1);
        return { ...save, agregadoEnV2: true };
      },
      (save: RawSave) => {
        trace.push(2);
        return { ...save, agregadoEnV3: true };
      },
      (save: RawSave) => {
        trace.push(3);
        return { ...save, agregadoEnV4: true };
      },
    ];

    const result = applyMigrations({ version: 1, dato: "original" }, 1, 4, chain);

    expect(trace).toEqual([1, 2, 3]);
    expect(result).toMatchObject({
      version: 4,
      dato: "original",
      agregadoEnV2: true,
      agregadoEnV3: true,
      agregadoEnV4: true,
    });
  });

  it("arrancar desde una versión intermedia salta las migraciones ya aplicadas", () => {
    const trace: number[] = [];
    const chain: Migration[] = [
      (save) => {
        trace.push(1);
        return save;
      },
      (save) => {
        trace.push(2);
        return save;
      },
    ];

    applyMigrations({ version: 2 }, 2, 3, chain);
    expect(trace).toEqual([2]);
  });

  it("migrar a la misma versión no hace nada", () => {
    const trace: number[] = [];
    const chain: Migration[] = [
      (save) => {
        trace.push(1);
        return save;
      },
    ];
    const result = applyMigrations({ version: 1, dato: 7 }, 1, 1, chain);
    expect(trace).toEqual([]);
    expect(result).toEqual({ version: 1, dato: 7 });
  });
});
