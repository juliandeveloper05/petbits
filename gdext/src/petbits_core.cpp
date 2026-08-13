/**
 * petbits_core.cpp — ver petbits_core.h.
 */

#include "petbits_core.h"

#include "genome.h"
#include "actions.h"
#include "expeditions.h"
#include "sprite_gen.h"
#include "traits.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <cstring>
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

petbits::CreatureState* PetBitsCore::activa() {
    if (!partida.has_value()) return nullptr;
    for (petbits::CreatureState& c : partida->criaturas) {
        if (c.id == partida->activaId) return &c;
    }
    return nullptr;
}

const petbits::CreatureState* PetBitsCore::activa() const {
    return const_cast<PetBitsCore*>(this)->activa();
}

void PetBitsCore::_bind_methods() {
    ClassDB::bind_method(D_METHOD("formatear_seed", "entrada"), &PetBitsCore::formatear_seed);
    ClassDB::bind_method(D_METHOD("decodificar", "entrada"), &PetBitsCore::decodificar);
    ClassDB::bind_method(D_METHOD("rarezas", "entrada"), &PetBitsCore::rarezas);
    ClassDB::bind_method(D_METHOD("seed_al_azar"), &PetBitsCore::seed_al_azar);
    ClassDB::bind_method(D_METHOD("version"), &PetBitsCore::version);

    ClassDB::bind_method(D_METHOD("nacer", "seed", "ahora_ms", "tz_min"), &PetBitsCore::nacer);
    ClassDB::bind_method(D_METHOD("tiene_criatura"), &PetBitsCore::tiene_criatura);
    ClassDB::bind_method(D_METHOD("simular", "ahora_ms"), &PetBitsCore::simular);
    ClassDB::bind_method(D_METHOD("estado"), &PetBitsCore::estado);

    ClassDB::bind_method(D_METHOD("alimentos"), &PetBitsCore::alimentos);
    ClassDB::bind_method(D_METHOD("alimentar", "alimento_id", "ahora_ms"),
                         &PetBitsCore::alimentar);
    ClassDB::bind_method(D_METHOD("jugar", "ahora_ms"), &PetBitsCore::jugar);
    ClassDB::bind_method(D_METHOD("acariciar", "ahora_ms"), &PetBitsCore::acariciar);

    ClassDB::bind_method(D_METHOD("destinos"), &PetBitsCore::destinos);
    ClassDB::bind_method(D_METHOD("enviar", "destino_id", "ahora_ms"), &PetBitsCore::enviar);
    ClassDB::bind_method(D_METHOD("recibir", "ahora_ms"), &PetBitsCore::recibir);
    ClassDB::bind_method(D_METHOD("falta_para_volver", "ahora_ms"),
                         &PetBitsCore::falta_para_volver);

    ClassDB::bind_method(D_METHOD("sprite", "seed", "etapa", "forma", "parpadeo"),
                         &PetBitsCore::sprite, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("sprite_actual", "parpadeo"), &PetBitsCore::sprite_actual,
                         DEFVAL(false));

    ClassDB::bind_method(D_METHOD("guardar", "ahora_ms"), &PetBitsCore::guardar);
    ClassDB::bind_method(D_METHOD("cargar", "texto"), &PetBitsCore::cargar);
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
        // Estos tres son ASCII puro, así que la conversión directa andaría. Se
        // usa el helper igual: el día que alguien escriba "épico" con tilde, no
        // tiene por qué acordarse de esta distinción.
        rareza["tier"] = aGodot(NOMBRES_TIER[static_cast<int>(t.tier)]);
        rareza["frecuencia"] = t.approxRate;
        salida.push_back(rareza);
    }

    return salida;
}

String PetBitsCore::seed_al_azar() const {
    return aGodot(petbits::formatSeed(petbits::randomSeed()));
}

// ---------------------------------------------------------------------------
// Simulación
// ---------------------------------------------------------------------------

static const char* ETAPAS[] = {"bebe", "juvenil", "adulto"};

bool PetBitsCore::nacer(const String& seed, int64_t ahora_ms, int64_t tz_min) {
    petbits::Seed valor = 0;
    if (!leerSeed(seed, valor)) return false;

    partida = petbits::partidaInicial(
        petbits::createCreature(valor, ahora_ms, static_cast<int>(tz_min)));
    return true;
}

bool PetBitsCore::tiene_criatura() const {
    return activa() != nullptr;
}

