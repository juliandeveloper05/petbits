import { describe, expect, it } from "vitest";
import { alimentar, jugar } from "../src/core/actions.ts";
import {
  DESTINOS,
  describirBotin,
  destinoPorId,
  enviar,
  estaFuera,
  faltaParaVolver,
  puedeSalir,
  recibir,
  resolverBotin,
} from "../src/core/expeditions.ts";
import {
  agregar,
  agregarVarios,
  consumir,
  cuanto,
  hay,
  inventarioInicial,
  total,
} from "../src/core/inventory.ts";
import { type CreatureState, createCreature } from "../src/core/simulation.ts";

const SEED = 0xa3f091c477be2d08n;
const T0 = Date.UTC(2026, 2, 2, 9, 0, 0);
const MINUTO = 60_000;

function criatura(extra: Partial<CreatureState> = {}): CreatureState {
  const base = createCreature(SEED, T0, 0);
  return { ...base, stats: { ...base.stats, energia: 80 }, ...extra };
}

const PATIO = destinoPorId("patio");
const BOSQUE = destinoPorId("bosque");
const RUINAS = destinoPorId("ruinas");
if (!PATIO || !BOSQUE || !RUINAS) throw new Error("faltan destinos");

describe("despensa", () => {
  it("arranca con algo para dar de comer", () => {
    const inv = inventarioInicial();
    expect(total(inv)).toBeGreaterThan(0);
    // El cristal empieza en cero: es lo raro y tiene que sentirse así.
    expect(cuanto(inv, "cristal")).toBe(0);
  });

  it("consumir descuenta una unidad sin mutar el original", () => {
    const antes = inventarioInicial();
    const copia = JSON.stringify(antes);
    const despues = consumir(antes, "baya");

    expect(JSON.stringify(antes)).toBe(copia);
    expect(cuanto(despues ?? {}, "baya")).toBe(cuanto(antes, "baya") - 1);
  });

  it("consumir devuelve null si no hay", () => {
    // Devolver null en vez de un inventario en cero es lo que permite que quien
    // llama no descuente comida cuando la acción que venía después falla.
    expect(consumir(inventarioInicial(), "cristal")).toBeNull();
    expect(hay(inventarioInicial(), "cristal")).toBe(false);
  });

  it("no se puede bajar de cero a fuerza de consumir", () => {
    let inv = inventarioInicial();
    for (let i = 0; i < 20; i++) {
      const siguiente = consumir(inv, "baya");
      if (!siguiente) break;
      inv = siguiente;
    }
    expect(cuanto(inv, "baya")).toBe(0);
  });

  it("agregarVarios suma un botín entero", () => {
    const inv = agregarVarios(inventarioInicial(), { baya: 2, cristal: 1 });
    expect(cuanto(inv, "baya")).toBe(5);
    expect(cuanto(inv, "cristal")).toBe(1);
  });

  it("agregar ignora cantidades no positivas", () => {
    const inv = inventarioInicial();
    expect(agregar(inv, "baya", 0)).toEqual(inv);
    expect(agregar(inv, "baya", -3)).toEqual(inv);
  });
});

describe("no quedarse trabado", () => {
  /**
   * La regla que evita el callejón sin salida.
   *
   * Si la comida se acaba y para conseguir más hiciera falta comida, el jugador
   * quedaría sin nada que hacer. El patio es el piso: sin etapa mínima, sin
   * costo de energía y siempre trae algo.
   */
  it("el patio está disponible para un bebé exhausto", () => {
    const agotada = criatura({ etapa: "bebe", stats: { ...criatura().stats, energia: 0 } });
    expect(puedeSalir(agotada, PATIO, T0)).toEqual({ puede: true });
    expect(PATIO.costoEnergia).toBe(0);
  });

  it("el patio siempre trae al menos un alimento", () => {
    for (let i = 0; i < 200; i++) {
      const botin = resolverBotin(SEED, PATIO, T0 + i * MINUTO);
      expect(total(botin.alimentos), `salida ${i} volvió vacía`).toBeGreaterThan(0);
    }
  });

  it("los destinos que valen la pena sí piden condiciones", () => {
    const bebe = criatura({ etapa: "bebe" });
    expect(puedeSalir(bebe, BOSQUE, T0).puede).toBe(false);
    expect(puedeSalir(bebe, RUINAS, T0).puede).toBe(false);

    const sinEnergia = criatura({ etapa: "adulto", stats: { ...criatura().stats, energia: 5 } });
    const veredicto = puedeSalir(sinEnergia, RUINAS, T0);
    expect(veredicto.puede).toBe(false);
    if (veredicto.puede) return;
    expect(veredicto.motivo).toContain("energía");
  });
});

