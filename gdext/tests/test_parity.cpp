/**
 * Tests de paridad TypeScript ↔ C++.
 *
 * Compilar y correr (ver tests/README.md para el detalle por compilador):
 *
 *     g++ -std=c++17 -O2 -I../src test_parity.cpp ../src/genome.cpp \
 *         ../src/traits.cpp ../src/evolution.cpp -o run_tests && ./run_tests
 *
 * ---
 *
 * DOS DECISIONES QUE VALE LA PENA EXPLICAR.
 *
 * 1. Los valores esperados no están acá. Están en vectores_generados.h, que
 *    produce `npm run parity` ejecutando el TypeScript de verdad. Escribirlos a
 *    mano sería pedirle al mismo criterio que hizo el port que se corrija solo:
 *    si leí mal el TS, lo leo mal las dos veces y el test pasa igual.
 *
 * 2. No hay framework. Ni Catch2 ni GoogleTest ni el build de SCons. Estos tres
 *    módulos son C++ puro —no incluyen un solo header de Godot— así que
 *    verificarlos no tendría que exigir tener instalada la cadena entera del
 *    motor. Con un compilador y este archivo alcanza, y eso hace que la
 *    paridad se pueda comprobar el primer día, antes de bajar nada más.
 *
 * El arnés son treinta líneas al final del archivo. Cuando haga falta algo que
 * no da, ahí sí conviene traer un framework.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/actions.h"
#include "../src/evolution.h"
#include "../src/genome.h"
#include "../src/json.h"
#include "../src/save_manager.h"
#include "../src/palette.h"
#include "../src/rng.h"
#include "../src/simulation.h"
#include "../src/sprite_gen.h"
#include "../src/traits.h"
#include "vectores_generados.h"

using namespace petbits;

// ---------------------------------------------------------------------------
// Arnés
// ---------------------------------------------------------------------------

static int comprobaciones = 0;
static int fallos = 0;
static const char* bloqueActual = "";

/** Solo imprime cuando algo falla: mil líneas de "ok" no las lee nadie. */
static void revisar(bool condicion, const char* contexto, const char* detalle) {
    ++comprobaciones;
    if (condicion) return;
    ++fallos;
    // Se cortan los fallos en 40. Cuando un port se rompe suele romperse para
    // todos los vectores a la vez, y treinta mil líneas iguales tapan el resto.
    if (fallos <= 40) {
        std::printf("  FALLA [%s] %s\n         %s\n", bloqueActual, contexto, detalle);
    } else if (fallos == 41) {
        std::printf("  ... (se omiten los siguientes)\n");
    }
}

/**
 * Compara dos doubles por igualdad EXACTA, a propósito.
 *
 * Una tolerancia sería lo razonable si esto midiera algo físico. Acá los dos
 * lados hacen las mismas operaciones en el mismo orden sobre los mismos
 * IEEE-754: si difieren aunque sea en el último bit, es que el orden o una
 * constante no coinciden, y eso se amplifica tick a tick hasta separar las dos
 * criaturas. Un epsilon escondería justamente lo que hay que ver.
 */
static void revisarDobles(double obtenido, double esperado, const char* contexto,
                          const char* campo) {
    char detalle[256];
    std::snprintf(detalle, sizeof(detalle), "%s: C++ dio %.17g, el TS da %.17g", campo, obtenido,
                  esperado);
    revisar(obtenido == esperado, contexto, detalle);
}

static void revisarEnteros(uint64_t obtenido, uint64_t esperado, const char* contexto,
                           const char* campo) {
    char detalle[256];
    std::snprintf(detalle, sizeof(detalle), "%s: C++ dio %llu, el TS da %llu", campo,
                  static_cast<unsigned long long>(obtenido),
                  static_cast<unsigned long long>(esperado));
    revisar(obtenido == esperado, contexto, detalle);
}

static void bloque(const char* nombre) {
    bloqueActual = nombre;
    std::printf("%s\n", nombre);
}

// ---------------------------------------------------------------------------
// genome
// ---------------------------------------------------------------------------