Dictionary PetBitsCore::simular(int64_t ahora_ms) {
    Dictionary salida;
    petbits::CreatureState* c = activa();
    if (c == nullptr) return salida;

    const petbits::SimResult r = petbits::simulate(*c, ahora_ms);
    *c = r.state;

    salida["ticks"] = r.ticks;
    salida["omitidos"] = r.omitted;

    Array eventos;
    for (const petbits::SimEvent& e : r.events) {
        Dictionary d;
        d["tipo"] = aGodot(petbits::simEventKindId(e.kind));
        d["cuando_ms"] = e.atMs;
        d["texto"] = aGodot(e.text);
        eventos.push_back(d);
    }
    salida["eventos"] = eventos;

    Dictionary resumen;
    for (const auto& [id, cantidad] : r.summary) {
        resumen[aGodot(id)] = cantidad;
    }
    salida["resumen"] = resumen;

    return salida;
}

Dictionary PetBitsCore::estado() const {
    Dictionary d;
    const petbits::CreatureState* activo = activa();
    if (activo == nullptr) return d;

    const petbits::CreatureState& c = *activo;

    d["id"] = aGodot(c.id);
    d["seed"] = aGodot(petbits::formatSeed(c.seed));
    d["nacimiento_ms"] = c.nacimientoMs;
    d["ultimo_tick_ms"] = c.lastTickMs;
    d["ticks_vividos"] = c.ticksVividos;
    d["ticks_activos"] = c.ticksActivos;
    d["ticks_sin_cuidado"] = c.ticksSinCuidado;
    d["letargico"] = c.letargico;
    d["durmiendo"] = c.durmiendo;
    d["etapa"] = ETAPAS[static_cast<int>(c.etapa)];
    d["forma"] = aGodot(petbits::formName(c.forma));

    Dictionary stats;
    stats["energia"] = c.stats.energia;
    stats["animo"] = c.stats.animo;
    stats["salud"] = c.stats.salud;
    stats["vinculo"] = c.stats.vinculo;
    d["stats"] = stats;

    return d;
}

// ---------------------------------------------------------------------------
// Acciones
// ---------------------------------------------------------------------------

Array PetBitsCore::alimentos() const {
    Array salida;
    for (const petbits::Food& f : petbits::FOODS) {
        Dictionary d;
        d["id"] = aGodot(f.id);
        d["nombre"] = aGodot(f.name);
        d["energia"] = f.energia;
        d["animo"] = f.animo;
        d["salud"] = f.salud;
        // Cuántas quedan, para que el botón pueda decirlo. Sin esto el jugador
        // se entera de que no le queda comida recién al apretar.
        d["cantidad"] = partida.has_value() ? partida->inventario.cuanto(f.id) : 0;
        salida.push_back(d);
    }
    return salida;
}

/** Aplica el resultado al estado guardado y lo traduce a diccionario. */
static Dictionary aplicar(petbits::CreatureState* destino, const petbits::ActionResult& r) {
    Dictionary d;
    d["ok"] = r.ok;
    d["mensaje"] = aGodot(r.message);
    if (r.ok) *destino = r.state;
    return d;
}

Dictionary PetBitsCore::alimentar(const String& alimento_id, int64_t ahora_ms) {
    Dictionary d;
    petbits::CreatureState* c = activa();
    if (c == nullptr) {
        d["ok"] = false;
        d["mensaje"] = String("No hay ninguna criatura.");
        return d;
    }

    const CharString bytes = aBytes(alimento_id);
    const std::string_view id(bytes.get_data(), static_cast<size_t>(bytes.length()));

    // Apartar, actuar, cobrar. Es el orden que usa la web y no es intercambiable:
    //
    //   1. la unidad se descuenta sobre una COPIA de la despensa
    //   2. se llama a alimentar()
    //   3. si la acción falla, la copia se descarta y no se cobró nada
    //
    // Cobrando primero sobre la despensa real, una acción rechazada —la criatura
    // está de expedición, por ejemplo— se comería la baya igual.
    petbits::Inventario despensaPendiente = partida->inventario;
    if (!despensaPendiente.consumir(id)) {
        d["ok"] = false;
        d["mensaje"] = String::utf8("No te queda de eso. Mandala a buscar.");
        return d;
    }

    const petbits::ActionResult r = petbits::alimentar(*c, id, ahora_ms);
    if (!r.ok) {
        d["ok"] = false;
        d["mensaje"] = aGodot(r.message);
        return d;
    }

    *c = r.state;
    partida->inventario = std::move(despensaPendiente);

    d["ok"] = true;
    d["mensaje"] = aGodot(r.message);
    return d;
}

