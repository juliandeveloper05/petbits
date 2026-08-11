/**
 * traits.cpp — Port de src/core/traits.ts
 *
 * Las mismas 12 bases de Miller-Rabin, los mismos predicados de bits.
 * Ver traits.h para la documentación del invariante de paridad.
 */

#include "traits.h"
#include <cassert>
#include <stdexcept>

namespace petbits {

// ---------------------------------------------------------------------------
// Primitivas de bits
// ---------------------------------------------------------------------------

int popcount(uint64_t value) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(value);
#else
    // Fallback portable (mismo algoritmo que el TS)
    int count = 0;
    while (value > 0) {
        value &= value - 1;
        ++count;
    }
    return count;
#endif
}

int longestRunOfOnes(uint64_t value) {
    int longest = 0;
    int current = 0;
    for (int i = 0; i < 64; ++i) {
        if ((value >> i) & 1ULL) {
            if (++current > longest) longest = current;
        } else {
            current = 0;
        }
    }
    return longest;
}

uint32_t reverseBits16(uint32_t value) {
    uint32_t reversed = 0;
    for (int i = 0; i < 16; ++i) {
        reversed = (reversed << 1) | ((value >> i) & 1u);
    }
    return reversed;
}

bool nibblesAllDistinct(uint64_t value) {
    uint32_t seen = 0;
    for (int i = 0; i < 16; ++i) {
        const uint32_t nibble = static_cast<uint32_t>((value >> (i * 4)) & 0xFULL);
        const uint32_t bit    = 1u << nibble;
        if (seen & bit) return false;
        seen |= bit;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Miller-Rabin determinista
// ---------------------------------------------------------------------------

static uint64_t modmul(uint64_t a, uint64_t b, uint64_t mod) {
    // Usa __uint128_t cuando está disponible para evitar overflow
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<uint64_t>(
        (static_cast<unsigned __int128>(a) * b) % mod
    );
#else
    // Fallback: multiplicación por doblado de bits
    uint64_t result = 0;
    a %= mod;
    while (b > 0) {
        if (b & 1) result = (result + a) % mod;  // podría desbordar para mod grande
        a = (a * 2) % mod;
        b >>= 1;
    }
    return result;
#endif
}

static uint64_t modpow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1) result = modmul(result, base, mod);
        base = modmul(base, base, mod);
        exp >>= 1;
    }
    return result;
}

// Las mismas 12 bases que traits.ts — suficientes para certeza en 64 bits.
static constexpr std::array<uint64_t, 12> MILLER_RABIN_BASES = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37
};

bool isPrime(uint64_t n) {
    if (n < 2) return false;
    for (const uint64_t base : MILLER_RABIN_BASES) {
        if (n == base) return true;
        if (n % base == 0) return false;
    }

    uint64_t d = n - 1;
    int r = 0;
    while ((d & 1) == 0) { d >>= 1; ++r; }

    for (const uint64_t base : MILLER_RABIN_BASES) {
        uint64_t x = modpow(base, d, n);
        if (x == 1 || x == n - 1) continue;

        bool composite = true;
        for (int i = 1; i < r; ++i) {
            x = modmul(x, x, n);
            if (x == n - 1) { composite = false; break; }
        }
        if (composite) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Catálogo de rarezas (mismo orden y tasas que TRAIT_RULES en traits.ts)
// ---------------------------------------------------------------------------

const std::array<Trait, 8> TRAIT_CATALOG = {{
    { "equilibrado", "Equilibrado", "Exactamente 32 de sus 64 bits están en 1",
      TraitTier::Raro,       0.0998 },
    { "vacio",       "Vacío",       "24 bits o menos en 1 — genoma escaso, pigmentación pálida",
      TraitTier::Raro,       0.03   },
    { "saturado",    "Saturado",    "40 bits o más en 1 — genoma denso, pigmentación intensa",
      TraitTier::Raro,       0.0296 },
    { "racha",       "Racha",       "Tiene 9 o más bits en 1 consecutivos",
      TraitTier::Raro,       0.0553 },
    { "primordial",  "Primordial",  "El genoma es un número primo",
      TraitTier::Epico,      0.0215 },
    { "uroboros",    "Uróboros",    "Su primer byte es idéntico al último",
      TraitTier::Epico,      0.0035 },
    { "espejo",      "Espejo",      "Sus 16 bits más altos son el reflejo exacto de los 16 más bajos",
      TraitTier::Legendario, 0.0000153  },
    { "pangrama",    "Pangrama",    "Sus 16 nibbles son los 16 valores posibles, sin repetir ninguno",
      TraitTier::Legendario, 0.00000113 },
}};

// ---------------------------------------------------------------------------
// detectTraits
// ---------------------------------------------------------------------------

std::vector<Trait> detectTraits(Seed seed) {
    std::vector<Trait> found;
    found.reserve(4); // caso típico

    if (popcount(seed) == 32)              found.push_back(TRAIT_CATALOG[0]); // equilibrado
    if (popcount(seed) <= 24)              found.push_back(TRAIT_CATALOG[1]); // vacio
    if (popcount(seed) >= 40)             found.push_back(TRAIT_CATALOG[2]); // saturado
    if (longestRunOfOnes(seed) >= 9)       found.push_back(TRAIT_CATALOG[3]); // racha
    if (isPrime(seed))                     found.push_back(TRAIT_CATALOG[4]); // primordial
    if (((seed >> 56) & 0xFF) == (seed & 0xFF))
                                           found.push_back(TRAIT_CATALOG[5]); // uroboros
    if (reverseBits16(static_cast<uint32_t>(seed & 0xFFFF)) ==
        static_cast<uint32_t>((seed >> 48) & 0xFFFF))
                                           found.push_back(TRAIT_CATALOG[6]); // espejo
    if (nibblesAllDistinct(seed))          found.push_back(TRAIT_CATALOG[7]); // pangrama

    return found;
}

// ---------------------------------------------------------------------------
// rarityScore / rarityTier
// ---------------------------------------------------------------------------

int rarityScore(const std::vector<Trait>& traits) {
    int score = 0;
    for (const auto& t : traits) {
        switch (t.tier) {
            case TraitTier::Raro:       score += 1;  break;
            case TraitTier::Epico:      score += 3;  break;
            case TraitTier::Legendario: score += 10; break;
        }
    }
    return score;
}

RarityTier rarityTier(const std::vector<Trait>& traits) {
    const int score = rarityScore(traits);
    if (score >= 10) return RarityTier::Legendario;
    if (score >= 3)  return RarityTier::Epico;
    if (score >= 1)  return RarityTier::Raro;
    return RarityTier::Comun;
}

std::string_view rarityTierName(RarityTier t) {
    switch (t) {
        case RarityTier::Comun:      return "Común";
        case RarityTier::Raro:       return "Raro";
        case RarityTier::Epico:      return "Épico";
        case RarityTier::Legendario: return "Legendario";
    }
    return "?";
}

} // namespace petbits
