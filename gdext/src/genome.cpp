/**
 * genome.cpp — Port de src/core/genome.ts
 *
 * Ver genome.h para la documentación completa y los invariantes de paridad.
 */

#include "genome.h"

#include <cassert>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#   include <windows.h>
#   include <bcrypt.h>
#   pragma comment(lib, "bcrypt.lib")
#else
#   include <fcntl.h>
#   include <unistd.h>
#endif

namespace petbits {

// ---------------------------------------------------------------------------
// Primitiva: extraer N bits a partir de offset
// ---------------------------------------------------------------------------

static inline uint64_t bits(uint64_t seed, int offset, int length) {
    const uint64_t mask = (length == 64) ? ~0ULL : ((1ULL << length) - 1ULL);
    return (seed >> offset) & mask;
}

// ---------------------------------------------------------------------------
// decodeGenome
// ---------------------------------------------------------------------------

Genes decodeGenome(Seed seed) {
    // Mapa de bits idéntico al GENOME_LAYOUT de genome.ts:
    //   lineage    offset=0  bits=4
    //   bodyShape  offset=4  bits=4
    //   eyes       offset=8  bits=4
    //   mouth      offset=12 bits=4
    //   appendages offset=16 bits=4
    //   pattern    offset=20 bits=4
    //   hue        offset=24 bits=8
    //   paletteMode  offset=32 bits=3
    //   temperament  offset=35 bits=3
    //   metabolism   offset=38 bits=3
    //   affinity     offset=41 bits=3
    //   proportion   offset=44 bits=4
    //   statBias     offset=48 bits=8
    //   mutation     offset=56 bits=8

    Genes g;
    g.lineage     = static_cast<uint8_t>(bits(seed,  0, 4));
    g.bodyShape   = static_cast<uint8_t>(bits(seed,  4, 4));
    g.eyes        = static_cast<uint8_t>(bits(seed,  8, 4));
    g.mouth       = static_cast<uint8_t>(bits(seed, 12, 4));
    g.appendages  = static_cast<uint8_t>(bits(seed, 16, 4));
    g.pattern     = static_cast<uint8_t>(bits(seed, 20, 4));
    g.hue         = static_cast<uint8_t>(bits(seed, 24, 8));
    g.paletteMode = static_cast<uint8_t>(bits(seed, 32, 3));
    g.temperament = static_cast<uint8_t>(bits(seed, 35, 3));
    g.metabolism  = static_cast<uint8_t>(bits(seed, 38, 3));
    g.affinity    = static_cast<uint8_t>(bits(seed, 41, 3));
    g.proportion  = static_cast<uint8_t>(bits(seed, 44, 4));

    const uint8_t statRaw = static_cast<uint8_t>(bits(seed, 48, 8));
    g.statBias.vigor   =  statRaw        & 0b11;
    g.statBias.animo   = (statRaw >> 2)  & 0b11;
    g.statBias.ingenio = (statRaw >> 4)  & 0b11;
    g.statBias.vinculo = (statRaw >> 6)  & 0b11;

    g.mutation = static_cast<uint8_t>(bits(seed, 56, 8));

    return g;
}

// ---------------------------------------------------------------------------
// formatSeed — "A3F0-91C4-77BE-2D08"
// ---------------------------------------------------------------------------

std::string formatSeed(Seed seed) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(16) << seed;
    const std::string hex = oss.str();
    return hex.substr(0, 4) + "-" + hex.substr(4, 4) + "-"
         + hex.substr(8, 4) + "-" + hex.substr(12, 4);
}

// ---------------------------------------------------------------------------
// hashString — FNV-1a 64 bits (mismo que hashString en genome.ts)
// ---------------------------------------------------------------------------

Seed hashString(std::string_view text) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    constexpr uint64_t prime = 0x100000001b3ULL;
    for (unsigned char c : text) {
        hash ^= static_cast<uint64_t>(c);
        hash *= prime;
    }
    return hash;
}

// ---------------------------------------------------------------------------
// parseSeed — replica exacta de parseSeed() en genome.ts
// ---------------------------------------------------------------------------

Seed parseSeed(std::string_view input) {
    // Eliminar espacios y guiones
    std::string compact;
    for (char c : input) {
        if (c != ' ' && c != '-') compact += c;
    }

    if (compact.empty()) {
        throw std::invalid_argument("El seed no puede estar vacío");
    }

    // 0x... → hex explícito
    if (compact.size() > 2 && compact[0] == '0' && (compact[1] == 'x' || compact[1] == 'X')) {
        return std::stoull(compact, nullptr, 16);
    }

    // Todo dígito decimal
    bool allDigits = true;
    for (char c : compact) { if (!std::isdigit(c)) { allDigits = false; break; } }
    if (allDigits) {
        return std::stoull(compact, nullptr, 10);
    }

    // 1-16 caracteres hex (con letras a-f)
    bool allHex = (compact.size() >= 1 && compact.size() <= 16);
    bool hasLetter = false;
    for (char c : compact) {
        if (!std::isxdigit(c)) { allHex = false; break; }
        if (std::isalpha(c)) hasLetter = true;
    }
    if (allHex && hasLetter) {
        return std::stoull(compact, nullptr, 16);
    }
    if (allHex) {
        return std::stoull(compact, nullptr, 16);
    }

    // Texto arbitrario → hash FNV-1a (misma lógica que el TS)
    return hashString(input);  // con input original, no compact
}

// ---------------------------------------------------------------------------
// randomSeed — entropía criptográfica del sistema operativo
// ---------------------------------------------------------------------------

Seed randomSeed() {
    Seed seed = 0;
#ifdef _WIN32
    BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&seed), sizeof(seed),
                    BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, &seed, sizeof(seed));
        close(fd);
    }
#endif
    return seed;
}

// ---------------------------------------------------------------------------
// Helpers de nombre
// ---------------------------------------------------------------------------

template<typename T>
static std::string_view nameFrom(const T& table, uint8_t index) {
    return table[index % table.size()];
}

std::string_view lineageName(const Genes& g)     { return nameFrom(LINEAGES,    g.lineage);    }
std::string_view temperamentName(const Genes& g) { return nameFrom(TEMPERAMENTS, g.temperament); }
std::string_view affinityName(const Genes& g)    { return nameFrom(AFFINITIES,  g.affinity);   }
std::string_view metabolismName(const Genes& g)  { return nameFrom(METABOLISMS, g.metabolism);  }

} // namespace petbits