static void probarGenomas() {
    bloque("decodeGenome / formatSeed");

    for (const auto& v : vectores::GENOMAS) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX",
                      static_cast<unsigned long long>(v.seed));

        const Genes g = decodeGenome(v.seed);
        revisarEnteros(g.lineage, v.lineage, ctx, "lineage");
        revisarEnteros(g.bodyShape, v.bodyShape, ctx, "bodyShape");
        revisarEnteros(g.eyes, v.eyes, ctx, "eyes");
        revisarEnteros(g.mouth, v.mouth, ctx, "mouth");
        revisarEnteros(g.appendages, v.appendages, ctx, "appendages");
        revisarEnteros(g.pattern, v.pattern, ctx, "pattern");
        revisarEnteros(g.hue, v.hue, ctx, "hue");
        revisarEnteros(g.paletteMode, v.paletteMode, ctx, "paletteMode");
        revisarEnteros(g.temperament, v.temperament, ctx, "temperament");
        revisarEnteros(g.metabolism, v.metabolism, ctx, "metabolism");
        revisarEnteros(g.affinity, v.affinity, ctx, "affinity");
        revisarEnteros(g.proportion, v.proportion, ctx, "proportion");
        revisarEnteros(g.statBias.vigor, v.vigor, ctx, "statBias.vigor");
        revisarEnteros(g.statBias.animo, v.animo, ctx, "statBias.animo");
        revisarEnteros(g.statBias.ingenio, v.ingenio, ctx, "statBias.ingenio");
        revisarEnteros(g.statBias.vinculo, v.vinculo, ctx, "statBias.vinculo");
        revisarEnteros(g.mutation, v.mutation, ctx, "mutation");

        const std::string formateado = formatSeed(v.seed);
        char detalle[160];
        std::snprintf(detalle, sizeof(detalle), "formatSeed: C++ dio \"%s\", el TS da \"%s\"",
                      formateado.c_str(), v.seedFormateado);
        revisar(formateado == v.seedFormateado, ctx, detalle);
    }
}

static void probarParseo() {
    bloque("parseSeed");

    for (const auto& v : vectores::PARSEOS) {
        char ctx[160];
        std::snprintf(ctx, sizeof(ctx), "entrada \"%s\"", v.entrada);

        Seed obtenido = 0;
        const bool ok = parseSeed(v.entrada, obtenido);
        revisar(ok, ctx, "parseSeed dijo que la entrada estaba vacía y no lo está");
        if (ok) revisarEnteros(obtenido, v.seed, ctx, "seed");
    }

    // La entrada vacía es el único caso en que devuelve false. En el TS tira una
    // excepción; acá no puede, porque godot-cpp compila sin excepciones.
    Seed descartado = 0;
    revisar(!parseSeed("", descartado), "entrada vacía", "tendría que devolver false");
    revisar(!parseSeed("   ", descartado), "solo espacios", "tendría que devolver false");
}

static void probarHashes() {
    bloque("hashString (FNV-1a 64)");

    for (const auto& v : vectores::HASHES) {
        char ctx[128];
        std::snprintf(ctx, sizeof(ctx), "texto \"%s\"", v.texto);
        revisarEnteros(hashString(v.texto), v.hash, ctx, "hash");
    }
}

// ---------------------------------------------------------------------------
// traits
// ---------------------------------------------------------------------------

/** Las rarezas detectadas, como bitmask sobre TRAIT_CATALOG. */
static uint8_t mascaraRarezas(Seed seed) {
    const std::vector<Trait> detectadas = detectTraits(seed);
    uint8_t mascara = 0;
    for (const auto& t : detectadas) {
        for (size_t i = 0; i < TRAIT_CATALOG.size(); ++i) {
            if (TRAIT_CATALOG[i].id == t.id) mascara |= static_cast<uint8_t>(1u << i);
        }
    }
    return mascara;
}

/**
 * Los nombres de las rarezas de una máscara, separados por coma.
 *
 * Con std::string y no con buffers de char: la versión anterior encadenaba
 * strncat calculando el espacio restante a mano en cada llamada, que es
 * exactamente el patrón por el que MSVC avisa. Acá no hay capacidad que
 * calcular ni terminador que recordar.
 */
static std::string describirRarezas(uint8_t mascara) {
    std::string salida;
    for (size_t i = 0; i < TRAIT_CATALOG.size(); ++i) {
        if (!(mascara & (1u << i))) continue;
        if (!salida.empty()) salida += ", ";
        salida += std::string(TRAIT_CATALOG[i].id);
    }
    return salida.empty() ? "(ninguna)" : salida;
}

static void probarRarezas() {
    bloque("detectTraits / rarityTier");

    for (const auto& v : vectores::GENOMAS) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX",
                      static_cast<unsigned long long>(v.seed));

        const uint8_t obtenida = mascaraRarezas(v.seed);
        if (obtenida != v.rarezas) {
            const std::string detalle = "rarezas: C++ dio [" + describirRarezas(obtenida) +
                                        "], el TS da [" + describirRarezas(v.rarezas) + "]";
            revisar(false, ctx, detalle.c_str());
        } else {
            revisar(true, ctx, "");
        }

        revisarEnteros(static_cast<uint64_t>(rarityTier(detectTraits(v.seed))), v.tier, ctx,
                       "rarityTier");
    }
}

// ---------------------------------------------------------------------------
// evolution
// ---------------------------------------------------------------------------

