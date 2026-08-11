#pragma once
/**
 * genome.h — Port de src/core/genome.ts
 *
 * El genoma: una semilla de 64 bits que ES la criatura.
 *
 * No hay tabla de mascotas ni lista de sprites. Cuerpo, color, carácter y
 * rarezas se derivan todos de este número. Mismo número → misma criatura,
 * siempre. Compatibilidad garantizada con el formato del cliente web.
 *
 * INVARIANTE DE PARIDAD: decodeGenome(seed) en C++ debe producir exactamente
 * la misma Genes que su equivalente TypeScript para cualquier seed de 64 bits.
 * Los tests en tests/test_genome.cpp verifican esto contra vectores del TS.
 */

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace petbits {

// ---------------------------------------------------------------------------
// Tipos base
// ---------------------------------------------------------------------------

/** El seed es siempre uint64 sin signo, igual que bigint & MASK64 en TS. */
using Seed = uint64_t;

struct StatBias {
    uint8_t vigor;
    uint8_t animo;
    uint8_t ingenio;
    uint8_t vinculo;
};

/**
 * Genes decodificados del genoma de 64 bits.
 * Campos en el mismo orden y rango que genome.ts para facilitar la revisión.
 */
struct Genes {
    uint8_t lineage;    ///< 0-15  — linaje base (familia de la criatura)
    uint8_t bodyShape;  ///< 0-15  — arquetipo de silueta
    uint8_t eyes;       ///< 0-15  — estilo de ojos
    uint8_t mouth;      ///< 0-15  — estilo de boca
    uint8_t appendages; ///< 0-15  — bitfield: 1=orejas 2=cuernos 4=alas 8=cola
    uint8_t pattern;    ///< 0-15  — patrón sobre el cuerpo
    uint8_t hue;        ///< 0-255 — tono base → 0-360°
    uint8_t paletteMode;  ///< 0-7   — modo de paleta
    uint8_t temperament;  ///< 0-7   — carácter (afecta eventos de simulación)
    uint8_t metabolism;   ///< 0-7   — velocidad de gasto de energía
    uint8_t affinity;     ///< 0-7   — afinidad elemental (sesga rama evolutiva)
    uint8_t proportion;   ///< 0-15  — proporciones de cabeza y cuerpo
    StatBias statBias;    ///< sesgo de estadísticas, 2 bits cada una
    uint8_t mutation;     ///< 0-255 — bits de mutación / reserva
};

// ---------------------------------------------------------------------------
// Catálogos de nombres (mirrors exactos de genome.ts)
// ---------------------------------------------------------------------------

inline constexpr std::array<std::string_view, 16> LINEAGES = {
    "Nébula", "Fungo", "Cristal", "Limo", "Pluma", "Escama",
    "Musgo",  "Brasa", "Vapor",   "Óxido", "Coral", "Cirro",
    "Duna",   "Eco",   "Prisma",  "Raíz",
};

inline constexpr std::array<std::string_view, 8> TEMPERAMENTS = {
    "Plácido", "Curioso", "Arisco", "Leal",
    "Errante", "Voraz",   "Tímido", "Feroz",
};

inline constexpr std::array<std::string_view, 8> AFFINITIES = {
    "Brasa", "Marea", "Raíz",     "Chispa",
    "Escarcha", "Polvo", "Eco",   "Vacío",
};

inline constexpr std::array<std::string_view, 8> METABOLISMS = {
    "Aletargado", "Lento",   "Sereno",  "Regular",
    "Activo",     "Inquieto", "Ávido",  "Frenético",
};

// ---------------------------------------------------------------------------
// Funciones principales
// ---------------------------------------------------------------------------

/**
 * Decodifica el genoma de 64 bits en sus campos.
 * Puro y determinista: mismos bits → misma Genes.
 */
Genes decodeGenome(Seed seed);

/** Formatea el seed como hex agrupado: "A3F0-91C4-77BE-2D08". */
std::string formatSeed(Seed seed);

/**
 * Interpreta la entrada del usuario como seed.
 *
 * Acepta hex con o sin guiones, decimal, o texto arbitrario (hash FNV-1a).
 * Replica exactamente la lógica de parseSeed() del TS.
 *
 * Devuelve false —sin tocar `salida`— solo si la entrada queda vacía después
 * de sacarle los espacios. Cualquier otra cosa es un seed válido.
 *
 * NO LANZA, y por eso la forma de la firma. El TS tira una excepción con la
 * entrada vacía, pero godot-cpp compila con las excepciones deshabilitadas
 * (`disable_exceptions` viene en true): un throw acá no se propaga, termina el
 * proceso. Es un detalle del entorno que no se ve leyendo el TS, y es la razón
 * por la que un port "línea por línea" del original no serviría.
 */
bool parseSeed(std::string_view input, Seed& salida);

/** Hash FNV-1a de 64 bits. Reproduce hashString() del TS. */
Seed hashString(std::string_view text);

/** Genera un seed con entropía del sistema operativo. */
Seed randomSeed();

/** Normaliza al rango de 64 bits sin signo. */
inline Seed normalizeSeed(Seed seed) { return seed; /* uint64 ya es 64 bits */ }

// Helpers de nombre
std::string_view lineageName(const Genes& g);
std::string_view temperamentName(const Genes& g);
std::string_view affinityName(const Genes& g);
std::string_view metabolismName(const Genes& g);

} // namespace petbits
