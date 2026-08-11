/**
 * Tests de paridad TypeScript ↔ C++.
 *
 * Compilar y correr (ver tests/README.md para el detalle por compilador):
 *
 *     g++ -std=c++17 -O2 -I../src test_parity.cpp ../src/genome.cpp \
 *         ../src/traits.cpp ../src/evolution.cpp -o run_tests && ./run_tests
 *
 * ---
 *
 * DOS DECISIONES QUE VALE LA PENA EXPLICAR.
 *
 * 1. Los valores esperados no están acá. Están en vectores_generados.h, que
 *    produce `npm run parity` ejecutando el TypeScript de verdad. Escribirlos a
 *    mano sería pedirle al mismo criterio que hizo el port que se corrija solo:
 *    si leí mal el TS, lo leo mal las dos veces y el test pasa igual.
 *
 * 2. No hay framework. Ni Catch2 ni GoogleTest ni el build de SCons. Estos tres
 *    módulos son C++ puro —no incluyen un solo header de Godot— así que
 *    verificarlos no tendría que exigir tener instalada la cadena entera del
 *    motor. Con un compilador y este archivo alcanza, y eso hace que la
 *    paridad se pueda comprobar el primer día, antes de bajar nada más.
 *
 * El arnés son treinta líneas al final del archivo. Cuando haga falta algo que
 * no da, ahí sí conviene traer un framework.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/evolution.h"
#include "../src/genome.h"
#include "../src/traits.h"
#include "vectores_generados.h"

using namespace petbits;

// ---------------------------------------------------------------------------
// Arnés
// ---------------------------------------------------------------------------

static int comprobaciones = 0;
static int fallos = 0;
static const char* bloqueActual = "";

/** Solo imprime cuando algo falla: mil líneas de "ok" no las lee nadie. */
static void revisar(bool condicion, const char* contexto, const char* detalle) {
    ++comprobaciones;
    if (condicion) return;
    ++fallos;
    // Se cortan los fallos en 40. Cuando un port se rompe suele romperse para
    // todos los vectores a la vez, y treinta mil líneas iguales tapan el resto.
    if (fallos <= 40) {
        std::printf("  FALLA [%s] %s\n         %s\n", bloqueActual, contexto, detalle);
    } else if (fallos == 41) {
        std::printf("  ... (se omiten los siguientes)\n");
    }
}

static void revisarEnteros(uint64_t obtenido, uint64_t esperado, const char* contexto,
                           const char* campo) {
    char detalle[256];
    std::snprintf(detalle, sizeof(detalle), "%s: C++ dio %llu, el TS da %llu", campo,
                  static_cast<unsigned long long>(obtenido),
                  static_cast<unsigned long long>(esperado));
    revisar(obtenido == esperado, contexto, detalle);
}

static void bloque(const char* nombre) {
    bloqueActual = nombre;
    std::printf("%s\n", nombre);
}

// ---------------------------------------------------------------------------
// genome
// ---------------------------------------------------------------------------

static void probarGenomas() {
    bloque("decodeGenome / formatSeed");

    for (const auto& v : vectores::GENOMAS) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX",
                      static_cast<unsigned long long>(v.seed));

        const Genes g = decodeGenome(v.seed);
        revisarEnteros(g.lineage, v.lineage, ctx, "lineage");
        revisarEnteros(g.bodyShape, v.bodyShape, ctx, "bodyShape");
        revisarEnteros(g.eyes, v.eyes, ctx, "eyes");
        revisarEnteros(g.mouth, v.mouth, ctx, "mouth");
        revisarEnteros(g.appendages, v.appendages, ctx, "appendages");
        revisarEnteros(g.pattern, v.pattern, ctx, "pattern");
        revisarEnteros(g.hue, v.hue, ctx, "hue");
        revisarEnteros(g.paletteMode, v.paletteMode, ctx, "paletteMode");
        revisarEnteros(g.temperament, v.temperament, ctx, "temperament");
        revisarEnteros(g.metabolism, v.metabolism, ctx, "metabolism");
        revisarEnteros(g.affinity, v.affinity, ctx, "affinity");
        revisarEnteros(g.proportion, v.proportion, ctx, "proportion");
        revisarEnteros(g.statBias.vigor, v.vigor, ctx, "statBias.vigor");
        revisarEnteros(g.statBias.animo, v.animo, ctx, "statBias.animo");
        revisarEnteros(g.statBias.ingenio, v.ingenio, ctx, "statBias.ingenio");
        revisarEnteros(g.statBias.vinculo, v.vinculo, ctx, "statBias.vinculo");
        revisarEnteros(g.mutation, v.mutation, ctx, "mutation");

        const std::string formateado = formatSeed(v.seed);
        char detalle[160];
        std::snprintf(detalle, sizeof(detalle), "formatSeed: C++ dio \"%s\", el TS da \"%s\"",
                      formateado.c_str(), v.seedFormateado);
        revisar(formateado == v.seedFormateado, ctx, detalle);
    }
}