static void probarEvolucion() {
    bloque("resolverJuvenil / resolverAdulto");

    static const char* FORMAS[] = {"indefinida", "petreo",  "vaporoso", "coloso",
                                   "guardian",   "errante", "oraculo"};

    for (const auto& v : vectores::EVOLUCIONES) {
        Crianza c{};
        c.dieta_proteina = v.proteina;
        c.dieta_dulce = v.dulce;
        c.dieta_mineral = v.mineral;
        c.dieta_raro = v.raro;
        c.juego = v.juego;
        c.calma = v.calma;
        c.sumaAnimo = v.sumaAnimo;
        c.sumaSalud = v.sumaSalud;
        c.ticksMedidos = v.ticksMedidos;

        Genes g{};
        g.affinity = v.affinity;

        const Form juvenil = resolverJuvenil(c, g);
        const Form adulto = resolverAdulto(c, g, juvenil);

        char detalle[256];
        std::snprintf(detalle, sizeof(detalle), "juvenil: C++ dio %s, el TS da %s",
                      FORMAS[static_cast<int>(juvenil)], FORMAS[v.juvenil]);
        revisar(static_cast<uint8_t>(juvenil) == v.juvenil, v.nombre, detalle);

        std::snprintf(detalle, sizeof(detalle), "adulto: C++ dio %s, el TS da %s",
                      FORMAS[static_cast<int>(adulto)], FORMAS[v.adulto]);
        revisar(static_cast<uint8_t>(adulto) == v.adulto, v.nombre, detalle);
    }
}

// ---------------------------------------------------------------------------
// rng
// ---------------------------------------------------------------------------

static void probarRng() {
    bloque("mulberry32 / deriveSeed");

    for (const auto& v : vectores::RNGS) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "semilla %u", v.semilla);

        Rng rng(v.semilla);
        for (size_t i = 0; i < 8; ++i) {
            char campo[32];
            std::snprintf(campo, sizeof(campo), "next() #%zu", i);
            revisarDobles(rng.next(), v.valores[i], ctx, campo);
        }
    }

    for (const auto& v : vectores::DERIVES) {
        char ctx[160];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX etiqueta \"%s\"",
                      static_cast<unsigned long long>(v.seed), v.etiqueta);
        revisarEnteros(deriveSeed(v.seed, v.etiqueta), v.derivada, ctx, "deriveSeed");
    }
}

// ---------------------------------------------------------------------------
// simulation
// ---------------------------------------------------------------------------