Dictionary PetBitsCore::jugar(int64_t ahora_ms) {
    Dictionary d;
    petbits::CreatureState* c = activa();
    if (c == nullptr) {
        d["ok"] = false;
        d["mensaje"] = String("No hay ninguna criatura.");
        return d;
    }
    return aplicar(c, petbits::jugar(*c, ahora_ms));
}

Dictionary PetBitsCore::acariciar(int64_t ahora_ms) {
    Dictionary d;
    petbits::CreatureState* c = activa();
    if (c == nullptr) {
        d["ok"] = false;
        d["mensaje"] = String("No hay ninguna criatura.");
        return d;
    }
    return aplicar(c, petbits::acariciar(*c, ahora_ms));
}

// ---------------------------------------------------------------------------
// Expediciones
// ---------------------------------------------------------------------------

Array PetBitsCore::destinos() const {
    Array salida;
    const petbits::CreatureState* c = activa();

    for (const petbits::Destino& d : petbits::destinos()) {
        Dictionary dic;
        dic["id"] = aGodot(d.id);
        dic["nombre"] = aGodot(d.nombre);
        dic["descripcion"] = aGodot(d.descripcion);
        dic["duracion_ms"] = d.duracionMs;
        dic["costo_energia"] = d.costoEnergia;

        if (c == nullptr) {
            dic["puede"] = false;
            dic["motivo"] = String("No hay ninguna criatura.");
        } else {
            const petbits::PuedeSalir p = petbits::puedeSalir(*c, d);
            dic["puede"] = p.puede;
            dic["motivo"] = aGodot(p.motivo);
        }

        salida.push_back(dic);
    }
    return salida;
}

Dictionary PetBitsCore::enviar(const String& destino_id, int64_t ahora_ms) {
    Dictionary d;
    petbits::CreatureState* c = activa();
    if (c == nullptr) {
        d["ok"] = false;
        d["mensaje"] = String("No hay ninguna criatura.");
        return d;
    }

    const CharString bytes = aBytes(destino_id);
    const petbits::Destino* destino = petbits::destinoPorId(
        std::string_view(bytes.get_data(), static_cast<size_t>(bytes.length())));
    if (destino == nullptr) {
        d["ok"] = false;
        d["mensaje"] = String("Ese lugar no existe.");
        return d;
    }

    const petbits::PuedeSalir p = petbits::puedeSalir(*c, *destino);
    if (!p.puede) {
        d["ok"] = false;
        d["mensaje"] = aGodot(p.motivo);
        return d;
    }

    *c = petbits::enviar(*c, *destino, ahora_ms);
    d["ok"] = true;
    d["mensaje"] = aGodot("Salió para " + std::string(destino->nombre) + ".");
    return d;
}

Dictionary PetBitsCore::recibir(int64_t ahora_ms) {
    Dictionary d;
    d["volvio"] = false;

    petbits::CreatureState* c = activa();
    if (c == nullptr) return d;

    const petbits::Regreso r = petbits::recibir(*c, ahora_ms);
    if (!r.volvio) return d;

    *c = r.criatura;

    // El botín entra en la despensa acá y no en `recibir` del núcleo, por lo
    // mismo que el descuento de alimentar: el núcleo calcula, la capa de juego
    // decide qué hacer con el resultado. Es también donde el TS lo hace.
    Dictionary botin;
    for (const auto& [id, cantidad] : r.botin.alimentos.items()) {
        if (cantidad <= 0) continue;
        partida->inventario.agregar(id, cantidad);
        botin[aGodot(id)] = cantidad;
    }

    d["volvio"] = true;
    d["destino"] = aGodot(r.destino != nullptr ? r.destino->desde : "");
    d["botin"] = botin;
    d["mensaje"] = aGodot(petbits::describirBotin(r.botin));

    // La semilla va como texto decimal, igual que en el guardado: no entra
    // exacta en un int con signo de GDScript.
    if (r.botin.semilla.has_value()) {
        d["semilla"] = aGodot(petbits::formatSeed(*r.botin.semilla));
        // Se anota en el save para poder incubarla después. Va en `otros`, que
        // es donde vive lo que el nativo todavía no usa: el TS la guarda en
        // decimal, así que se respeta ese formato aunque acá se muestre en hex.
        petbits::Json lista = petbits::Json::arreglo();
        if (const petbits::Json* previa = partida->otros.buscar("semillas")) {
            if (previa->esArreglo()) lista = *previa;
        }
        lista.agregar(petbits::Json::texto(petbits::seedADecimal(*r.botin.semilla)));
        partida->otros.poner("semillas", std::move(lista));
    } else {
        d["semilla"] = String();
    }

    return d;
}

