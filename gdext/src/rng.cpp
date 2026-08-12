/**
 * rng.cpp — ver rng.h para por qué todo es uint32_t.
 */

#include "rng.h"
#include "utf16.h"

#include <cmath>

namespace petbits {

uint64_t splitmix64(uint64_t seed) {
    // El enmascarado a 64 bits que el TS hace con `& MASK64` en cada paso acá
    // es gratis: uint64_t ya es aritmética módulo 2^64.
    uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint32_t deriveSeed(uint64_t seed, std::string_view label) {
    uint64_t h = splitmix64(seed);
    paraCadaUnidadUtf16(label, [&](uint32_t unidad) {
        h = splitmix64(h ^ static_cast<uint64_t>(unidad));
    });
    return static_cast<uint32_t>(h & 0xFFFFFFFFULL);
}

double Rng::next() {
    // Traducción línea por línea del TS:
    //
    //     state = (state + 0x6d2b79f5) >>> 0;
    //     let t = Math.imul(state ^ (state >>> 15), 1 | state);
    //     t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    //     return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    //
    // `Math.imul(a, b)` es la multiplicación de 32 bits quedándose con los 32
    // bits bajos: en uint32_t eso es simplemente `a * b`. Y el `+` de la
    // tercera línea, que en JS podría pasarse de int32, se recorta igual
    // porque el `^` que viene después convierte a int32 — mismo patrón de bits
    // que el desborde de uint32_t.
    estado = estado + 0x6D2B79F5u;

    uint32_t t = (estado ^ (estado >> 15)) * (1u | estado);
    t = ((t + ((t ^ (t >> 7)) * (61u | t))) ^ t);

    // 4294967296 es 2^32: el resultado queda en [0, 1).
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

int64_t Rng::intMenorQue(int64_t maxExclusive) {
    return static_cast<int64_t>(std::floor(next() * static_cast<double>(maxExclusive)));
}

bool Rng::boolCon(double p) {
    return next() < p;
}

} // namespace petbits