static void probarSimulacion() {
    bloque("simulate");

    static const char* FORMAS[] = {"indefinida", "petreo",  "vaporoso", "coloso",
                                   "guardian",   "errante", "oraculo"};
    static const char* ETAPAS[] = {"bebe", "juvenil", "adulto"};

    for (const auto& v : vectores::SIMULACIONES) {
        const CreatureState inicial = createCreature(v.seed, v.inicioMs, v.tz);
        const SimResult r = simulate(inicial, v.inicioMs + v.ticksPedidos * TICK_MS);
        const CreatureState& s = r.state;

        revisarEnteros(static_cast<uint64_t>(r.ticks), static_cast<uint64_t>(v.ticks), v.nombre,
                       "ticks");
        revisarEnteros(static_cast<uint64_t>(s.lastTickMs), static_cast<uint64_t>(v.lastTickMs),
                       v.nombre, "lastTickMs");
        revisarEnteros(static_cast<uint64_t>(s.ticksVividos),
                       static_cast<uint64_t>(v.ticksVividos), v.nombre, "ticksVividos");
        revisarEnteros(static_cast<uint64_t>(s.ticksActivos),
                       static_cast<uint64_t>(v.ticksActivos), v.nombre, "ticksActivos");
        revisarEnteros(static_cast<uint64_t>(s.ticksSinCuidado),
                       static_cast<uint64_t>(v.ticksSinCuidado), v.nombre, "ticksSinCuidado");
        revisarEnteros(static_cast<uint64_t>(s.diaIndice), static_cast<uint64_t>(v.diaIndice),
                       v.nombre, "diaIndice");
        revisarEnteros(s.letargico ? 1 : 0, v.letargico, v.nombre, "letargico");
        revisarEnteros(s.durmiendo ? 1 : 0, v.durmiendo, v.nombre, "durmiendo");

        {
            char detalle[192];
            std::snprintf(detalle, sizeof(detalle), "etapa: C++ dio %s, el TS da %s",
                          ETAPAS[static_cast<int>(s.etapa)], ETAPAS[v.etapa]);
            revisar(static_cast<uint8_t>(s.etapa) == v.etapa, v.nombre, detalle);

            std::snprintf(detalle, sizeof(detalle), "forma: C++ dio %s, el TS da %s",
                          FORMAS[static_cast<int>(s.forma)], FORMAS[v.forma]);
            revisar(static_cast<uint8_t>(s.forma) == v.forma, v.nombre, detalle);
        }

        // Los stats son lo más delicado: se acumulan miles de veces.
        revisarDobles(s.stats.energia, v.energia, v.nombre, "energia");
        revisarDobles(s.stats.animo, v.animo, v.nombre, "animo");
        revisarDobles(s.stats.salud, v.salud, v.nombre, "salud");
        revisarDobles(s.stats.vinculo, v.vinculo, v.nombre, "vinculo");
        revisarDobles(s.crianza.sumaAnimo, v.sumaAnimo, v.nombre, "crianza.sumaAnimo");
        revisarDobles(s.crianza.sumaSalud, v.sumaSalud, v.nombre, "crianza.sumaSalud");
        revisarEnteros(s.crianza.ticksMedidos, static_cast<uint64_t>(v.ticksMedidos), v.nombre,
                       "crianza.ticksMedidos");

        revisarEnteros(r.events.size(), static_cast<uint64_t>(v.eventos), v.nombre, "eventos");
        revisarEnteros(static_cast<uint64_t>(r.omitted), static_cast<uint64_t>(v.omitidos),
                       v.nombre, "omitidos");

        const auto cuenta = [&](const char* id) -> uint64_t {
            const auto it = r.summary.find(id);
            return it == r.summary.end() ? 0 : static_cast<uint64_t>(it->second);
        };
        revisarEnteros(cuenta("hallazgo"), static_cast<uint64_t>(v.hallazgos), v.nombre,
                       "hallazgos");
        revisarEnteros(cuenta("evolucion"), static_cast<uint64_t>(v.evoluciones), v.nombre,
                       "evoluciones");
        revisarEnteros(cuenta("durmio"), static_cast<uint64_t>(v.durmio), v.nombre, "durmio");
        revisarEnteros(cuenta("desperto"), static_cast<uint64_t>(v.desperto), v.nombre, "desperto");
        revisarEnteros(cuenta("letargo"), static_cast<uint64_t>(v.letargo), v.nombre, "letargo");
        revisarEnteros(cuenta("hambre"), static_cast<uint64_t>(v.hambre), v.nombre, "hambre");
        revisarEnteros(cuenta("animo"), static_cast<uint64_t>(v.animoEv), v.nombre, "animo (evento)");
        revisarEnteros(cuenta("salud"), static_cast<uint64_t>(v.saludEv), v.nombre, "salud (evento)");
    }
}

/**
 * El invariante de partición.
 *
 * Simular un intervalo de una sola vez tiene que dar exactamente lo mismo que
 * simularlo en pedazos. No hace falta compararlo contra el TypeScript: es una
 * propiedad del código en sí, y se comprueba acá.
 *
 * Es LA propiedad que hace que el juego funcione. El jugador cierra la pestaña
 * cuando quiere y vuelve cuando quiere, así que el mismo tiempo transcurrido se
 * simula partido de mil maneras distintas. Si el resultado dependiera de en
 * cuántos pedazos se hizo, dos jugadores con la misma criatura y la misma
 * ausencia tendrían criaturas distintas — y sería imposible de reproducir, que
 * es lo peor de todo.
 *
 * Rompe si el azar se arrastra entre ticks en vez de sembrarse por índice, o si
 * algún tick lee el reloj de afuera en vez del de su propia frontera.
 */