int64_t PetBitsCore::falta_para_volver(int64_t ahora_ms) const {
    const petbits::CreatureState* c = activa();
    if (c == nullptr) return 0;
    return petbits::faltaParaVolver(*c, ahora_ms);
}

// ---------------------------------------------------------------------------
// Sprite
// ---------------------------------------------------------------------------

/** Traduce el texto de etapa. Cualquier cosa desconocida cae en adulto. */
static petbits::Stage etapaDesde(const String& texto) {
    if (texto == "bebe") return petbits::Stage::Bebe;
    if (texto == "juvenil") return petbits::Stage::Juvenil;
    return petbits::Stage::Adulto;
}

/** Acepta tanto el id ("petreo") como el nombre mostrado ("Pétreo"). */
static petbits::Form formaDesde(const String& texto) {
    if (texto == "petreo" || texto == "Pétreo") return petbits::Form::Petreo;
    if (texto == "vaporoso" || texto == "Vaporoso") return petbits::Form::Vaporoso;
    if (texto == "coloso" || texto == "Coloso") return petbits::Form::Coloso;
    if (texto == "guardian" || texto == "Guardián") return petbits::Form::Guardian;
    if (texto == "errante" || texto == "Errante") return petbits::Form::Errante;
    if (texto == "oraculo" || texto == "Oráculo") return petbits::Form::Oraculo;
    return petbits::Form::Indefinida;
}

static Ref<Image> aImagen(const petbits::Sprite& s) {
    PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(s.data.size()));
    // El layout del generador ya es RGBA8 de arriba a abajo, que es exactamente
    // lo que espera Image::create_from_data. No hay conversión: se copia.
    std::memcpy(bytes.ptrw(), s.data.data(), s.data.size());

    return Image::create_from_data(s.width, s.height, /*mipmaps*/ false, Image::FORMAT_RGBA8,
                                   bytes);
}

Ref<Image> PetBitsCore::sprite(const String& seed, const String& etapa, const String& forma,
                               bool parpadeo) const {
    petbits::Seed valor = 0;
    if (!leerSeed(seed, valor)) return Ref<Image>();

    const petbits::Sprite s = petbits::generateSprite(
        valor, etapaDesde(etapa), formaDesde(forma),
        parpadeo ? petbits::Expression::Parpadeo : petbits::Expression::Normal);

    return aImagen(s);
}

Ref<Image> PetBitsCore::sprite_actual(bool parpadeo) const {
    const petbits::CreatureState* c = activa();
    if (c == nullptr) return Ref<Image>();

    const petbits::Sprite s = petbits::generateSprite(
        c->seed, c->etapa, c->forma,
        parpadeo ? petbits::Expression::Parpadeo : petbits::Expression::Normal);

    return aImagen(s);
}

// ---------------------------------------------------------------------------
// Guardado
// ---------------------------------------------------------------------------

String PetBitsCore::guardar(int64_t ahora_ms) const {
    if (!partida.has_value()) return String();
    return aGodot(petbits::guardarPartida(*partida, ahora_ms));
}

Dictionary PetBitsCore::cargar(const String& texto) {
    Dictionary d;

    const CharString bytes = aBytes(texto);
    petbits::Partida cargada;
    std::string error;

    if (!petbits::cargarPartida(std::string(bytes.get_data(), static_cast<size_t>(bytes.length())),
                                cargada, error)) {
        d["ok"] = false;
        d["mensaje"] = aGodot(error);
        // La partida que ya estaba NO se toca. Si alguien abre un archivo roto,
        // perder además lo que tenía cargado sería sumar un problema al que ya
        // tiene.
        return d;
    }

    partida = std::move(cargada);
    d["ok"] = true;
    d["mensaje"] = String("Partida cargada.");
    return d;
}

String PetBitsCore::version() const {
    // aGodot y no String(...) directo. El constructor de String desde const
    // char* NO interpreta UTF-8: ensancha cada byte como si fuera Latin-1. El
    // "·" de acá son dos bytes (C2 B7) y salía en pantalla como "Â·".
    //
    // Es el mismo error que este archivo documenta más arriba, cometido en la
    // única línea que no pasaba por el helper. Vale dejarlo anotado: la
    // conversión correcta hay que usarla siempre, no cuando uno se acuerda.
    return aGodot("PetBits core 3.0.0-fase1 · genome + traits + evolution + rng + simulation");
}
