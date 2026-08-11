/**
 * PetBits — Test de paridad TS ↔ C++
 *
 * Estos tests verifican que los algoritmos C++ producen exactamente los
 * mismos resultados que su equivalente TypeScript para los mismos inputs.
 *
 * Vectores de test generados con: npx vite-node tools/verify_parity.ts
 */

// Catch2 (header-only, single file)
#define CATCH_CONFIG_MAIN
#include "catch2/catch_amalgamated.hpp"

#include "../src/genome.h"
#include "../src/traits.h"
#include "../src/evolution.h"

using namespace petbits;

// ---------------------------------------------------------------------------
// genome.cpp
// ---------------------------------------------------------------------------

TEST_CASE("decodeGenome — vector de paridad con TypeScript", "[genome][parity]") {
    // Vectores generados en TS con decodeGenome(seed) para estos seeds específicos
    SECTION("seed = 0") {
        const Genes g = decodeGenome(0ULL);
        REQUIRE(g.lineage     == 0);
        REQUIRE(g.bodyShape   == 0);
        REQUIRE(g.hue         == 0);
        REQUIRE(g.paletteMode == 0);
        REQUIRE(g.temperament == 0);
        REQUIRE(g.metabolism  == 0);
        REQUIRE(g.affinity    == 0);
        REQUIRE(g.mutation    == 0);
    }

    SECTION("seed = 0xFFFFFFFFFFFFFFFF (todos los bits en 1)") {
        const Genes g = decodeGenome(0xFFFFFFFFFFFFFFFFULL);
        REQUIRE(g.lineage     == 15);
        REQUIRE(g.bodyShape   == 15);
        REQUIRE(g.hue         == 255);
        REQUIRE(g.paletteMode == 7);
        REQUIRE(g.temperament == 7);
        REQUIRE(g.metabolism  == 7);
        REQUIRE(g.affinity    == 7);
        REQUIRE(g.mutation    == 255);
        REQUIRE(g.statBias.vigor   == 3);
        REQUIRE(g.statBias.animo   == 3);
        REQUIRE(g.statBias.ingenio == 3);
        REQUIRE(g.statBias.vinculo == 3);
    }

    SECTION("seed = 0xA3F091C477BE2D08") {
        // Generado en TS: decodeGenome(0xA3F091C477BE2D08n)
        const Genes g = decodeGenome(0xA3F091C477BE2D08ULL);
        REQUIRE(g.lineage     == 8);   // bits 0-3 de 0x...08
        REQUIRE(g.bodyShape   == 0);   // bits 4-7 de 0x...08
        REQUIRE(g.hue         == 0xBE); // bits 24-31
        // El resto se verifica con verify_parity.ts antes del merge
    }
}

TEST_CASE("formatSeed — formato hex agrupado", "[genome]") {
    REQUIRE(formatSeed(0xA3F091C477BE2D08ULL) == "A3F0-91C4-77BE-2D08");
    REQUIRE(formatSeed(0x0000000000000000ULL) == "0000-0000-0000-0000");
    REQUIRE(formatSeed(0xFFFFFFFFFFFFFFFFULL) == "FFFF-FFFF-FFFF-FFFF");
}

TEST_CASE("hashString — FNV-1a 64 bits, paridad con TS", "[genome][parity]") {
    // Estos valores se verifican contra hashString() del TS
    REQUIRE(hashString("hello") == 0xa430d84680aabd0bULL);
    REQUIRE(hashString("petbits") != 0ULL); // sanity check
    REQUIRE(hashString("") == 0xcbf29ce484222325ULL); // valor inicial FNV-1a
}

// ---------------------------------------------------------------------------
// traits.cpp
// ---------------------------------------------------------------------------

