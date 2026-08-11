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

/**
 * FNV-1a de 64 bits sobre unidades UTF-16, no sobre bytes.
 *
 * ---
 *
 * Esta es la parte que un port hace mal sin darse cuenta.
 *
 * El TS recorre el texto con `charCodeAt`, que devuelve unidades UTF-16. En C++
 * un std::string_view son bytes UTF-8. Para todo el ASCII da igual y por eso
 * pasa desapercibido: "hello" son cinco bytes y cinco unidades, mismo hash.
 *
 * Pero "Nébula" son seis unidades en JS —la é es U+00E9, una sola— y siete
 * bytes en UTF-8, porque la é se codifica como C3 A9. Hasheando bytes da otro
 * número, o sea otra criatura.
 *
 * Y no es un caso rebuscado: el juego está en castellano, los linajes se llaman
 * "Nébula" y "Raíz", y escribir tu nombre como seed es una de las cosas que la
 * interfaz invita a hacer. Bastaba una tilde para que la web y el nativo
 * incubaran bichos distintos con el mismo texto.
 *
 * Así que acá se decodifica UTF-8 a punto de código y se emiten las unidades
 * UTF-16 que emitiría JavaScript, con el par suplente incluido para todo lo que
 * está arriba del plano básico (emojis, por ejemplo).
 */
Seed hashString(std::string_view text) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    constexpr uint64_t prime = 0x100000001b3ULL;

    const auto mezclar = [&](uint32_t unidad) {
        hash ^= static_cast<uint64_t>(unidad);
        hash *= prime;
    };

    const size_t largoTexto = text.size();
    size_t i = 0;

    while (i < largoTexto) {
        const unsigned char b0 = static_cast<unsigned char>(text[i]);
        uint32_t punto = 0;
        size_t largo = 0;

        if (b0 < 0x80) {
            punto = b0;
            largo = 1;
        } else if ((b0 & 0xE0) == 0xC0) {
            punto = b0 & 0x1Fu;
            largo = 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            punto = b0 & 0x0Fu;
            largo = 3;
        } else if ((b0 & 0xF8) == 0xF0) {
            punto = b0 & 0x07u;
            largo = 4;
        } else {
            // Byte suelto que no puede empezar una secuencia. Un string de JS
            // nunca llega así; se mezcla tal cual y se sigue, que es preferible
            // a quedarse en el lugar.
            mezclar(b0);
            ++i;
            continue;
        }

        bool completa = (i + largo <= largoTexto);
        for (size_t k = 1; completa && k < largo; ++k) {
            const unsigned char bk = static_cast<unsigned char>(text[i + k]);
            if ((bk & 0xC0) != 0x80) {
                completa = false;
                break;
            }
            punto = (punto << 6) | (bk & 0x3Fu);
        }
        if (!completa) {
            mezclar(b0);
            ++i;
            continue;
        }

        i += largo;

        if (punto <= 0xFFFF) {
            mezclar(punto);
        } else {
            // Fuera del plano básico: JavaScript lo guarda como dos unidades, y
            // charCodeAt las devuelve por separado.
            const uint32_t resto = punto - 0x10000u;
            mezclar(0xD800u + (resto >> 10));
            mezclar(0xDC00u + (resto & 0x3FFu));
        }
    }

    return hash;
}

// ---------------------------------------------------------------------------
// parseSeed — replica exacta de parseSeed() en genome.ts
// ---------------------------------------------------------------------------

// Los ctype de <cctype> reciben un int que tiene que valer como unsigned char.
// Pasarles un `char` directo es comportamiento indefinido cuando es negativo, y
// en este proyecto eso pasa todo el tiempo: "Nébula", "Raíz", "Tímido" traen
// bytes UTF-8 por encima de 0x7F, que en un char con signo son negativos.
static bool esDigito(char c) { return c >= '0' && c <= '9'; }

static bool esHex(char c) {
    return esDigito(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool esLetraHex(char c) { return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

static bool esEspacio(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int valorHex(char c) {
    if (esDigito(c)) return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}

/**
 * Acumula dígitos dejando que el valor dé la vuelta a los 64 bits.
 *
 * En TS esto es `BigInt(texto) & MASK64`: el número se construye completo, con
 * la precisión que haga falta, y recién al final se recorta. Un uint64_t hace
 * lo mismo solo, porque el desborde de enteros sin signo está definido como
 * aritmética módulo 2^64 — que es exactamente lo que significa `& MASK64`.
 *
 * Lo que NO servía era std::stoull: con un decimal más largo de lo que entra,
 * lanza out_of_range en vez de recortar. Distinto resultado que el TS, y encima
 * un throw en un build sin excepciones.
 */
static Seed acumular(std::string_view texto, unsigned base) {
    Seed valor = 0;
    for (char c : texto) {
        valor = valor * base + static_cast<Seed>(base == 16 ? valorHex(c) : c - '0');
    }
    return valor;
}

bool parseSeed(std::string_view input, Seed& salida) {
    // El TS hace .trim() ANTES de todo, y después hashea el texto ya recortado.
    // Saltearse eso hacía que " hola " y "hola" dieran criaturas distintas en la
    // web y en el nativo.
    size_t inicio = 0;
    size_t fin = input.size();
    while (inicio < fin && esEspacio(input[inicio])) ++inicio;
    while (fin > inicio && esEspacio(input[fin - 1])) --fin;
    const std::string_view recortado = input.substr(inicio, fin - inicio);

    if (recortado.empty()) return false;

    // Se sacan espacios interiores y guiones: "A3F0-91C4-77BE-2D08".
    std::string compacto;
    compacto.reserve(recortado.size());
    for (char c : recortado) {
        if (!esEspacio(c) && c != '-') compacto += c;
    }

    // Entradas como "-" o "---": queda vacío después de limpiar, pero el texto
    // original no lo estaba. En el TS ninguna de las cuatro expresiones regulares
    // matchea la cadena vacía, así que termina cayendo al hash. Devolver "seed
    // inválido" acá sería razonable y sería otra cosa que lo que hace el TS.
    if (compacto.empty()) {
        salida = hashString(recortado);
        return true;
    }

    const std::string_view vista(compacto);

    // 0x… explícito
    if (vista.size() > 2 && vista[0] == '0' && (vista[1] == 'x' || vista[1] == 'X')) {
        const std::string_view cuerpo = vista.substr(2);
        bool todoHex = true;
        for (char c : cuerpo) {
            if (!esHex(c)) { todoHex = false; break; }
        }
        if (todoHex) {
            salida = acumular(cuerpo, 16);
            return true;
        }
    }

    // El orden de los tres casos que siguen es el del TS y no da lo mismo:
    // "1234" es hex y decimal a la vez, y tiene que leerse como decimal.
    bool todoHex = vista.size() <= 16;
    bool tieneLetra = false;
    for (char c : vista) {
        if (!esHex(c)) { todoHex = false; break; }
        if (esLetraHex(c)) tieneLetra = true;
    }
    if (todoHex && tieneLetra) {
        salida = acumular(vista, 16);
        return true;
    }

    bool todoDigito = true;
    for (char c : vista) {
        if (!esDigito(c)) { todoDigito = false; break; }
    }
    if (todoDigito) {
        salida = acumular(vista, 10);
        return true;
    }

    if (todoHex) {
        salida = acumular(vista, 16);
        return true;
    }

    // Cualquier otra cosa: el nombre de alguien, una frase. Se hashea el texto
    // recortado —no el original— para coincidir con el TS.
    salida = hashString(recortado);
    return true;
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
