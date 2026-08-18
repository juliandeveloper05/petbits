#include "breeding.h"

#include "rng.h"

#include <cmath>

namespace petbits {

std::string_view nombreOrigen(Origen o) {
    switch (o) {
        case Origen::A:      return "A";
        case Origen::B:      return "B";
        case Origen::Mutado: return "mutado";
    }
    return "?";
}

const std::array<CampoGenoma, 14>& camposGenoma() {
    // Los catorce campos cubren los 64 bits sin huecos ni solapes, así que el
    // bucle de mutación siempre encuentra a quién culpar por un bit dado vuelta.
    static const std::array<CampoGenoma, 14> campos = {{
        {"lineage", 0, 4},     {"bodyShape", 4, 4},    {"eyes", 8, 4},
        {"mouth", 12, 4},      {"appendages", 16, 4},  {"pattern", 20, 4},
        {"hue", 24, 8},        {"paletteMode", 32, 3}, {"temperament", 35, 3},
        {"metabolism", 38, 3}, {"affinity", 41, 3},    {"proportion", 44, 4},
        {"statBias", 48, 8},   {"mutation", 56, 8},
    }};
    return campos;
}

Cruza cruzar(Seed seedA, Seed seedB, int64_t nonce) {
    // Se ordenan para sembrar el azar, y eso es lo que la vuelve simétrica:
    // cruzar A con B da lo mismo que cruzar B con A.
    const Seed menor = (seedA <= seedB) ? seedA : seedB;
    const Seed mayor = (seedA <= seedB) ? seedB : seedA;

    // El seed va en DECIMAL. En el TS la etiqueta se arma interpolando el bigint,
    // y esa interpolación rinde base diez. En hexadecimal el hash daría otro
    // número y el hijo saldría distinto, sin que nada más se rompa.
    const std::string etiqueta =
        "cruza:" + seedADecimal(mayor) + ":" + std::to_string(nonce);
    Rng rng(deriveSeed(menor, etiqueta));

    Cruza r;
    const std::array<CampoGenoma, 14>& campos = camposGenoma();

    for (size_t i = 0; i < campos.size(); ++i) {
        const CampoGenoma& campo = campos[i];
        const Seed mascara = ((1ULL << campo.bits) - 1ULL) << campo.offset;

        const Seed desde = rng.boolCon() ? menor : mayor;
        r.seed |= desde & mascara;

        // Contra seedA, no contra el menor: si seedA es el mayor de los dos, la
        // etiqueta "A" sale justamente cuando desde es el mayor. Y con dos padres
        // de genoma idéntico, todo sale "A". Es lo que hace el TS.
        r.herencia[i] = (desde == seedA) ? Origen::A : Origen::B;
    }

    // Mutación, bit a bit. La probabilidad por bit se reparte para que el
    // promedio sea MUTACIONES_ESPERADAS sobre el genoma entero.
    //
    // Los 64 bits se recorren SIEMPRE. Cada vuelta consume un número del PRNG,
    // así que cortar antes correría la secuencia y cambiaría los hijos.
    const double probabilidad = MUTACIONES_ESPERADAS / 64.0;
    for (int bit = 0; bit < 64; ++bit) {
        if (rng.next() >= probabilidad) {
            continue;
        }
        r.seed ^= (1ULL << bit);
        ++r.mutaciones;

        for (size_t i = 0; i < campos.size(); ++i) {
            if (bit >= campos[i].offset && bit < campos[i].offset + campos[i].bits) {
                // El primero que lo contiene, igual que el find() del TS.
                r.herencia[i] = Origen::Mutado;
                break;
            }
        }
    }

    return r;
}

Elegibilidad elegibilidad(const CreatureState& criatura, int64_t nowMs) {
    // El orden es el del TS y no da lo mismo: solo se informa el primer motivo,
    // y "está en letargo" es más útil que "no está sana" cuando pasan las dos.
    if (criatura.etapa != Stage::Adulto) {
        return {false, "Todavía no terminó de crecer."};
    }
    if (criatura.letargico) {
        return {false, "Está en letargo. Primero hay que reconectar con ella."};
    }
    if (criatura.stats.salud < CRUZA_MIN_SALUD) {
        return {false, "No está lo bastante sana."};
    }
    if (criatura.stats.vinculo < CRUZA_MIN_VINCULO) {
        return {false, "Todavía no hay suficiente vínculo con ella."};
    }

    if (criatura.ultimaCruzaMs.has_value()) {
        const int64_t desde = nowMs - *criatura.ultimaCruzaMs;
        if (desde < CRUZA_COOLDOWN_MS) {
            const int64_t faltanMs = CRUZA_COOLDOWN_MS - desde;
            // Hacia arriba y en punto flotante, como el Math.ceil del TS. Con
            // división entera, "faltan 0 h" aparecería durante la última hora.
            const int64_t horas =
                static_cast<int64_t>(std::ceil(static_cast<double>(faltanMs) / 3600000.0));
            return {false, "Necesita descansar. Faltan " + std::to_string(horas) + " h."};
        }
    }

    return {true, ""};
}

Elegibilidad puedenCruzar(const CreatureState& a, const CreatureState& b, int64_t nowMs) {
    if (a.id == b.id) {
        return {false, "Hace falta otra criatura."};
    }

    const Elegibilidad ea = elegibilidad(a, nowMs);
    if (!ea.puede) {
        return ea;
    }

    const Elegibilidad eb = elegibilidad(b, nowMs);
    if (!eb.puede) {
        return eb;
    }

    return {true, ""};
}

CreatureState marcarCruzada(const CreatureState& criatura, int64_t nowMs) {
    CreatureState copia = criatura;
    copia.ultimaCruzaMs = nowMs;
    return copia;
}

std::string describirHerencia(const Cruza& cruza) {
    int deA = 0;
    int deB = 0;
    for (Origen o : cruza.herencia) {
        if (o == Origen::A) ++deA;
        if (o == Origen::B) ++deB;
    }

    // Los mutados no cuentan para ninguno de los dos lados, así que con muchas
    // mutaciones deA y deB pueden sumar bastante menos de catorce. Por eso el
    // caso de "mutó mucho" se pregunta primero.
    if (cruza.mutaciones >= 5) return "Salió con bastante de suyo. Mutó mucho.";
    if (deA > deB * 2) return "Salió casi calcado al primero.";
    if (deB > deA * 2) return "Salió casi calcado al segundo.";
    return "Salió una mezcla pareja de los dos.";
}

} // namespace petbits
