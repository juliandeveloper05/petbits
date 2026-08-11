/**
 * Generador de vectores de paridad TypeScript → C++.
 *
 *   npm run parity
 *   npm run parity -- --seeds 5000
 *
 * ---
 *
 * POR QUÉ ESTO EXISTE, Y NO UN TEST ESCRITO A MANO.
 *
 * La versión anterior de test_parity.cpp tenía líneas así:
 *
 *     REQUIRE(g.hue == 0xBE);  // bits 24-31 de 0xA3F091C477BE2D08
 *
 * Ese número no salió de correr el TypeScript: salió de que alguien leyó el
 * código y calculó a mano. Y ahí está el problema — si la lectura fue igual de
 * equivocada que el port, el test pasa y la paridad sigue rota. Un test cuyos
 * valores esperados los escribió la misma cabeza que escribió la
 * implementación no verifica nada: confirma.
 *
 * Este script ejecuta el TypeScript de verdad, el mismo que corre en
 * producción, y vuelca lo que devuelve. Los valores esperados dejan de ser una
 * opinión sobre lo que el TS hace y pasan a ser lo que el TS hace.
 *
 * Los seeds no son aleatorios: salen de splitmix64 con una constante fija, así
 * que regenerar el archivo dos veces da exactamente lo mismo y el diff está
 * vacío mientras nada cambie. Un cambio en el header es señal de que cambió el
 * comportamiento del TS, y eso merece mirarse.
 */

import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { type Crianza, resolverAdulto, resolverJuvenil } from "../src/core/evolution.ts";
import { type Genes, decodeGenome, formatSeed, hashString } from "../src/core/genome.ts";
import { splitmix64 } from "../src/core/rng.ts";
import { TRAIT_CATALOG, detectTraits, rarityTier } from "../src/core/traits.ts";

function arg(name: string, fallback: number): number {
  const index = process.argv.indexOf(`--${name}`);
  if (index === -1) return fallback;
  const value = Number(process.argv[index + 1]);
  return Number.isFinite(value) ? value : fallback;
}

const CANTIDAD = arg("seeds", 2000);
const SALIDA = resolve(import.meta.dirname, "../gdext/tests/vectores_generados.h");

/**
 * Seeds que conviene tener sí o sí.
 *
 * Los aleatorios cubren el caso típico y son malísimos para los bordes: entre
 * dos mil seeds al azar no aparece ni el cero, ni el máximo, ni un pangrama, ni
 * el primo más grande que entra en 64 bits. Justo los valores donde un port se
 * rompe.
 */
const BORDES: readonly bigint[] = [
  0n,
  1n,
  0xffffffffffffffffn, // todos los bits en 1
  0x5555555555555555n, // 32 bits en 1 → Equilibrado
  0xaaaaaaaaaaaaaaaan, // 32 bits en 1, complemento del anterior
  0xfedcba9876543210n, // los 16 nibbles distintos → Pangrama
  0xa3f091c477be2d08n, // el seed de ejemplo del README
  18446744073709551557n, // el primo más grande por debajo de 2^64
  0x00000000000001ffn, // 9 unos consecutivos → Racha
  0x8000000000000001n, // Uróboros: primer byte = último
];

/**
 * Los ids del catálogo, en el orden en que están declarados.
 *
 * Ese orden es el contrato con el C++: el bit i de la máscara es
 * TRAIT_CATALOG[i] de los dos lados. Si algún día se agrega una rareza, va al
 * final o los dos catálogos dejan de hablar del mismo bit.
 */
const IDS_RAREZAS = TRAIT_CATALOG.map((r) => r.id);

/**
 * Las rarezas como bitmask.
 *
 * Comparar listas de strings entre lenguajes trae problemas que no tienen nada
 * que ver con lo que se quiere verificar (orden, acentos, codificación del
 * archivo). Un entero de 8 bits o coincide o no.
 */
function mascaraRarezas(seed: bigint): number {
  const presentes = new Set(detectTraits(seed).map((t) => t.id));
  let mascara = 0;
  IDS_RAREZAS.forEach((id, i) => {
    if (presentes.has(id)) mascara |= 1 << i;
  });
  return mascara;
}

