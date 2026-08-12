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
import { type Genes, decodeGenome, formatSeed, hashString, parseSeed } from "../src/core/genome.ts";
import { deriveSeed, mulberry32, splitmix64 } from "../src/core/rng.ts";
import { TICK_MS, createCreature, simulate } from "../src/core/simulation.ts";
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

/**
 * Un double, escrito de forma que C++ lo lea EXACTAMENTE igual.
 *
 * `String(x)` en JavaScript da la cadena más corta que vuelve al mismo double
 * al releerla. La conversión de decimal a binario del compilador está obligada
 * a redondear correctamente, así que el literal que sale de acá reconstruye el
 * mismo patrón de bits. No es aproximado: es el mismo número.
 *
 * Importa porque los stats se acumulan a lo largo de miles de ticks. Una
 * diferencia en el último bit del primer tick se amplifica, y a los tres días
 * simulados la criatura de la web y la del nativo dejan de ser la misma.
 */
function dbl(valor: number): string {
  if (!Number.isFinite(valor)) throw new Error(`Valor no finito: ${valor}`);
  const texto = String(valor);
  return /[.eE]/.test(texto) ? texto : `${texto}.0`;
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
// Los acentos y el emoji están a propósito. En ASCII un byte es una unidad
// UTF-16 y hashear bytes o unidades da lo mismo, así que solo con "hello" el
// test pasa aunque el port esté mal. La é ocupa dos bytes y una unidad; el
// emoji, cuatro bytes y dos unidades (par suplente).
for (const texto of [
  "",
  "hello",
  "petbits",
  "criatura",
  "0",
  "Nébula",
  "Raíz",
  "Plácido",
  "ñandú",
  "🧬",
  "petbits 🧬 Raíz",
  "a".repeat(64),
]) {
  lineas.push(`    {${JSON.stringify(texto)}, ${hex(hashString(texto))}},`);
}
lineas.push("};");
lineas.push("");

// --- parseSeed ---
//
// Es la función que corre cuando alguien escribe un seed en la pantalla, así
// que una diferencia acá se ve enseguida: el mismo texto da otra criatura.
// Las entradas están elegidas para pegarle a cada rama y a los bordes entre
// ramas, que es donde el orden de los `if` importa.
const ENTRADAS: readonly string[] = [
  "A3F0-91C4-77BE-2D08", // el formato que muestra la interfaz
  "a3f091c477be2d08", // el mismo, en minúscula y sin guiones
  "0xA3F091C477BE2D08", // con prefijo explícito
  "  A3F0 91C4 77BE 2D08  ", // con espacios de sobra alrededor y adentro
  "1234", // hex y decimal a la vez: manda decimal
  "999", // decimal corto
  "0", // el borde de abajo
  "18446744073709551615", // 2^64-1 exacto
  "18446744073709551616", // uno más: tiene que dar la vuelta, no reventar
  "999999999999999999999999999999", // muy pasado de rosca
  "FFFFFFFFFFFFFFFFFF", // 18 hex: pasa los 16 del patrón, así que se hashea
  "deadbeef", // hex con letras
  "julian", // texto: se hashea
  "Nébula", // texto con acento — dos bytes UTF-8, una unidad UTF-16
  "  julian  ", // el mismo texto con espacios: tiene que dar igual que "julian"
  "0x", // parece prefijo y no lo es
  "0xZZ", // prefijo con basura atrás
  "petbits 2026", // frase con espacio interior
  "-", // queda vacío al limpiar, pero el original no lo estaba: se hashea
  "---", // lo mismo, más largo
  "1-2-3-4", // los guiones se van y queda un decimal
];

lineas.push("struct VectorParseo {");
lineas.push("    const char* entrada;");
lineas.push("    uint64_t    seed;");
lineas.push("};");
lineas.push("");
lineas.push("inline constexpr VectorParseo PARSEOS[] = {");
for (const entrada of ENTRADAS) {
  lineas.push(`    {${JSON.stringify(entrada)}, ${hex(parseSeed(entrada))}},`);
}
lineas.push("};");
lineas.push("");

// --- rng ---
//
// Va aparte de la simulación porque si mulberry32 se desvía, TODO lo que
// depende del azar se desvía con él y los errores de más arriba se vuelven
// ilegibles. Teniendo esto verificado, una falla en la simulación se sabe que
// es de la simulación.
lineas.push("struct VectorRng {");
lineas.push("    uint32_t semilla;");
lineas.push("    double   valores[8];   ///< las primeras 8 salidas de next()");
lineas.push("};");
lineas.push("");
lineas.push("inline constexpr VectorRng RNGS[] = {");
for (const semilla of [0, 1, 42, 0x6d2b79f5, 0xffffffff, 0x9e3779b1, 123456789]) {
  const rng = mulberry32(semilla);
  const valores = Array.from({ length: 8 }, () => dbl(rng.next()));
  lineas.push(`    {${semilla >>> 0}u, {${valores.join(", ")}}},`);
}
lineas.push("};");
lineas.push("");

lineas.push("struct VectorDerive {");
lineas.push("    uint64_t    seed;");
lineas.push("    const char* etiqueta;");
lineas.push("    uint32_t    derivada;");
lineas.push("};");
lineas.push("");
lineas.push("inline constexpr VectorDerive DERIVES[] = {");
for (const [seed, etiqueta] of [
  [0n, "eventos"],
  [1n, "eventos"],
  [0xa3f091c477be2d08n, "eventos"],
  [0xffffffffffffffffn, "eventos"],
  [0xa3f091c477be2d08n, "manchas"],
  [0xa3f091c477be2d08n, ""],
  // Con acento: hoy ninguna etiqueta lo lleva, pero el día que una lo lleve
  // esto avisa en vez de dar otro número en silencio.
  [0xa3f091c477be2d08n, "señuelo"],
] as [bigint, string][]) {
  lineas.push(`    {${hex(seed)}, ${JSON.stringify(etiqueta)}, ${deriveSeed(seed, etiqueta)}u},`);
}
lineas.push("};");
lineas.push("");

// --- simulación ---
//
// Es el módulo con más superficie para desviarse: mezcla punto flotante
// acumulado a lo largo de miles de ticks, aritmética de fechas con divisiones
// que redondean distinto en cada lenguaje, y azar sembrado por índice de tick.
//
// Los escenarios están elegidos para cruzar los bordes que importan: el paso a
// juvenil (1440 ticks activos), el letargo (2880 ticks sin cuidado), la entrada
// y salida de la noche, y el cambio de día local con desfasajes horarios
// distintos — incluido uno negativo, que es donde la división con piso se
// separa de la división truncada.
const BASE_MS = 1786406400000; // 2026-08-11T00:00:00Z, elegido y fijo

interface Escenario {
  nombre: string;
  seed: bigint;
  inicioMs: number;
  tz: number;
  ticks: number;
}

const ESCENARIOS: readonly Escenario[] = [
  {
    nombre: "corto de dia",
    seed: 0xa3f091c477be2d08n,
    inicioMs: BASE_MS + 12 * 3600_000,
    tz: -180,
    ticks: 30,
  },
  {
    nombre: "cruza la noche",
    seed: 0xa3f091c477be2d08n,
    inicioMs: BASE_MS + 22 * 3600_000,
    tz: -180,
    ticks: 180,
  },
  { nombre: "un dia entero", seed: 0xa3f091c477be2d08n, inicioMs: BASE_MS, tz: -180, ticks: 1440 },
  {
    nombre: "justo al evolucionar",
    seed: 0xa3f091c477be2d08n,
    inicioMs: BASE_MS,
    tz: -180,
    ticks: 1441,
  },
  {
    nombre: "hasta el letargo",
    seed: 0xa3f091c477be2d08n,
    inicioMs: BASE_MS,
    tz: -180,
    ticks: 2880,
  },
  {
    nombre: "pasado el letargo",
    seed: 0xa3f091c477be2d08n,
    inicioMs: BASE_MS,
    tz: -180,
    ticks: 3000,
  },
  { nombre: "siete dias", seed: 0xa3f091c477be2d08n, inicioMs: BASE_MS, tz: -180, ticks: 10080 },
  {
    nombre: "metabolismo lento",
    seed: 0x0000000000000000n,
    inicioMs: BASE_MS,
    tz: -180,
    ticks: 2000,
  },
  {
    nombre: "metabolismo frenetico",
    seed: 0xffffffffffffffffn,
    inicioMs: BASE_MS,
    tz: -180,
    ticks: 2000,
  },
  { nombre: "tz cero", seed: 0x5555555555555555n, inicioMs: BASE_MS, tz: 0, ticks: 1500 },
  { nombre: "tz positivo", seed: 0x5555555555555555n, inicioMs: BASE_MS, tz: 540, ticks: 1500 },
  {
    nombre: "tz negativo grande",
    seed: 0x5555555555555555n,
    inicioMs: BASE_MS,
    tz: -720,
    ticks: 1500,
  },
  {
    nombre: "arranca de madrugada",
    seed: 0xfedcba9876543210n,
    inicioMs: BASE_MS + 3 * 3600_000,
    tz: -180,
    ticks: 600,
  },
  {
    nombre: "sin ticks completos",
    seed: 0xa3f091c477be2d08n,
    inicioMs: BASE_MS,
    tz: -180,
    ticks: 0,
  },
];

const ETAPAS = ["bebe", "juvenil", "adulto"];

lineas.push("struct VectorSim {");
lineas.push("    uint64_t seed;");
lineas.push("    int64_t  inicioMs;");
lineas.push("    int32_t  tz;");
lineas.push("    int64_t  ticksPedidos;");
lineas.push("    // --- lo que tiene que dar ---");
lineas.push("    int64_t  ticks;");
lineas.push("    int64_t  lastTickMs;");
lineas.push("    int64_t  ticksVividos;");
lineas.push("    int64_t  ticksActivos;");
lineas.push("    int64_t  ticksSinCuidado;");
lineas.push("    int64_t  diaIndice;");
lineas.push("    uint8_t  letargico;");
lineas.push("    uint8_t  durmiendo;");
lineas.push("    uint8_t  etapa;      ///< 0 bebe, 1 juvenil, 2 adulto");
lineas.push("    uint8_t  forma;      ///< índice en FORMAS");
lineas.push("    double   energia, animo, salud, vinculo;");
lineas.push("    double   sumaAnimo, sumaSalud;");
lineas.push("    int64_t  ticksMedidos;");
lineas.push("    int64_t  eventos;    ///< cuántos quedaron después del recorte");
lineas.push("    int64_t  omitidos;");
lineas.push("    int64_t  hallazgos, evoluciones, durmio, desperto, letargo;");
lineas.push("    int64_t  hambre, animoEv, saludEv;");
lineas.push("    const char* nombre;");
lineas.push("};");
lineas.push("");
lineas.push("inline constexpr VectorSim SIMULACIONES[] = {");

for (const e of ESCENARIOS) {
  const inicial = createCreature(e.seed, e.inicioMs, e.tz);
  const r = simulate(inicial, e.inicioMs + e.ticks * TICK_MS);
  const s = r.state;
  const c = s.crianza;
  const n = (k: string) => r.summary[k] ?? 0;

  const campos = [
    hex(e.seed),
    `${e.inicioMs}LL`,
    e.tz,
    `${e.ticks}LL`,
    `${r.ticks}LL`,
    `${s.lastTickMs}LL`,
    `${s.ticksVividos}LL`,
    `${s.ticksActivos}LL`,
    `${s.ticksSinCuidado}LL`,
    `${s.diaIndice}LL`,
    s.letargico ? 1 : 0,
    s.durmiendo ? 1 : 0,
    ETAPAS.indexOf(s.etapa),
    indiceForma(s.forma),
    dbl(s.stats.energia),
    dbl(s.stats.animo),
    dbl(s.stats.salud),
    dbl(s.stats.vinculo),
    dbl(c.sumaAnimo),
    dbl(c.sumaSalud),
    `${c.ticksMedidos}LL`,
    `${r.events.length}LL`,
    `${r.omitted}LL`,
    `${n("hallazgo")}LL`,
    `${n("evolucion")}LL`,
    `${n("durmio")}LL`,
    `${n("desperto")}LL`,
    `${n("letargo")}LL`,
    `${n("hambre")}LL`,
    `${n("animo")}LL`,
    `${n("salud")}LL`,
    `"${e.nombre}"`,
  ];
  lineas.push(`    {${campos.join(", ")}},`);
}

lineas.push("};");
lineas.push("");
lineas.push("} // namespace petbits::vectores");
lineas.push("");

writeFileSync(SALIDA, lineas.join("\n"), "utf8");

console.log(`Vectores escritos en ${SALIDA}`);
console.log(`  ${seeds.length} genomas`);
console.log(`  ${CRIANZAS.length * 8} crianzas`);
console.log(`  ${ENTRADAS.length} entradas de parseSeed`);
console.log(`  ${ESCENARIOS.length} escenarios de simulación`);
console.log("\nAhora compilá y corré los tests de C++ — ver gdext/tests/README.md");