static void probarHashes() {
    bloque("hashString (FNV-1a 64)");

    for (const auto& v : vectores::HASHES) {
        char ctx[128];
        std::snprintf(ctx, sizeof(ctx), "texto \"%s\"", v.texto);
        revisarEnteros(hashString(v.texto), v.hash, ctx, "hash");
    }
}

// ---------------------------------------------------------------------------
// traits
// ---------------------------------------------------------------------------

/** Las rarezas detectadas, como bitmask sobre TRAIT_CATALOG. */
static uint8_t mascaraRarezas(Seed seed) {
    const std::vector<Trait> detectadas = detectTraits(seed);
    uint8_t mascara = 0;
    for (const auto& t : detectadas) {
        for (size_t i = 0; i < TRAIT_CATALOG.size(); ++i) {
            if (TRAIT_CATALOG[i].id == t.id) mascara |= static_cast<uint8_t>(1u << i);
        }
    }
    return mascara;
}

static void describirRarezas(uint8_t mascara, char* salida, size_t capacidad) {
    salida[0] = '\0';
    bool primera = true;
    for (size_t i = 0; i < TRAIT_CATALOG.size(); ++i) {
        if (!(mascara & (1u << i))) continue;
        if (!primera) std::strncat(salida, ", ", capacidad - std::strlen(salida) - 1);
        const std::string id(TRAIT_CATALOG[i].id);
        std::strncat(salida, id.c_str(), capacidad - std::strlen(salida) - 1);
        primera = false;
    }
    if (primera) std::strncat(salida, "(ninguna)", capacidad - 1);
}

static void probarRarezas() {
    bloque("detectTraits / rarityTier");

    for (const auto& v : vectores::GENOMAS) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX",
                      static_cast<unsigned long long>(v.seed));

        const uint8_t obtenida = mascaraRarezas(v.seed);
        if (obtenida != v.rarezas) {
            char mias[256];
            char suyas[256];
            describirRarezas(obtenida, mias, sizeof(mias));
            describirRarezas(v.rarezas, suyas, sizeof(suyas));
            char detalle[600];
            std::snprintf(detalle, sizeof(detalle), "rarezas: C++ dio [%s], el TS da [%s]", mias,
                          suyas);
            revisar(false, ctx, detalle);
        } else {
            revisar(true, ctx, "");
        }

        revisarEnteros(static_cast<uint64_t>(rarityTier(detectTraits(v.seed))), v.tier, ctx,
                       "rarityTier");
    }
}

// ---------------------------------------------------------------------------
// evolution
// ---------------------------------------------------------------------------

static void probarEvolucion() {
    bloque("resolverJuvenil / resolverAdulto");

    static const char* FORMAS[] = {"indefinida", "petreo",  "vaporoso", "coloso",
                                   "guardian",   "errante", "oraculo"};

    for (const auto& v : vectores::EVOLUCIONES) {
        Crianza c{};
        c.dieta_proteina = v.proteina;
        c.dieta_dulce = v.dulce;
        c.dieta_mineral = v.mineral;
        c.dieta_raro = v.raro;
        c.juego = v.juego;
        c.calma = v.calma;
        c.sumaAnimo = v.sumaAnimo;
        c.sumaSalud = v.sumaSalud;
        c.ticksMedidos = v.ticksMedidos;

        Genes g{};
        g.affinity = v.affinity;

        const Form juvenil = resolverJuvenil(c, g);
        const Form adulto = resolverAdulto(c, g, juvenil);

        char detalle[256];
        std::snprintf(detalle, sizeof(detalle), "juvenil: C++ dio %s, el TS da %s",
                      FORMAS[static_cast<int>(juvenil)], FORMAS[v.juvenil]);
        revisar(static_cast<uint8_t>(juvenil) == v.juvenil, v.nombre, detalle);

        std::snprintf(detalle, sizeof(detalle), "adulto: C++ dio %s, el TS da %s",
                      FORMAS[static_cast<int>(adulto)], FORMAS[v.adulto]);
        revisar(static_cast<uint8_t>(adulto) == v.adulto, v.nombre, detalle);
    }
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("\nPetBits — paridad TypeScript <-> C++\n");
    std::printf("%zu genomas, %zu crianzas, %zu hashes\n\n",
                sizeof(vectores::GENOMAS) / sizeof(vectores::GENOMAS[0]),
                sizeof(vectores::EVOLUCIONES) / sizeof(vectores::EVOLUCIONES[0]),
                sizeof(vectores::HASHES) / sizeof(vectores::HASHES[0]));

    probarGenomas();
    probarHashes();
    probarRarezas();
    probarEvolucion();

    std::printf("\n%d comprobaciones, %d fallas\n", comprobaciones, fallos);
    if (fallos == 0) {
        std::printf("Paridad OK: el C++ da exactamente lo mismo que el TypeScript.\n\n");
        return 0;
    }
    std::printf("PARIDAD ROTA. Un mismo seed da criaturas distintas en la web y en el nativo.\n\n");
    return 1;
}
