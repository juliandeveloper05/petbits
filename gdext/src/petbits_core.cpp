/**
 * petbits_core.cpp — ver petbits_core.h.
 */

#include "petbits_core.h"

#include "genome.h"
#include "traits.h"

#include <godot_cpp/core/class_db.hpp>

#include <string>

using namespace godot;

// ---------------------------------------------------------------------------
// Conversiones
// ---------------------------------------------------------------------------

/**
 * String de Godot → std::string_view utilizable.
 *
 * Se pasa por utf8() porque el núcleo trabaja con bytes UTF-8: `hashString` los
 * decodifica a unidades UTF-16 para coincidir con `charCodeAt` del TypeScript.
 * Con `ascii()` cualquier nombre con tilde daría un seed distinto al de la web.
 *
 * El CharString se devuelve por valor a propósito: es dueño del buffer y
 * quedarse con un puntero a un temporal es apuntar a memoria liberada.
 */
static CharString aBytes(const String& texto) {
    return texto.utf8();
}

static String aGodot(std::string_view texto) {
    return String::utf8(texto.data(), static_cast<int64_t>(texto.size()));
}

/** Interpreta la entrada del jugador. Devuelve false si no hay nada que leer. */
static bool leerSeed(const String& entrada, petbits::Seed& salida) {
    const CharString bytes = aBytes(entrada);
    return petbits::parseSeed(std::string_view(bytes.get_data(), bytes.length()), salida);
}

// ---------------------------------------------------------------------------

void PetBitsCore::_bind_methods() {
    ClassDB::bind_method(D_METHOD("formatear_seed", "entrada"), &PetBitsCore::formatear_seed);
    ClassDB::bind_method(D_METHOD("decodificar", "entrada"), &PetBitsCore::decodificar);
    ClassDB::bind_method(D_METHOD("rarezas", "entrada"), &PetBitsCore::rarezas);
    ClassDB::bind_method(D_METHOD("seed_al_azar"), &PetBitsCore::seed_al_azar);
    ClassDB::bind_method(D_METHOD("version"), &PetBitsCore::version);
}

String PetBitsCore::formatear_seed(const String& entrada) const {
    petbits::Seed seed = 0;
    if (!leerSeed(entrada, seed)) return String();
    return aGodot(petbits::formatSeed(seed));
}

Dictionary PetBitsCore::decodificar(const String& entrada) const {
    Dictionary salida;

    petbits::Seed seed = 0;
    if (!leerSeed(entrada, seed)) return salida;

    const petbits::Genes g = petbits::decodeGenome(seed);

    salida["seed"] = aGodot(petbits::formatSeed(seed));
    salida["lineage"] = g.lineage;
    salida["bodyShape"] = g.bodyShape;
    salida["eyes"] = g.eyes;
    salida["mouth"] = g.mouth;
    salida["appendages"] = g.appendages;
    salida["pattern"] = g.pattern;
    salida["hue"] = g.hue;
    salida["paletteMode"] = g.paletteMode;
    salida["temperament"] = g.temperament;
    salida["metabolism"] = g.metabolism;
    salida["affinity"] = g.affinity;
    salida["proportion"] = g.proportion;
    salida["mutation"] = g.mutation;

    Dictionary sesgo;
    sesgo["vigor"] = g.statBias.vigor;
    sesgo["animo"] = g.statBias.animo;
    sesgo["ingenio"] = g.statBias.ingenio;
    sesgo["vinculo"] = g.statBias.vinculo;
    salida["statBias"] = sesgo;

    // Los nombres van aparte de los índices: la interfaz quiere mostrar
    // "Nébula", pero el resto del juego necesita el número.
    salida["linaje"] = aGodot(petbits::lineageName(g));
    salida["temperamento"] = aGodot(petbits::temperamentName(g));
    salida["afinidad"] = aGodot(petbits::affinityName(g));
    salida["metabolismo"] = aGodot(petbits::metabolismName(g));

    return salida;
}

Array PetBitsCore::rarezas(const String& entrada) const {
    Array salida;

    petbits::Seed seed = 0;
    if (!leerSeed(entrada, seed)) return salida;

    static const char* NOMBRES_TIER[] = {"raro", "epico", "legendario"};

    for (const petbits::Trait& t : petbits::detectTraits(seed)) {
        Dictionary rareza;
        rareza["id"] = aGodot(t.id);
        rareza["nombre"] = aGodot(t.name);
        rareza["regla"] = aGodot(t.rule);
        rareza["tier"] = NOMBRES_TIER[static_cast<int>(t.tier)];
        rareza["frecuencia"] = t.approxRate;
        salida.push_back(rareza);
    }

    return salida;
}

String PetBitsCore::seed_al_azar() const {
    return aGodot(petbits::formatSeed(petbits::randomSeed()));
}

String PetBitsCore::version() const {
    return String("PetBits core 3.0.0-fase1 · genome + traits + evolution");
}
