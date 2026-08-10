import { describe, expect, it } from "vitest";
import { codexInicial } from "../src/core/codex.ts";
import { createCreature } from "../src/core/simulation.ts";
import {
  type Migration,
  type RawSave,
  SaveVersionError,
  applyMigrations,
} from "../src/state/migrations.ts";
import {
  SAVE_VERSION,
  createSave,
  criaturaActiva,
  parseSave,
  partidaInicial,
  reemplazarCriatura,
} from "../src/state/save.ts";

const T0 = Date.UTC(2026, 2, 2, 9, 0, 0);
const creature = createCreature(0xa3f091c477be2d08n, T0, -180);
const partida = partidaInicial(creature);

describe("ida y vuelta del guardado", () => {
  it("un save válido sobrevive a JSON", () => {
    const save = createSave(partida, T0);
    // El viaje real: IndexedDB estructura el objeto, pero JSON es el caso
    // más hostil y si pasa por acá pasa por cualquier lado.
    const outcome = parseSave(JSON.parse(JSON.stringify(save)));

    expect(outcome.ok, outcome.ok ? "" : outcome.reason).toBe(true);
    if (!outcome.ok) return;
    expect(outcome.save.criaturas).toHaveLength(1);
    expect(outcome.save.criaturas[0]).toEqual(creature);
    expect(outcome.save.activaId).toBe(creature.id);
    expect(outcome.save.version).toBe(SAVE_VERSION);
  });

  it("el genoma viaja como texto porque JSON no sabe de bigint", () => {
    const save = createSave(partida, T0);
    expect(typeof save.criaturas[0]?.seed).toBe("string");
    expect(BigInt(save.criaturas[0]?.seed ?? "0")).toBe(0xa3f091c477be2d08n);
  });

  it("el id distingue criaturas que comparten genoma", () => {
    // Al cruzar puede repetirse el genoma, así que la semilla no alcanza
    // como identificador.
    const gemela = createCreature(0xa3f091c477be2d08n, T0 + 1000, -180);
    expect(gemela.seed).toBe(creature.seed);
    expect(gemela.id).not.toBe(creature.id);
  });
});

describe("manejo de la colección", () => {
  it("criaturaActiva devuelve la que corresponde", () => {
    expect(criaturaActiva(partida)).toEqual(creature);
  });

  it("reemplazarCriatura no muta el estado anterior", () => {
    const antes = JSON.stringify(partida);
    const crecida = { ...creature, etapa: "adulto" as const };
    const despues = reemplazarCriatura(partida, crecida);

    expect(JSON.stringify(partida)).toBe(antes);
    expect(criaturaActiva(despues)?.etapa).toBe("adulto");
  });

  it("rechaza un save cuya criatura activa no existe", () => {
    // Sin esta validación la partida cargaría sin nada que mostrar.
    const save = createSave(partida, T0);
    const roto = { ...save, activaId: "no-existe" };
    const outcome = parseSave(JSON.parse(JSON.stringify(roto)));

    expect(outcome.ok).toBe(false);
    if (outcome.ok) return;
    expect(outcome.reason).toContain("activaId");
  });

  it("rechaza una colección vacía", () => {
    const roto = {
      version: SAVE_VERSION,
      guardadoMs: T0,
      criaturas: [],
      activaId: "x",
      codex: codexInicial(),
    };
    expect(parseSave(roto).ok).toBe(false);
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
    ["sin versión", { criaturas: [creature], activaId: creature.id }],
    ["versión en texto", { version: "3", criaturas: [creature] }],
    ["sin criaturas", { version: SAVE_VERSION, guardadoMs: T0 }],
    [
      "criatura incompleta",
      { version: SAVE_VERSION, guardadoMs: T0, criaturas: [{ id: "a", seed: "1" }], activaId: "a" },
    ],
    [
      "genoma no numérico",
      {
        version: SAVE_VERSION,
        guardadoMs: T0,
        criaturas: [{ ...creature, seed: "abc" }],
        activaId: creature.id,
        codex: codexInicial(),
      },
    ],
    [
      "estadística fuera de rango",
      {
        version: SAVE_VERSION,
        guardadoMs: T0,
        criaturas: [{ ...creature, stats: { ...creature.stats, energia: 9999 } }],
        activaId: creature.id,
        codex: codexInicial(),
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
      version: SAVE_VERSION,
      guardadoMs: T0,
      criaturas: [{ ...creature, stats: { ...creature.stats, salud: -5 } }],
      activaId: creature.id,
      codex: codexInicial(),
    });
    expect(outcome.ok).toBe(false);
    if (outcome.ok) return;
    expect(outcome.reason).toContain("salud");
  });
});