static void probarParticion() {
    bloque("invariante de partición");

    const int64_t base = 1786406400000LL;
    const Seed seed = 0xA3F091C477BE2D08ULL;

    // Cortes elegidos para caer en lugares incómodos: sobre el cambio de día,
    // sobre el borde del letargo, y en números que no son múltiplos redondos.
    const int64_t cortes[] = {1, 7, 60, 719, 1440, 1441, 2879, 2880, 2881, 3607};
    const int64_t total = 4000;

    for (const int64_t corte : cortes) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "corte en el tick %lld",
                      static_cast<long long>(corte));

        const CreatureState inicial = createCreature(seed, base, -180);

        // De una sola vez.
        const SimResult entero = simulate(inicial, base + total * TICK_MS);

        // En dos pedazos.
        const SimResult primero = simulate(inicial, base + corte * TICK_MS);
        const SimResult segundo = simulate(primero.state, base + total * TICK_MS);

        const CreatureState& a = entero.state;
        const CreatureState& b = segundo.state;

        revisarEnteros(static_cast<uint64_t>(b.lastTickMs), static_cast<uint64_t>(a.lastTickMs),
                       ctx, "lastTickMs");
        revisarEnteros(static_cast<uint64_t>(b.ticksVividos),
                       static_cast<uint64_t>(a.ticksVividos), ctx, "ticksVividos");
        revisarEnteros(static_cast<uint64_t>(b.ticksActivos),
                       static_cast<uint64_t>(a.ticksActivos), ctx, "ticksActivos");
        revisarEnteros(b.letargico ? 1 : 0, a.letargico ? 1 : 0, ctx, "letargico");
        revisarEnteros(b.durmiendo ? 1 : 0, a.durmiendo ? 1 : 0, ctx, "durmiendo");
        revisarEnteros(static_cast<uint8_t>(b.etapa), static_cast<uint8_t>(a.etapa), ctx, "etapa");
        revisarEnteros(static_cast<uint8_t>(b.forma), static_cast<uint8_t>(a.forma), ctx, "forma");
        revisarDobles(b.stats.energia, a.stats.energia, ctx, "energia");
        revisarDobles(b.stats.animo, a.stats.animo, ctx, "animo");
        revisarDobles(b.stats.salud, a.stats.salud, ctx, "salud");
        revisarDobles(b.crianza.sumaAnimo, a.crianza.sumaAnimo, ctx, "crianza.sumaAnimo");
        revisarDobles(b.crianza.sumaSalud, a.crianza.sumaSalud, ctx, "crianza.sumaSalud");

        // Los eventos también: la suma de los dos tramos tiene que dar lo mismo
        // que el entero, tipo por tipo.
        for (const auto& [id, cantidad] : entero.summary) {
            const auto p = primero.summary.find(id);
            const auto s = segundo.summary.find(id);
            const int64_t partido = (p == primero.summary.end() ? 0 : p->second) +
                                    (s == segundo.summary.end() ? 0 : s->second);
            char campo[96];
            std::snprintf(campo, sizeof(campo), "eventos \"%s\"", id.c_str());
            revisarEnteros(static_cast<uint64_t>(partido), static_cast<uint64_t>(cantidad), ctx,
                           campo);
        }
    }
}

// ---------------------------------------------------------------------------
// actions
// ---------------------------------------------------------------------------