const TIERS = ["comun", "raro", "epico", "legendario"] as const;

function generarSeeds(cantidad: number): bigint[] {
  const seeds = [...BORDES];
  let estado = 0x9e3779b97f4a7c15n;
  for (let i = 0; i < cantidad; i++) {
    estado = splitmix64(estado);
    seeds.push(estado);
  }
  return seeds;
}

/**
 * Crianzas de prueba.
 *
 * Incluye a propósito casos con más caricias que juego. Ahí es donde el port
 * fallaba: en C++ los contadores son enteros sin signo y la resta daba la
 * vuelta, así que toda criatura calma terminaba evolucionando a la forma
 * activa. Un caso con juego > calma nunca lo habría mostrado.
 */
type CrianzaParcial = Omit<Partial<Crianza>, "dieta"> & { dieta?: Partial<Crianza["dieta"]> };

function crianza(parcial: CrianzaParcial): Crianza {
  return {
    dieta: { proteina: 0, dulce: 0, mineral: 0, raro: 0, ...parcial.dieta },
    juego: parcial.juego ?? 0,
    calma: parcial.calma ?? 0,
    sumaAnimo: parcial.sumaAnimo ?? 0,
    sumaSalud: parcial.sumaSalud ?? 0,
    ticksMedidos: parcial.ticksMedidos ?? 0,
  };
}

const CRIANZAS: readonly { nombre: string; c: Crianza }[] = [
  { nombre: "vacia", c: crianza({}) },
  { nombre: "proteina y juego", c: crianza({ dieta: { proteina: 50 }, juego: 20 }) },
  { nombre: "dulce y calma", c: crianza({ dieta: { dulce: 50 }, calma: 20 }) },
  { nombre: "solo calma", c: crianza({ calma: 20 }) },
  { nombre: "solo juego", c: crianza({ juego: 20 }) },
  { nombre: "mineral y calma", c: crianza({ dieta: { mineral: 30 }, calma: 5 }) },
  { nombre: "raro y juego", c: crianza({ dieta: { raro: 30 }, juego: 5 }) },
  { nombre: "empate seco", c: crianza({ dieta: { proteina: 10, dulce: 10 }, juego: 7, calma: 7 }) },
  {
    nombre: "animo alto sin jugar",
    c: crianza({ dieta: { proteina: 5 }, sumaAnimo: 9000, sumaSalud: 9000, ticksMedidos: 100 }),
  },
  {
    nombre: "animo bajo con juego",
    c: crianza({
      dieta: { proteina: 5 },
      juego: 2,
      sumaAnimo: 500,
      sumaSalud: 5000,
      ticksMedidos: 100,
    }),
  },
];

const FORMAS = ["indefinida", "petreo", "vaporoso", "coloso", "guardian", "errante", "oraculo"];

function indiceForma(forma: string): number {
  const i = FORMAS.indexOf(forma);
  if (i === -1) throw new Error(`Forma desconocida: ${forma}`);
  return i;
}

function hex(valor: bigint): string {
  return `0x${valor.toString(16).toUpperCase().padStart(16, "0")}ULL`;
}

// ---------------------------------------------------------------------------
// Emisión
// ---------------------------------------------------------------------------

const seeds = generarSeeds(CANTIDAD);
const lineas: string[] = [];

lineas.push("#pragma once");
lineas.push("// ARCHIVO GENERADO — no editar a mano.");
lineas.push("//");
lineas.push("// Lo produce `npm run parity` ejecutando el TypeScript de src/core/.");
lineas.push("// Cada valor de acá salió de correr el código real, no de leerlo.");
lineas.push("//");
lineas.push(`// ${seeds.length} genomas (${BORDES.length} de borde + ${CANTIDAD} de splitmix64)`);
lineas.push(`// ${CRIANZAS.length} crianzas x 8 genomas de afinidad`);
lineas.push("");
lineas.push("#include <cstdint>");
lineas.push("");
lineas.push("namespace petbits::vectores {");
lineas.push("");

