import { describe, expect, it } from "vitest";
import {
  type Codex,
  codexInicial,
  conoceForma,
  conoceLinaje,
  progresoCodex,
  registrar,
} from "../src/core/codex.ts";
import { COLLECTIBLE_FORMS } from "../src/core/evolution.ts";
import { LINEAGES, decodeGenome } from "../src/core/genome.ts";
import { TRAIT_CATALOG } from "../src/core/traits.ts";

/** Genoma con popcount 32 y los 16 nibbles distintos: Equilibrado + Pangrama. */
const SEED_RARO = 0x0f1e2d3c4b5a6978n;
const SEED_A = 0xa3f091c477be2d08n;

describe("registro", () => {
  it("un codex nuevo está vacío", () => {
    const codex = codexInicial();
    expect(codex.linajes).toEqual([]);
    expect(codex.formas).toEqual([]);
    expect(codex.rarezas).toEqual([]);
    expect(codex.totalRegistradas).toBe(0);
  });

  it("anota linaje, forma y rarezas, y avisa qué fue novedad", () => {
    const { codex, nuevos } = registrar(codexInicial(), SEED_RARO, "coloso");

    expect(codex.linajes).toContain(decodeGenome(SEED_RARO).lineage);
    expect(codex.formas).toContain("coloso");
    expect(codex.rarezas).toContain("equilibrado");
    expect(codex.rarezas).toContain("pangrama");
    expect(codex.totalRegistradas).toBe(1);

    const tipos = nuevos.map((n) => n.tipo);
    expect(tipos).toContain("linaje");
    expect(tipos).toContain("forma");
    expect(tipos).toContain("rareza");
    // Los nombres son los que va a mostrar la UI, no los ids internos.
    expect(nuevos.find((n) => n.tipo === "forma")?.nombre).toBe("Coloso");
  });

  it("registrar lo mismo dos veces no reporta novedades", () => {
    // Es lo que permite llamarlo en cada carga sin inundar al jugador de avisos.
    const primera = registrar(codexInicial(), SEED_RARO, "coloso");
    const segunda = registrar(primera.codex, SEED_RARO, "coloso");

    expect(segunda.nuevos).toEqual([]);
    expect(segunda.codex.linajes).toEqual(primera.codex.linajes);
    expect(segunda.codex.rarezas).toEqual(primera.codex.rarezas);
    // Pero el contador de registradas sí sube: cuenta criaturas, no especies.
    expect(segunda.codex.totalRegistradas).toBe(2);
  });

  it("no registra la forma indefinida", () => {
    // No es una forma alcanzada, es la ausencia de una. Registrarla haría que
    // el codex arrancara con algo hecho.
    const { codex, nuevos } = registrar(codexInicial(), SEED_A, "indefinida");
    expect(codex.formas).toEqual([]);
    expect(nuevos.some((n) => n.tipo === "forma")).toBe(false);
  });

  it("no muta el codex que recibe", () => {
    const original = codexInicial();
    const copia = JSON.stringify(original);
    registrar(original, SEED_RARO, "coloso");
    expect(JSON.stringify(original)).toBe(copia);
  });

  it("mantiene las listas ordenadas", () => {
    // Si no, dos partidas equivalentes producen JSON distinto y cualquier
    // comparación de guardados miente.
    let codex: Codex = codexInicial();
    for (const seed of [0xf0n, 0x0an, 0x5cn, 0x21n]) {
      codex = registrar(codex, seed, "errante").codex;
    }
    expect(codex.linajes).toEqual([...codex.linajes].sort((a, b) => a - b));
    expect(codex.rarezas).toEqual([...codex.rarezas].sort());
  });
});

describe("consultas", () => {
  it("conoceLinaje y conoceForma responden lo registrado", () => {
    const { codex } = registrar(codexInicial(), SEED_A, "oraculo");
    const genes = decodeGenome(SEED_A);

    expect(conoceLinaje(codex, genes.lineage)).toBe(true);
    expect(conoceForma(codex, "oraculo")).toBe(true);
    expect(conoceForma(codex, "coloso")).toBe(false);
  });
});

describe("progreso", () => {
  it("un codex vacío está en cero", () => {
    const progreso = progresoCodex(codexInicial());
    expect(progreso.porcentaje).toBe(0);
    expect(progreso.linajes).toEqual({ vistos: 0, total: LINEAGES.length });
    expect(progreso.formas.total).toBe(COLLECTIBLE_FORMS.length);
    expect(progreso.rarezas.total).toBe(TRAIT_CATALOG.length);
  });

  it("sube a medida que se descubren cosas", () => {
    const vacio = progresoCodex(codexInicial());
    const conUna = progresoCodex(registrar(codexInicial(), SEED_RARO, "coloso").codex);
    expect(conUna.porcentaje).toBeGreaterThan(vacio.porcentaje);
  });

  it("nunca llega a 100 por accidente", () => {
    // Las dos legendarias entran en el total aunque sean casi inalcanzables
    // —Pangrama es 1 en 880.000—. Un codex que se completa deja de dar motivo
    // para seguir mirando.
    let codex: Codex = codexInicial();
    for (let i = 0; i < 200; i++) {
      codex = registrar(codex, BigInt(i) * 0x9e3779b97f4a7c15n, "coloso").codex;
    }
    expect(progresoCodex(codex).porcentaje).toBeLessThan(100);
  });
});