describe("salir y volver", () => {
  it("salir cuesta energía y marca la ausencia", () => {
    const antes = criatura({ etapa: "adulto" });
    const fuera = enviar(antes, RUINAS, T0);

    expect(estaFuera(fuera)).toBe(true);
    expect(fuera.stats.energia).toBe(antes.stats.energia - RUINAS.costoEnergia);
    expect(fuera.expedicion?.regresoMs).toBe(T0 + RUINAS.duracionMs);
    // Y no muta la original.
    expect(estaFuera(antes)).toBe(false);
  });

  it("no vuelve antes de tiempo", () => {
    const fuera = enviar(criatura(), PATIO, T0);
    expect(recibir(fuera, T0 + 5 * MINUTO)).toBeNull();
    expect(faltaParaVolver(fuera, T0 + 5 * MINUTO)).toBe(10 * MINUTO);
  });

  it("al volver trae el botín y queda libre", () => {
    const fuera = enviar(criatura(), PATIO, T0);
    const regreso = recibir(fuera, T0 + PATIO.duracionMs);

    expect(regreso).not.toBeNull();
    if (!regreso) return;
    expect(estaFuera(regreso.criatura)).toBe(false);
    expect(total(regreso.botin.alimentos)).toBeGreaterThan(0);
    // Salir a laburar cuenta como atención: no estuvo abandonada.
    expect(regreso.criatura.ticksSinCuidado).toBe(0);
  });

  it("una criatura en casa no vuelve de ningún lado", () => {
    expect(recibir(criatura(), T0)).toBeNull();
  });

  it("no se puede mandar a dos lados a la vez", () => {
    const fuera = enviar(criatura({ etapa: "adulto" }), PATIO, T0);
    const veredicto = puedeSalir(fuera, BOSQUE, T0);
    expect(veredicto.puede).toBe(false);
    if (veredicto.puede) return;
    expect(veredicto.motivo).toContain("afuera");
  });

  it("un destino que ya no existe no la deja atrapada", () => {
    // Pasaría si una actualización del juego sacara un destino con criaturas
    // en camino. Vuelve con las manos vacías, pero vuelve.
    const perdida = {
      ...criatura(),
      expedicion: { destinoId: "lugar-borrado", salidaMs: T0, regresoMs: T0 + 1000 },
    };
    const regreso = recibir(perdida, T0 + 2000);
    expect(regreso).not.toBeNull();
    if (!regreso) return;
    expect(estaFuera(regreso.criatura)).toBe(false);
  });
});