static void probarAcciones() {
    bloque("alimentar / jugar / acariciar");

    for (const auto& v : vectores::ACCIONES) {
        // El estado inicial se arma campo por campo desde el vector. Varios de
        // estos escenarios —salud por debajo de 30, el tope de vínculo ya
        // alcanzado, una expedición en curso— no se alcanzan simulando: hay que
        // ponerlos.
        CreatureState s = createCreature(v.seed, v.nowMs, v.tz);
        s.stats.energia = v.energia;
        s.stats.animo = v.animo;
        s.stats.salud = v.salud;
        s.stats.vinculo = v.vinculo;
        s.vinculoHoy = v.vinculoHoy;
        s.diaIndice = v.diaIndice;
        s.letargico = v.letargico != 0;
        s.ticksSinCuidado = v.ticksSinCuidado;
        if (v.conExpedicion != 0) {
            s.expedicion = Expedicion{"patio", v.nowMs, v.nowMs + 900'000};
        }

        bool ultimoOk = true;
        std::string ultimoMensaje;

        for (const char* p = v.secuencia; *p != '\0'; ++p) {
            ActionResult r{};
            switch (*p) {
                case 'j': r = jugar(s, v.nowMs); break;
                case 'a': r = acariciar(s, v.nowMs); break;
                case 'b': r = alimentar(s, "baya", v.nowMs); break;
                case 'r': r = alimentar(s, "raiz", v.nowMs); break;
                case 'l': r = alimentar(s, "larva", v.nowMs); break;
                case 'c': r = alimentar(s, "cristal", v.nowMs); break;
                default:  r = alimentar(s, "no-existe", v.nowMs); break;
            }
            ultimoOk = r.ok;
            ultimoMensaje = r.message;
            if (r.ok) s = r.state;
        }

        revisarDobles(s.stats.energia, v.eEnergia, v.nombre, "energia");
        revisarDobles(s.stats.animo, v.eAnimo, v.nombre, "animo");
        revisarDobles(s.stats.salud, v.eSalud, v.nombre, "salud");
        revisarDobles(s.stats.vinculo, v.eVinculo, v.nombre, "vinculo");
        revisarDobles(s.vinculoHoy, v.eVinculoHoy, v.nombre, "vinculoHoy");
        revisarEnteros(static_cast<uint64_t>(s.diaIndice), static_cast<uint64_t>(v.eDiaIndice),
                       v.nombre, "diaIndice");
        revisarEnteros(s.letargico ? 1 : 0, v.eLetargico, v.nombre, "letargico");
        revisarEnteros(static_cast<uint64_t>(s.ticksSinCuidado),
                       static_cast<uint64_t>(v.eTicksSinCuidado), v.nombre, "ticksSinCuidado");

        revisarEnteros(s.crianza.dieta_proteina, v.eProteina, v.nombre, "dieta.proteina");
        revisarEnteros(s.crianza.dieta_dulce, v.eDulce, v.nombre, "dieta.dulce");
        revisarEnteros(s.crianza.dieta_mineral, v.eMineral, v.nombre, "dieta.mineral");
        revisarEnteros(s.crianza.dieta_raro, v.eRaro, v.nombre, "dieta.raro");
        revisarEnteros(s.crianza.juego, v.eJuego, v.nombre, "crianza.juego");
        revisarEnteros(s.crianza.calma, v.eCalma, v.nombre, "crianza.calma");

        revisarEnteros(ultimoOk ? 1 : 0, v.eOk, v.nombre, "ok de la última acción");

        // El mensaje se compara entero. No es adorno: es lo que le dice al
        // jugador por qué pasó lo que pasó, y cada texto corresponde a una rama
        // distinta —comió con ganas o sin ganas, jugó entero o a media máquina—.
        // Si el estado coincide pero el mensaje no, el port tomó otro camino
        // para llegar al mismo número, y eso es un bug esperando.
        const std::string detalle =
            "mensaje: C++ dio \"" + ultimoMensaje + "\", el TS da \"" + v.eMensaje + "\"";
        revisar(ultimoMensaje == v.eMensaje, v.nombre, detalle.c_str());
    }
}

// ---------------------------------------------------------------------------
// save_manager
// ---------------------------------------------------------------------------

static void probarGuardado() {
    bloque("cargar y guardar la partida");

    for (const auto& v : vectores::SAVES) {
        Partida p;
        std::string error;

        // 1. Leer el save que escribió la web.
        if (!cargarPartida(v.json, p, error)) {
            revisar(false, v.nombre, ("no se pudo cargar: " + error).c_str());
            continue;
        }

        const CreatureState* c = criaturaActiva(p);
        if (c == nullptr) {
            revisar(false, v.nombre, "el save cargó pero no hay criatura activa");
            continue;
        }

        revisarEnteros(c->seed, v.seed, v.nombre, "seed");
        revisar(c->id == v.id, v.nombre, "el id no coincide");
        revisarEnteros(static_cast<uint64_t>(c->lastTickMs),
                       static_cast<uint64_t>(v.lastTickMs), v.nombre, "lastTickMs");
        revisarEnteros(static_cast<uint64_t>(c->ticksVividos),
                       static_cast<uint64_t>(v.ticksVividos), v.nombre, "ticksVividos");
        revisarEnteros(static_cast<uint64_t>(c->ticksActivos),
                       static_cast<uint64_t>(v.ticksActivos), v.nombre, "ticksActivos");
        revisarEnteros(c->letargico ? 1 : 0, v.letargico, v.nombre, "letargico");
        revisarEnteros(static_cast<uint8_t>(c->etapa), v.etapa, v.nombre, "etapa");
        revisarEnteros(static_cast<uint8_t>(c->forma), v.forma, v.nombre, "forma");
        revisarDobles(c->stats.energia, v.energia, v.nombre, "energia");
        revisarDobles(c->stats.animo, v.animo, v.nombre, "animo");
        revisarDobles(c->stats.salud, v.salud, v.nombre, "salud");
        revisarDobles(c->crianza.sumaAnimo, v.sumaAnimo, v.nombre, "crianza.sumaAnimo");
        revisarDobles(c->crianza.sumaSalud, v.sumaSalud, v.nombre, "crianza.sumaSalud");

        // 2. Volver a escribirlo y volver a leerlo. Los stats acumulados tienen
        //    quince dígitos significativos: si el número se escribiera con menos
        //    precisión de la necesaria, la ida y vuelta lo cambiaría y la
        //    criatura se iría corriendo un poquito en cada guardado.
        const std::string reescrito = guardarPartida(p, 1786406400000LL);
        Partida q;
        if (!cargarPartida(reescrito, q, error)) {
            revisar(false, v.nombre, ("lo que escribimos no se puede releer: " + error).c_str());
            continue;
        }

        const CreatureState* d = criaturaActiva(q);
        if (d == nullptr) {
            revisar(false, v.nombre, "el save reescrito quedó sin criatura activa");
            continue;
        }

        revisarDobles(d->stats.energia, c->stats.energia, v.nombre, "ida y vuelta: energia");
        revisarDobles(d->stats.animo, c->stats.animo, v.nombre, "ida y vuelta: animo");
        revisarDobles(d->stats.salud, c->stats.salud, v.nombre, "ida y vuelta: salud");
        revisarDobles(d->crianza.sumaAnimo, c->crianza.sumaAnimo, v.nombre,
                      "ida y vuelta: sumaAnimo");
        revisarDobles(d->crianza.sumaSalud, c->crianza.sumaSalud, v.nombre,
                      "ida y vuelta: sumaSalud");
        revisarEnteros(d->seed, c->seed, v.nombre, "ida y vuelta: seed");

        // 3. Y lo que el C++ NO entiende tiene que seguir ahí. Es la parte que
        //    hace seguro compartir el formato: abrir la partida en el nativo no
        //    puede borrarte el codex.
        const Json* codex = q.otros.buscar("codex");
        if (codex == nullptr || !codex->esObjeto()) {
            revisar(false, v.nombre, "se perdió el codex al guardar");
        } else {
            const Json* total = codex->buscar("totalRegistradas");
            revisarEnteros(total != nullptr ? static_cast<uint64_t>(total->comoEntero()) : 0, 7,
                           v.nombre, "codex.totalRegistradas preservado");
            const Json* linajes = codex->buscar("linajes");
            revisarEnteros(linajes != nullptr ? linajes->elementos().size() : 0, 3, v.nombre,
                           "codex.linajes preservado");
        }

        const Json* inv = q.otros.buscar("inventario");
        if (inv == nullptr || !inv->esObjeto()) {
            revisar(false, v.nombre, "se perdió el inventario al guardar");
        } else {
            const Json* bayas = inv->buscar("baya");
            revisarEnteros(bayas != nullptr ? static_cast<uint64_t>(bayas->comoEntero()) : 0, 3,
                           v.nombre, "inventario.baya preservado");
        }

        const Json* semillas = q.otros.buscar("semillas");
        if (semillas == nullptr || !semillas->esArreglo()) {
            revisar(false, v.nombre, "se perdieron las semillas al guardar");
        } else {
            revisarEnteros(semillas->elementos().size(), 2, v.nombre, "semillas preservadas");
            // El genoma más grande no entra en un double sin perder precisión;
            // por eso viaja como texto. Si alguien lo convirtiera a número en el
            // camino, este valor volvería redondeado.
            const bool intacta = !semillas->elementos().empty() &&
                                 semillas->elementos()[0].comoTexto() == "11814994175403368200";
            revisar(intacta, v.nombre, "la semilla de 20 dígitos volvió cambiada");
        }
    }
}

/** El JSON tiene que aguantar entradas rotas sin reventar el arranque. */
static void probarJsonRoto() {
    bloque("guardados corruptos");

    static const char* BASURA[] = {
        "",
        "{",
        "null",
        "[1,2,3]",
        "{\"version\":5}",
        "{\"version\":\"cinco\",\"criaturas\":[]}",
        "{\"version\":5,\"criaturas\":[],\"activaId\":\"x\"}",
        "{\"version\":99,\"criaturas\":[{}],\"activaId\":\"x\"}",
        "{\"version\":1,\"criaturas\":[{}],\"activaId\":\"x\"}",
        "no soy json",
        "{\"version\":5,\"criaturas\":[{\"id\":\"a\"}],\"activaId\":\"a\"}",
    };

    for (const char* texto : BASURA) {
        char ctx[128];
        std::snprintf(ctx, sizeof(ctx), "entrada \"%.40s\"", texto);

        Partida p;
        std::string error;
        const bool cargo = cargarPartida(texto, p, error);
        revisar(!cargo, ctx, "un guardado corrupto no tendría que cargar");
        // Y tiene que decir por qué: "no se pudo" a secas no le sirve a nadie
        // que esté tratando de recuperar una partida.
        revisar(!error.empty(), ctx, "se rechazó sin explicar el motivo");
    }
}

/** El reloj para atrás no tiene que perder nada ni avanzar el tiempo. */
static void probarRelojAtras() {
    bloque("reloj hacia atrás");

    const int64_t base = 1786406400000LL;
    const CreatureState inicial = createCreature(0xA3F091C477BE2D08ULL, base, -180);
    const SimResult avanzado = simulate(inicial, base + 500 * TICK_MS);

    const SimResult atras = simulate(avanzado.state, base + 100 * TICK_MS);
    revisarEnteros(static_cast<uint64_t>(atras.ticks), 0, "reloj atrás", "ticks");
    revisarEnteros(static_cast<uint64_t>(atras.state.lastTickMs),
                   static_cast<uint64_t>(avanzado.state.lastTickMs), "reloj atrás", "lastTickMs");
    revisarDobles(atras.state.stats.energia, avanzado.state.stats.energia, "reloj atrás",
                  "energia");
    revisarEnteros(atras.summary.count("reloj"), 1, "reloj atrás", "evento de reloj");
}

// ---------------------------------------------------------------------------
// palette
// ---------------------------------------------------------------------------

static void probarRampas() {
    bloque("buildRamp (OKLCH)");

    static const char* NOMBRES[] = {"contorno", "sombra", "base", "luz", "acento"};
    static const char* FORMAS[] = {"indefinida", "petreo",  "vaporoso", "coloso",
                                   "guardian",   "errante", "oraculo"};

    for (const auto& v : vectores::RAMPAS) {
        char ctx[96];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX forma %s",
                      static_cast<unsigned long long>(v.seed), FORMAS[v.forma]);

        const Ramp r = buildRamp(decodeGenome(v.seed), detectTraits(v.seed),
                                 static_cast<Form>(v.forma));

        for (size_t i = 0; i < 5; ++i) {
            const uint8_t esperados[3] = {v.rgb[i * 3], v.rgb[i * 3 + 1], v.rgb[i * 3 + 2]};
            if (r[i].r == esperados[0] && r[i].g == esperados[1] && r[i].b == esperados[2]) {
                revisar(true, ctx, "");
                continue;
            }
            char detalle[192];
            std::snprintf(detalle, sizeof(detalle),
                          "%s: C++ dio (%u,%u,%u), el TS da (%u,%u,%u)", NOMBRES[i], r[i].r,
                          r[i].g, r[i].b, esperados[0], esperados[1], esperados[2]);
            revisar(false, ctx, detalle);
        }
    }
}

