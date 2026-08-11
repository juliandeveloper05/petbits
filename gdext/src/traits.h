#pragma once
/**
 * traits.h — Port de src/core/traits.ts
 *
 * Rarezas emergentes: propiedades matemáticas del seed.
 *
 * INVARIANTE: detectTraits(seed) en C++ debe retornar exactamente
 * las mismas rarezas que en TypeScript para cualquier seed de 64 bits.
 * Esto incluye el test de primalidad Miller-Rabin con las mismas 12 bases.
 */

#include "genome.h"
#include <array>
#include <string_view>
#include <vector>

namespace petbits {

// ---------------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------------

enum class TraitTier : uint8_t { Raro, Epico, Legendario };
enum class RarityTier : uint8_t { Comun, Raro, Epico, Legendario };

struct Trait {
    std::string_view id;
    std::string_view name;
    std::string_view rule;
    TraitTier        tier;
    double           approxRate;
};

// ---------------------------------------------------------------------------
// Primitivas de bits (mirrors exactos de traits.ts)
// ---------------------------------------------------------------------------

/** Peso de Hamming — cantidad de bits en 1. Replica popcount() del TS. */
int popcount(uint64_t value);

/** Racha más larga de bits en 1 consecutivos. Replica longestRunOfOnes(). */
int longestRunOfOnes(uint64_t value);

/** Invierte el orden de 16 bits. Replica reverseBits16(). */
uint32_t reverseBits16(uint32_t value);

/** ¿Los 16 nibbles del genoma son los 16 valores posibles, sin repetir? */
bool nibblesAllDistinct(uint64_t value);

/**
 * Miller-Rabin determinista para 64 bits.
 * Mismas 12 bases que traits.ts: 2,3,5,7,11,13,17,19,23,29,31,37.
 * Decisión correcta para todo n < 3.3·10^24.
 */
bool isPrime(uint64_t n);

// ---------------------------------------------------------------------------
// Catálogo y detección
// ---------------------------------------------------------------------------

/** Catálogo completo de rarezas, mismo orden que TRAIT_RULES en el TS. */
extern const std::array<Trait, 8> TRAIT_CATALOG;

/** Detecta todas las rarezas que cumple un genoma. */
std::vector<Trait> detectTraits(Seed seed);

/** Puntaje agregado (raro=1, epico=3, legendario=10). */
int rarityScore(const std::vector<Trait>& traits);

/** Categoría global derivada del puntaje. */
RarityTier rarityTier(const std::vector<Trait>& traits);

std::string_view rarityTierName(RarityTier t);

} // namespace petbits