describe("migración en cadena desde v1", () => {
  /**
   * Un guardado con la forma exacta que tenía la versión 1: sin crianza, sin
   * etapa, sin forma evolutiva, sin id y con una sola criatura suelta.
   *
   * Se escribe a mano en vez de derivarlo del estado actual a propósito. Si se
   * construyera quitándole campos a `createCreature`, el día que cambie el
   * estado el test mutaría con él y dejaría de probar la migración de la v1
   * real.
   */
  function saveV1(ticksVividos: number): unknown {
    return {
      version: 1,
      guardadoMs: T0,
      criatura: {
        seed: "11817069912113213192",
        nacimientoMs: T0,
        lastTickMs: T0 + ticksVividos * 60_000,
        ticksVividos,
        tzOffsetMin: -180,
        stats: { energia: 62.5, animo: 48, salud: 91, vinculo: 14 },
        vinculoHoy: 4,
        diaIndice: 20514,
        ticksSinCuidado: 30,
        letargico: false,
        durmiendo: false,
      },
    };
  }

  it("un save v1 atraviesa las dos migraciones y valida", () => {
    const outcome = parseSave(saveV1(100));
    expect(outcome.ok, outcome.ok ? "" : outcome.reason).toBe(true);
    if (!outcome.ok) return;

    expect(outcome.save.version).toBe(SAVE_VERSION);
    expect(outcome.save.criaturas).toHaveLength(1);
    // Lo que ya existía se conserva intacto a través de toda la cadena.
    expect(outcome.save.criaturas[0]?.stats.vinculo).toBe(14);
    expect(outcome.save.criaturas[0]?.ticksVividos).toBe(100);
  });

  it("reconstruye la etapa a partir de los ticks vividos", () => {
    const etapaDe = (ticks: number) => {
      const outcome = parseSave(saveV1(ticks));
      return outcome.ok ? outcome.save.criaturas[0]?.etapa : "?";
    };

    expect(etapaDe(100)).toBe("bebe");
    expect(etapaDe(2000)).toBe("juvenil");
    expect(etapaDe(9000)).toBe("adulto");
  });

  it("el id que inventa la migración apunta a la criatura activa", () => {
    // Si no coincidieran, el esquema rechazaría el save y se perdería la
    // partida de quien viniera de una versión vieja.
    const outcome = parseSave(saveV1(500));
    expect(outcome.ok).toBe(true);
    if (!outcome.ok) return;
    expect(outcome.save.criaturas[0]?.id).toBe(outcome.save.activaId);
  });

  it("el codex arranca vacío: la v1 no registraba nada de eso", () => {
    const outcome = parseSave(saveV1(9000));
    expect(outcome.ok).toBe(true);
    if (!outcome.ok) return;
    expect(outcome.save.codex).toEqual(codexInicial());
  });

  it("llega hasta v4 con el descanso de cruza sin usar", () => {
    // `null` y no 0: nunca haber cruzado no es lo mismo que haber cruzado en
    // la época Unix.
    const outcome = parseSave(saveV1(9000));
    expect(outcome.ok).toBe(true);
    if (!outcome.ok) return;
    expect(outcome.save.criaturas[0]?.ultimaCruzaMs).toBeNull();
  });
});

describe("migración v3 → v4", () => {
  /** Un guardado con la forma exacta de la v3: colección y codex, sin cruza. */
  function saveV3(): unknown {
    const { ultimaCruzaMs: _sin, ...sinCruza } = creature;
    return {
      version: 3,
      guardadoMs: T0,
      criaturas: [sinCruza],
      activaId: creature.id,
      codex: codexInicial(),
    };
  }

  it("agrega el descanso a todas las criaturas", () => {
    const outcome = parseSave(saveV3());
    expect(outcome.ok, outcome.ok ? "" : outcome.reason).toBe(true);
    if (!outcome.ok) return;

    expect(outcome.save.version).toBe(SAVE_VERSION);
    expect(outcome.save.criaturas[0]?.ultimaCruzaMs).toBeNull();
    // Y no toca nada de lo que ya estaba.
    expect(outcome.save.criaturas[0]?.seed).toBe(creature.seed);
    expect(outcome.save.activaId).toBe(creature.id);
  });
});

describe("mecánica de migraciones", () => {
  it("un save de una versión futura se rechaza en vez de degradarse", () => {
    // Pasa si alguien abrió una versión más nueva del juego y volvió a una
    // vieja. Interpretarlo a ciegas rompería datos.
    const outcome = parseSave({ version: 99, guardadoMs: T0, criaturas: [] });
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
