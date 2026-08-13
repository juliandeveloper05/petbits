/**
 * save_manager.cpp — ver save_manager.h.
 *
 * El orden en que se escriben los campos es el del esquema del TS, a propósito.
 * JSON no le da significado al orden, pero dos saves de la misma partida tienen
 * que poder compararse de un vistazo, y con las claves bailando cada diff sería
 * ilegible.
 */

#include "save_manager.h"

#include <algorithm>

namespace petbits {

// ---------------------------------------------------------------------------
// Etapas y formas: texto ↔ enum
// ---------------------------------------------------------------------------

static const char* const ETAPAS[] = {"bebe", "juvenil", "adulto"};
static const char* const FORMAS[] = {"indefinida", "petreo",  "vaporoso", "coloso",
                                     "guardian",   "errante", "oraculo"};

static bool etapaDesdeTexto(const std::string& t, Stage& salida) {
    for (size_t i = 0; i < 3; ++i) {
        if (t == ETAPAS[i]) {
            salida = static_cast<Stage>(i);
            return true;
        }
    }
    return false;
}

static bool formaDesdeTexto(const std::string& t, Form& salida) {
    for (size_t i = 0; i < 7; ++i) {
        if (t == FORMAS[i]) {
            salida = static_cast<Form>(i);
            return true;
        }
    }
    return false;
}

/**
 * El genoma decimal de vuelta a número.
 *
 * No se usa std::stoull: lanza, y acá las excepciones están deshabilitadas. La
 * acumulación deja desbordar a propósito, igual que `& MASK64` del lado del TS,
 * aunque un save bien formado nunca traiga un valor por encima de 2^64-1.
 */
static bool seedDesdeDecimal(const std::string& texto, Seed& salida) {
    if (texto.empty()) return false;
    Seed valor = 0;
    for (const char c : texto) {
        if (c < '0' || c > '9') return false;
        valor = valor * 10u + static_cast<Seed>(c - '0');
    }
    salida = valor;
    return true;
}

// ---------------------------------------------------------------------------
// Criatura → JSON
// ---------------------------------------------------------------------------

Json criaturaAJson(const CreatureState& c) {
    Json stats = Json::objeto();
    stats.poner("energia", Json::numero(c.stats.energia));
    stats.poner("animo", Json::numero(c.stats.animo));
    stats.poner("salud", Json::numero(c.stats.salud));
    stats.poner("vinculo", Json::numero(c.stats.vinculo));

    Json dieta = Json::objeto();
    dieta.poner("proteina", Json::numero(c.crianza.dieta_proteina));
    dieta.poner("dulce", Json::numero(c.crianza.dieta_dulce));
    dieta.poner("mineral", Json::numero(c.crianza.dieta_mineral));
    dieta.poner("raro", Json::numero(c.crianza.dieta_raro));

    Json crianza = Json::objeto();
    crianza.poner("dieta", std::move(dieta));
    crianza.poner("juego", Json::numero(c.crianza.juego));
    crianza.poner("calma", Json::numero(c.crianza.calma));
    crianza.poner("sumaAnimo", Json::numero(c.crianza.sumaAnimo));
    crianza.poner("sumaSalud", Json::numero(c.crianza.sumaSalud));
    crianza.poner("ticksMedidos", Json::numero(static_cast<double>(c.crianza.ticksMedidos)));

    Json j = Json::objeto();
    j.poner("id", Json::texto(c.id));
    // El genoma va como texto decimal porque JSON no sabe representar un entero
    // de 64 bits sin perder precisión: a partir de 2^53 un number de JavaScript
    // deja de ser exacto, y la mitad de los seeds están por encima.
    j.poner("seed", Json::texto(seedADecimal(c.seed)));
    j.poner("nacimientoMs", Json::numero(static_cast<double>(c.nacimientoMs)));
    j.poner("lastTickMs", Json::numero(static_cast<double>(c.lastTickMs)));
    j.poner("ticksVividos", Json::numero(static_cast<double>(c.ticksVividos)));
    j.poner("tzOffsetMin", Json::numero(c.tzOffsetMin));
    j.poner("stats", std::move(stats));
    j.poner("vinculoHoy", Json::numero(c.vinculoHoy));
    j.poner("diaIndice", Json::numero(static_cast<double>(c.diaIndice)));
    j.poner("ticksSinCuidado", Json::numero(static_cast<double>(c.ticksSinCuidado)));
    j.poner("letargico", Json::booleano(c.letargico));
    j.poner("durmiendo", Json::booleano(c.durmiendo));
    j.poner("ticksActivos", Json::numero(static_cast<double>(c.ticksActivos)));
    j.poner("etapa", Json::texto(ETAPAS[static_cast<size_t>(c.etapa)]));
    j.poner("forma", Json::texto(FORMAS[static_cast<size_t>(c.forma)]));
    j.poner("crianza", std::move(crianza));

    if (c.ultimaCruzaMs.has_value()) {
        j.poner("ultimaCruzaMs", Json::numero(static_cast<double>(*c.ultimaCruzaMs)));
    } else {
        j.poner("ultimaCruzaMs", Json::nulo());
    }

    if (c.expedicion.has_value()) {
        Json e = Json::objeto();
        e.poner("destinoId", Json::texto(c.expedicion->destinoId));
        e.poner("salidaMs", Json::numero(static_cast<double>(c.expedicion->salidaMs)));
        e.poner("regresoMs", Json::numero(static_cast<double>(c.expedicion->regresoMs)));
        j.poner("expedicion", std::move(e));
    } else {
        j.poner("expedicion", Json::nulo());
    }

    return j;
}

// ---------------------------------------------------------------------------
// JSON → Criatura
// ---------------------------------------------------------------------------

/** Exige que el campo exista y sea del tipo pedido. */
static const Json* exigir(const Json& j, const char* clave, Json::Tipo tipo, std::string& error) {
    const Json* v = j.buscar(clave);
    if (v == nullptr) {
        error = std::string("falta el campo \"") + clave + "\"";
        return nullptr;
    }
    if (v->tipo() != tipo) {
        error = std::string("el campo \"") + clave + "\" tiene el tipo equivocado";
        return nullptr;
    }
    return v;
}

static bool numeroDe(const Json& j, const char* clave, double& salida, std::string& error) {
    const Json* v = exigir(j, clave, Json::Tipo::Numero, error);
    if (v == nullptr) return false;
    salida = v->comoNumero();
    return true;
}

static bool enteroDe(const Json& j, const char* clave, int64_t& salida, std::string& error) {
    double d = 0;
    if (!numeroDe(j, clave, d, error)) return false;
    salida = static_cast<int64_t>(d);
    return true;
}

bool criaturaDesdeJson(const Json& j, CreatureState& c, std::string& error) {
    if (!j.esObjeto()) {
        error = "la criatura no es un objeto";
        return false;
    }

    const Json* id = exigir(j, "id", Json::Tipo::Texto, error);
    if (id == nullptr) return false;
    c.id = id->comoTexto();
    if (c.id.empty()) {
        error = "la criatura no tiene id";
        return false;
    }

    const Json* seed = exigir(j, "seed", Json::Tipo::Texto, error);
    if (seed == nullptr) return false;
    if (!seedDesdeDecimal(seed->comoTexto(), c.seed)) {
        error = "el genoma no es un entero en texto";
        return false;
    }

    if (!enteroDe(j, "nacimientoMs", c.nacimientoMs, error)) return false;
    if (!enteroDe(j, "lastTickMs", c.lastTickMs, error)) return false;
    if (!enteroDe(j, "ticksVividos", c.ticksVividos, error)) return false;

    int64_t tz = 0;
    if (!enteroDe(j, "tzOffsetMin", tz, error)) return false;
    c.tzOffsetMin = static_cast<int>(tz);

    const Json* stats = exigir(j, "stats", Json::Tipo::Objeto, error);
    if (stats == nullptr) return false;
    if (!numeroDe(*stats, "energia", c.stats.energia, error)) return false;
    if (!numeroDe(*stats, "animo", c.stats.animo, error)) return false;
    if (!numeroDe(*stats, "salud", c.stats.salud, error)) return false;
    if (!numeroDe(*stats, "vinculo", c.stats.vinculo, error)) return false;

    if (!numeroDe(j, "vinculoHoy", c.vinculoHoy, error)) return false;
    if (!enteroDe(j, "diaIndice", c.diaIndice, error)) return false;
    if (!enteroDe(j, "ticksSinCuidado", c.ticksSinCuidado, error)) return false;

    const Json* letargico = exigir(j, "letargico", Json::Tipo::Bool, error);
    if (letargico == nullptr) return false;
    c.letargico = letargico->comoBool();

    const Json* durmiendo = exigir(j, "durmiendo", Json::Tipo::Bool, error);
    if (durmiendo == nullptr) return false;
    c.durmiendo = durmiendo->comoBool();

    if (!enteroDe(j, "ticksActivos", c.ticksActivos, error)) return false;

    const Json* etapa = exigir(j, "etapa", Json::Tipo::Texto, error);
    if (etapa == nullptr) return false;
    if (!etapaDesdeTexto(etapa->comoTexto(), c.etapa)) {
        error = "etapa desconocida: " + etapa->comoTexto();
        return false;
    }

    const Json* forma = exigir(j, "forma", Json::Tipo::Texto, error);
    if (forma == nullptr) return false;
    if (!formaDesdeTexto(forma->comoTexto(), c.forma)) {
        error = "forma desconocida: " + forma->comoTexto();
        return false;
    }

    const Json* crianza = exigir(j, "crianza", Json::Tipo::Objeto, error);
    if (crianza == nullptr) return false;
    const Json* dieta = exigir(*crianza, "dieta", Json::Tipo::Objeto, error);
    if (dieta == nullptr) return false;

    double tmp = 0;
    if (!numeroDe(*dieta, "proteina", tmp, error)) return false;
    c.crianza.dieta_proteina = static_cast<uint32_t>(tmp);
    if (!numeroDe(*dieta, "dulce", tmp, error)) return false;
    c.crianza.dieta_dulce = static_cast<uint32_t>(tmp);
    if (!numeroDe(*dieta, "mineral", tmp, error)) return false;
    c.crianza.dieta_mineral = static_cast<uint32_t>(tmp);
    if (!numeroDe(*dieta, "raro", tmp, error)) return false;
    c.crianza.dieta_raro = static_cast<uint32_t>(tmp);

    if (!numeroDe(*crianza, "juego", tmp, error)) return false;
    c.crianza.juego = static_cast<uint32_t>(tmp);
    if (!numeroDe(*crianza, "calma", tmp, error)) return false;
    c.crianza.calma = static_cast<uint32_t>(tmp);
    if (!numeroDe(*crianza, "sumaAnimo", c.crianza.sumaAnimo, error)) return false;
    if (!numeroDe(*crianza, "sumaSalud", c.crianza.sumaSalud, error)) return false;

    int64_t medidos = 0;
    if (!enteroDe(*crianza, "ticksMedidos", medidos, error)) return false;
    c.crianza.ticksMedidos = static_cast<uint64_t>(medidos);

    // Los dos nullables. Que falten NO es lo mismo que que valgan null: un save
    // sin la clave está mal formado, y decirlo temprano evita cargar una partida
    // a medias.
    const Json* cruza = j.buscar("ultimaCruzaMs");
    if (cruza == nullptr) {
        error = "falta el campo \"ultimaCruzaMs\"";
        return false;
    }
    c.ultimaCruzaMs = cruza->esNulo() ? std::nullopt
                                      : std::optional<int64_t>(cruza->comoEntero());

    const Json* exp = j.buscar("expedicion");
    if (exp == nullptr) {
        error = "falta el campo \"expedicion\"";
        return false;
    }
    if (exp->esNulo()) {
        c.expedicion = std::nullopt;
    } else {
        if (!exp->esObjeto()) {
            error = "\"expedicion\" no es un objeto ni null";
            return false;
        }
        Expedicion e;
        const Json* destino = exigir(*exp, "destinoId", Json::Tipo::Texto, error);
        if (destino == nullptr) return false;
        e.destinoId = destino->comoTexto();
        if (!enteroDe(*exp, "salidaMs", e.salidaMs, error)) return false;
        if (!enteroDe(*exp, "regresoMs", e.regresoMs, error)) return false;
        c.expedicion = e;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Partida
// ---------------------------------------------------------------------------

const CreatureState* criaturaActiva(const Partida& p) {
    for (const CreatureState& c : p.criaturas) {
        if (c.id == p.activaId) return &c;
    }
    return nullptr;
}

void reemplazarCriatura(Partida& p, const CreatureState& criatura) {
    for (CreatureState& c : p.criaturas) {
        if (c.id == criatura.id) {
            c = criatura;
            return;
        }
    }
}

Partida partidaInicial(const CreatureState& criatura) {
    Partida p;
    p.criaturas.push_back(criatura);
    p.activaId = criatura.id;

    p.inventario = inventarioInicial();

    Json codex = Json::objeto();
    codex.poner("linajes", Json::arreglo());
    codex.poner("formas", Json::arreglo());
    codex.poner("rarezas", Json::arreglo());
    codex.poner("totalRegistradas", Json::numero(0));

    p.otros = Json::objeto();
    p.otros.poner("codex", std::move(codex));
    p.otros.poner("semillas", Json::arreglo());

    return p;
}

bool cargarPartida(const std::string& texto, Partida& salida, std::string& error) {
    Json raiz;
    if (!Json::leer(texto, raiz, error)) return false;

    if (!raiz.esObjeto()) {
        error = "el guardado no es un objeto";
        return false;
    }

    const Json* version = raiz.buscar("version");
    if (version == nullptr || !version->esNumero()) {
        error = "el guardado no declara versión";
        return false;
    }
    // Un save más nuevo que este código traería campos que no se saben
    // interpretar, y guardarlo encima los perdería. Mejor negarse a abrirlo.
    if (version->comoEntero() > SAVE_VERSION) {
        error = "el guardado es de una versión más nueva (v" +
                std::to_string(version->comoEntero()) + " contra v" +
                std::to_string(SAVE_VERSION) + ")";
        return false;
    }
    if (version->comoEntero() < SAVE_VERSION) {
        // Las migraciones viven del lado del TS. Traerlas para acá sería
        // duplicar constantes congeladas que no pueden divergir.
        error = "el guardado es de una versión vieja (v" +
                std::to_string(version->comoEntero()) +
                "). Abrilo una vez en la web para que lo migre.";
        return false;
    }

    const Json* criaturas = raiz.buscar("criaturas");
    if (criaturas == nullptr || !criaturas->esArreglo() || criaturas->elementos().empty()) {
        error = "el guardado no tiene criaturas";
        return false;
    }

    salida.criaturas.clear();
    for (const Json& c : criaturas->elementos()) {
        CreatureState estado{};
        if (!criaturaDesdeJson(c, estado, error)) return false;
        salida.criaturas.push_back(std::move(estado));
    }

    const Json* activa = raiz.buscar("activaId");
    if (activa == nullptr || !activa->esTexto() || activa->comoTexto().empty()) {
        error = "el guardado no dice cuál criatura está activa";
        return false;
    }
    salida.activaId = activa->comoTexto();

    if (criaturaActiva(salida) == nullptr) {
        error = "activaId no corresponde a ninguna criatura";
        return false;
    }

    // La despensa sí se interpreta: el juego la gasta.
    salida.inventario = Inventario();
    const Json* inventario = raiz.buscar("inventario");
    if (inventario != nullptr && inventario->esObjeto()) {
        for (const auto& [id, cantidad] : inventario->campos()) {
            if (cantidad.esNumero()) salida.inventario.poner(id, cantidad.comoEntero());
        }
    }

    // Todo lo demás se guarda sin mirar. Es lo que permite que un save de la
    // web pase por el nativo sin perder el codex.
    salida.otros = Json::objeto();
    for (const auto& [clave, valor] : raiz.campos()) {
        if (clave == "version" || clave == "guardadoMs" || clave == "criaturas" ||
            clave == "activaId" || clave == "inventario") {
            continue;
        }
        salida.otros.poner(clave, valor);
    }

    return true;
}

std::string guardarPartida(const Partida& p, int64_t nowMs) {
    Json raiz = Json::objeto();
    raiz.poner("version", Json::numero(static_cast<double>(SAVE_VERSION)));
    raiz.poner("guardadoMs", Json::numero(static_cast<double>(nowMs)));

    Json criaturas = Json::arreglo();
    for (const CreatureState& c : p.criaturas) {
        criaturas.agregar(criaturaAJson(c));
    }
    raiz.poner("criaturas", std::move(criaturas));
    raiz.poner("activaId", Json::texto(p.activaId));

    for (const auto& [clave, valor] : p.otros.campos()) {
        raiz.poner(clave, valor);
    }

    // La despensa va después de `otros` para que quede donde la escribe el TS:
    // entre el codex y las semillas. El orden no cambia el significado, pero sí
    // el diff entre un save de la web y uno del nativo.
    Json inventario = Json::objeto();
    for (const auto& [id, cantidad] : p.inventario.items()) {
        inventario.poner(id, Json::numero(static_cast<double>(cantidad)));
    }
    raiz.poner("inventario", std::move(inventario));

    return raiz.escribir();
}

} // namespace petbits