// --- genoma + rarezas ---
lineas.push("struct VectorGenoma {");
lineas.push("    uint64_t seed;");
lineas.push("    uint8_t  lineage, bodyShape, eyes, mouth, appendages, pattern;");
lineas.push("    uint8_t  hue, paletteMode, temperament, metabolism, affinity, proportion;");
lineas.push("    uint8_t  vigor, animo, ingenio, vinculo;");
lineas.push("    uint8_t  mutation;");
lineas.push("    uint8_t  rarezas;      ///< bitmask, bit i = TRAIT_CATALOG[i]");
lineas.push("    uint8_t  tier;         ///< 0 comun, 1 raro, 2 epico, 3 legendario");
lineas.push("    const char* seedFormateado;");
lineas.push("};");
lineas.push("");
lineas.push("inline constexpr VectorGenoma GENOMAS[] = {");

for (const seed of seeds) {
  const g: Genes = decodeGenome(seed);
  const s = g.statBias;
  const tier = TIERS.indexOf(rarityTier(detectTraits(seed)));
  const campos = [
    hex(seed),
    g.lineage,
    g.bodyShape,
    g.eyes,
    g.mouth,
    g.appendages,
    g.pattern,
    g.hue,
    g.paletteMode,
    g.temperament,
    g.metabolism,
    g.affinity,
    g.proportion,
    s.vigor,
    s.animo,
    s.ingenio,
    s.vinculo,
    g.mutation,
    mascaraRarezas(seed),
    tier,
    `"${formatSeed(seed)}"`,
  ];
  lineas.push(`    {${campos.join(", ")}},`);
}

lineas.push("};");
lineas.push("");

// --- evolución ---
lineas.push("struct VectorEvolucion {");
lineas.push("    uint32_t proteina, dulce, mineral, raro;");
lineas.push("    uint32_t juego, calma;");
lineas.push("    double   sumaAnimo, sumaSalud;");
lineas.push("    uint64_t ticksMedidos;");
lineas.push("    uint8_t  affinity;");
lineas.push("    uint8_t  juvenil;   ///< índice en FORMAS");
lineas.push("    uint8_t  adulto;");
lineas.push("    const char* nombre;");
lineas.push("};");
lineas.push("");
lineas.push("inline constexpr VectorEvolucion EVOLUCIONES[] = {");

for (const { nombre, c } of CRIANZAS) {
  // El gen de afinidad sesga el eje somático, así que se recorren los ocho.
  for (let affinity = 0; affinity < 8; affinity++) {
    const genes = { affinity } as Genes;
    const juvenil = resolverJuvenil(c, genes);
    const adulto = resolverAdulto(c, genes, juvenil);
    const campos = [
      c.dieta.proteina,
      c.dieta.dulce,
      c.dieta.mineral,
      c.dieta.raro,
      c.juego,
      c.calma,
      c.sumaAnimo.toFixed(1),
      c.sumaSalud.toFixed(1),
      `${c.ticksMedidos}ULL`,
      affinity,
      indiceForma(juvenil),
      indiceForma(adulto),
      `"${nombre} / afinidad ${affinity}"`,
    ];
    lineas.push(`    {${campos.join(", ")}},`);
  }
}

lineas.push("};");
lineas.push("");

// --- hashString ---
lineas.push("struct VectorHash {");
lineas.push("    const char* texto;");
lineas.push("    uint64_t    hash;");
lineas.push("};");
lineas.push("");
lineas.push("inline constexpr VectorHash HASHES[] = {");
for (const texto of ["", "hello", "petbits", "criatura", "0", "Nébula", "a".repeat(64)]) {
  lineas.push(`    {${JSON.stringify(texto)}, ${hex(hashString(texto))}},`);
}
lineas.push("};");
lineas.push("");
lineas.push("} // namespace petbits::vectores");
lineas.push("");

writeFileSync(SALIDA, lineas.join("\n"), "utf8");

console.log(`Vectores escritos en ${SALIDA}`);
console.log(`  ${seeds.length} genomas`);
console.log(`  ${CRIANZAS.length * 8} crianzas`);
console.log("\nAhora compilá y corré los tests de C++ — ver gdext/tests/README.md");