describe("el botín ya está decidido cuando sale", () => {
  /**
   * Determinista a partir del genoma y del momento de salida.
   *
   * Si dependiera de cuándo mirás, cerrar y reabrir hasta que salga un cristal
   * sería la estrategia óptima. Así, el resultado queda sellado al salir.
   */
  it("el mismo viaje da siempre el mismo botín", () => {
    const a = resolverBotin(SEED, RUINAS, T0);
    const b = resolverBotin(SEED, RUINAS, T0);
    expect(b).toEqual(a);
  });

  it("viajes distintos dan botines distintos", () => {
    // La semilla es un bigint y JSON.stringify no sabe serializarlos, así que
    // la clave se arma a mano.
    const clave = (i: number) => {
      const botin = resolverBotin(SEED, RUINAS, T0 + i * MINUTO);
      return `${JSON.stringify(botin.alimentos)}|${botin.semilla ?? "sin"}`;
    };
    const botines = new Set([0, 1, 2, 3, 4, 5].map(clave));
    expect(botines.size).toBeGreaterThan(1);
  });

  it("respeta la cantidad de items del destino", () => {
    for (let i = 0; i < 100; i++) {
      const botin = resolverBotin(SEED, BOSQUE, T0 + i * MINUTO);
      const [minimo, maximo] = BOSQUE.items;
      expect(total(botin.alimentos)).toBeGreaterThanOrEqual(minimo);
      expect(total(botin.alimentos)).toBeLessThanOrEqual(maximo);
    }
  });

  it("solo trae alimentos que ese destino puede dar", () => {
    const permitidos = Object.keys(RUINAS.pesos);
    for (let i = 0; i < 100; i++) {
      for (const id of Object.keys(resolverBotin(SEED, RUINAS, T0 + i * MINUTO).alimentos)) {
        expect(permitidos).toContain(id);
      }
    }
  });

  it("las semillas aparecen con la frecuencia declarada", () => {
    const MUESTRAS = 600;
    let conSemilla = 0;
    for (let i = 0; i < MUESTRAS; i++) {
      if (resolverBotin(SEED, RUINAS, T0 + i * MINUTO).semilla !== null) conSemilla++;
    }
    const tasa = conSemilla / MUESTRAS;
    expect(tasa).toBeGreaterThan(RUINAS.chanceSemilla * 0.6);
    expect(tasa).toBeLessThan(RUINAS.chanceSemilla * 1.4);
  });

  it("el patio nunca trae semillas", () => {
    for (let i = 0; i < 100; i++) {
      expect(resolverBotin(SEED, PATIO, T0 + i * MINUTO).semilla).toBeNull();
    }
  });

  it("las semillas que trae son genomas válidos de 64 bits", () => {
    for (let i = 0; i < 300; i++) {
      const { semilla } = resolverBotin(SEED, RUINAS, T0 + i * MINUTO);
      if (semilla === null) continue;
      expect(semilla).toBeGreaterThanOrEqual(0n);
      expect(semilla).toBeLessThan(2n ** 64n);
    }
  });
});

describe("mientras está afuera", () => {
  it("no se la puede alimentar, ni jugar con ella", () => {
    const fuera = enviar(criatura({ etapa: "adulto" }), RUINAS, T0);

    const comida = alimentar(fuera, "larva", T0);
    expect(comida.ok).toBe(false);
    if (comida.ok) return;
    expect(comida.reason).toContain("expedición");

    expect(jugar(fuera, T0).ok).toBe(false);
  });

  it("de vuelta en casa se la puede volver a atender", () => {
    const fuera = enviar(criatura({ etapa: "adulto" }), PATIO, T0);
    const regreso = recibir(fuera, T0 + PATIO.duracionMs);
    expect(regreso).not.toBeNull();
    if (!regreso) return;
    expect(alimentar(regreso.criatura, "larva", T0).ok).toBe(true);
  });
});

describe("catálogo", () => {
  it("los destinos tienen ids únicos y datos coherentes", () => {
    const ids = DESTINOS.map((d) => d.id);
    expect(new Set(ids).size).toBe(ids.length);

    for (const destino of DESTINOS) {
      expect(destino.duracionMs).toBeGreaterThan(0);
      expect(destino.items[0]).toBeGreaterThan(0);
      expect(destino.items[1]).toBeGreaterThanOrEqual(destino.items[0]);
      expect(Object.keys(destino.pesos).length).toBeGreaterThan(0);
      // La contracción va escrita, no concatenada: "de" + "el patio" daría
      // "de el patio", que no es castellano.
      expect(destino.desde, `${destino.id} sin frase de regreso`).toMatch(/^(del|de la|de las) /);
    }
  });

  it("describirBotin arma una frase legible", () => {
    const nombres = { baya: "Baya", raiz: "Raíz", larva: "Larva", cristal: "Cristal" };
    const texto = describirBotin({ alimentos: { baya: 2 }, semilla: 5n }, nombres);
    expect(texto).toContain("2 baya");
    expect(texto).toContain("semilla");

    expect(describirBotin({ alimentos: {}, semilla: null }, nombres)).toContain("vacías");
  });
});