TEST_CASE("popcount — peso de Hamming", "[traits]") {
    REQUIRE(popcount(0ULL) == 0);
    REQUIRE(popcount(0xFFFFFFFFFFFFFFFFULL) == 64);
    REQUIRE(popcount(1ULL) == 1);
    REQUIRE(popcount(0xAAAAAAAAAAAAAAAAULL) == 32);
}

TEST_CASE("longestRunOfOnes", "[traits]") {
    REQUIRE(longestRunOfOnes(0ULL) == 0);
    REQUIRE(longestRunOfOnes(0b111ULL) == 3);
    REQUIRE(longestRunOfOnes(0xFFFFFFFFFFFFFFFFULL) == 64);
    // 9 consecutivos
    REQUIRE(longestRunOfOnes(0x1FFULL) == 9);
}

TEST_CASE("reverseBits16", "[traits]") {
    REQUIRE(reverseBits16(0b1000000000000000u) == 0b0000000000000001u);
    REQUIRE(reverseBits16(0b0000000000000001u) == 0b1000000000000000u);
    REQUIRE(reverseBits16(0xFFFFu) == 0xFFFFu);
    REQUIRE(reverseBits16(0u) == 0u);
}

TEST_CASE("nibblesAllDistinct", "[traits]") {
    // Los 16 nibbles 0-F sin repetir (pangrama)
    REQUIRE(nibblesAllDistinct(0xFEDCBA9876543210ULL) == true);
    // Con repetición
    REQUIRE(nibblesAllDistinct(0xFFFFFFFFFFFFFFFFULL) == false);
    REQUIRE(nibblesAllDistinct(0ULL) == false);
}

TEST_CASE("isPrime — Miller-Rabin 64 bits", "[traits]") {
    REQUIRE(isPrime(0) == false);
    REQUIRE(isPrime(1) == false);
    REQUIRE(isPrime(2) == true);
    REQUIRE(isPrime(3) == true);
    REQUIRE(isPrime(4) == false);
    REQUIRE(isPrime(37) == true);
    REQUIRE(isPrime(39) == false);
    // Primo grande de 64 bits
    REQUIRE(isPrime(18446744073709551557ULL) == true); // mayor primo < 2^64
    // Compuesto grande
    REQUIRE(isPrime(18446744073709551615ULL) == false); // 2^64 - 1 = 3 * 5 * ...
}

TEST_CASE("detectTraits — paridad con TypeScript", "[traits][parity]") {
    // seed con 32 bits en 1 → "equilibrado"
    // 0x5555555555555555 tiene exactamente 32 bits en 1
    const auto traits = detectTraits(0x5555555555555555ULL);
    bool hasEquilibrado = false;
    for (const auto& t : traits) {
        if (t.id == "equilibrado") hasEquilibrado = true;
    }
    REQUIRE(hasEquilibrado);
}

// ---------------------------------------------------------------------------
// evolution.cpp
// ---------------------------------------------------------------------------

TEST_CASE("resolverAdulto — paridad con TypeScript", "[evolution][parity]") {
    // Con crianza de puro proteina + juego → Coloso (Petreo + Activo)
    Crianza c{};
    c.dieta_proteina = 50;
    c.juego = 20;

    Genes g{};
    g.affinity = 0; // Brasa = bias +2 (más pétreo)

    const Form juvenil = resolverJuvenil(c, g);
    REQUIRE(juvenil == Form::Petreo);

    const Form adulto = resolverAdulto(c, g, juvenil);
    REQUIRE(adulto == Form::Coloso);
}

TEST_CASE("resolverAdulto — Oráculo (vaporoso + inactivo)", "[evolution]") {
    Crianza c{};
    c.dieta_dulce = 50;
    c.calma = 20;

    Genes g{};
    g.affinity = 6; // Eco = bias -3 (más vaporoso)

    const Form juvenil = resolverJuvenil(c, g);
    REQUIRE(juvenil == Form::Vaporoso);

    const Form adulto = resolverAdulto(c, g, juvenil);
    REQUIRE(adulto == Form::Oraculo);
}