// ---------------------------------------------------------------------------
// sprite_gen
// ---------------------------------------------------------------------------

static uint32_t contarOpacos(const std::vector<uint8_t>& datos) {
    uint32_t total = 0;
    for (size_t i = 3; i < datos.size(); i += 4) {
        if (datos[i] > 0) ++total;
    }
    return total;
}

static void probarSprites() {
    bloque("generateSprite");

    static const char* ETAPAS[] = {"bebe", "juvenil", "adulto"};
    static const char* FORMAS[] = {"indefinida", "petreo",  "vaporoso", "coloso",
                                   "guardian",   "errante", "oraculo"};

    for (const auto& v : vectores::SPRITES) {
        char ctx[160];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX %s %s%s",
                      static_cast<unsigned long long>(v.seed), ETAPAS[v.etapa], FORMAS[v.forma],
                      v.expresion == 1 ? " parpadeo" : "");

        const Sprite s = generateSprite(
            v.seed, static_cast<Stage>(v.etapa), static_cast<Form>(v.forma),
            v.expresion == 1 ? Expression::Parpadeo : Expression::Normal);

        // El conteo de opacos va PRIMERO. Si la silueta cambió, el hash también,
        // y mirar el hash no diría cuál de las dos cosas se movió.
        revisarEnteros(contarOpacos(s.data), v.opacos, ctx, "píxeles opacos");
        revisarEnteros(hashPixels(s.data), v.hash, ctx, "hash del buffer");
    }
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("\nPetBits — paridad TypeScript <-> C++\n");
    std::printf("%zu genomas, %zu crianzas, %zu parseos, %zu hashes, %zu simulaciones,\n"
                "%zu rampas de color, %zu sprites, %zu escenarios de acciones,\n"
                "%zu guardados\n\n",
                sizeof(vectores::GENOMAS) / sizeof(vectores::GENOMAS[0]),
                sizeof(vectores::EVOLUCIONES) / sizeof(vectores::EVOLUCIONES[0]),
                sizeof(vectores::PARSEOS) / sizeof(vectores::PARSEOS[0]),
                sizeof(vectores::HASHES) / sizeof(vectores::HASHES[0]),
                sizeof(vectores::SIMULACIONES) / sizeof(vectores::SIMULACIONES[0]),
                sizeof(vectores::RAMPAS) / sizeof(vectores::RAMPAS[0]),
                sizeof(vectores::SPRITES) / sizeof(vectores::SPRITES[0]),
                sizeof(vectores::ACCIONES) / sizeof(vectores::ACCIONES[0]),
                sizeof(vectores::SAVES) / sizeof(vectores::SAVES[0]));

    probarGenomas();
    probarParseo();
    probarHashes();
    probarRarezas();
    probarEvolucion();
    probarRng();
    probarRampas();
    probarSprites();
    probarSimulacion();
    probarAcciones();
    probarGuardado();
    probarJsonRoto();
    probarParticion();
    probarRelojAtras();

    std::printf("\n%d comprobaciones, %d fallas\n", comprobaciones, fallos);
    if (fallos == 0) {
        std::printf("Paridad OK: el C++ da exactamente lo mismo que el TypeScript.\n\n");
        return 0;
    }
    std::printf("PARIDAD ROTA. Un mismo seed da criaturas distintas en la web y en el nativo.\n\n");
    return 1;
}
