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
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "../src/actions.h"
#include "../src/evolution.h"
#include "../src/expeditions.h"
#include "../src/genome.h"
#include "../src/inventory.h"
#include "../src/json.h"
#include "../src/save_manager.h"
#include "../src/palette.h"
#include "../src/rng.h"
#include "../src/simulation.h"
#include "../src/sprite_gen.h"
#include "../src/breeding.h"
#include "../src/codex.h"
#include "../src/font_gen.h"
#include "../src/tileset_gen.h"
#include "../src/world_gen.h"
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

        // 3. El codex. Ya NO viaja dentro de `otros`: se interpreta, porque el
        //    juego lo escribe. Eso convierte esta comprobacion en una mas fuerte
        //    que la que habia antes — antes bastaba con que el bloque opaco
        //    siguiera ahi, ahora hay que leer cada campo y volver a escribirlo
        //    sin perder nada.
        revisarEnteros(static_cast<uint64_t>(q.codex.totalRegistradas), 7, v.nombre,
                       "codex.totalRegistradas tras ida y vuelta");
        revisarEnteros(q.codex.linajes.size(), 3, v.nombre, "codex.linajes tras ida y vuelta");
        revisarEnteros(q.codex.rarezas.size(), p.codex.rarezas.size(), v.nombre,
                       "codex.rarezas tras ida y vuelta");
        revisarEnteros(q.codex.formas.size(), p.codex.formas.size(), v.nombre,
                       "codex.formas tras ida y vuelta");

        // El orden que entro es el que sale. Leer NO ordena, ni aca ni en el TS:
        // ordenar es cosa de `registrar`, que es cuando el codex cambia.
        //
        // La primera version de este test exigia que las listas salieran
        // ordenadas y fallo, con razon. El fixture trae `["petreo", "coloso"]`
        // justamente al reves a proposito, y ninguna de las dos implementaciones
        // lo toca. Ordenar al cargar seria peor que inutil: el nativo
        // "arreglaria" un archivo que la web deja como esta, y el diff entre los
        // dos —que es la herramienta con la que se encontro el bug del
        // inventario— empezaria a mostrar ruido que no significa nada.
        {
            std::vector<std::string> entraron;
            for (Form f : p.codex.formas) entraron.push_back(std::string(formId(f)));
            std::vector<std::string> salieron;
            for (Form f : q.codex.formas) salieron.push_back(std::string(formId(f)));
            revisar(entraron == salieron, v.nombre,
                    "el codex reordeno las formas al pasar por el archivo");
            revisar(p.codex.linajes == q.codex.linajes, v.nombre,
                    "el codex reordeno los linajes al pasar por el archivo");
            revisar(p.codex.rarezas == q.codex.rarezas, v.nombre,
                    "el codex reordeno las rarezas al pasar por el archivo");
        }

        // Y ya no puede quedar un "codex" colgado en el mapa de paso: si
        // quedara, el archivo tendria la clave dos veces.
        revisar(q.otros.buscar("codex") == nullptr, v.nombre,
                "el codex quedo tambien en el mapa de campos sin interpretar");

        // El inventario NO va en `otros`: se interpreta, porque el juego lo
        // gasta. Que llegue acá con los valores del save es lo que hace que
        // alimentar pueda cobrar.
        revisarEnteros(static_cast<uint64_t>(q.inventario.cuanto("baya")), 3, v.nombre,
                       "inventario.baya leído");
        revisarEnteros(static_cast<uint64_t>(q.inventario.cuanto("raiz")), 1, v.nombre,
                       "inventario.raiz leído");
        revisarEnteros(static_cast<uint64_t>(q.inventario.cuanto("cristal")), 0, v.nombre,
                       "inventario.cristal leído");
        // Y sobrevive la ida y vuelta por el archivo.
        revisarEnteros(static_cast<uint64_t>(q.inventario.cuanto("baya")),
                       static_cast<uint64_t>(p.inventario.cuanto("baya")), v.nombre,
                       "inventario tras ida y vuelta");

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

/**
 * La despensa se gasta, y solo cuando la acción salió bien.
 *
 * Este bloque existe por un bug que no encontró ningún test: apareció jugando
 * una partida de verdad y pasándola de la web al nativo. El inventario se
 * guardaba y se devolvía intacto —eso estaba testeado— pero nadie lo descontaba,
 * así que del lado nativo la comida era infinita. Como la dieta decide la rama
 * evolutiva, se podía empujar una evolución sin pagar lo que la web sí cobra.
 *
 * El orden importa tanto como el descuento: apartar, actuar, y recién cobrar si
 * la acción salió bien. Cobrando primero, una acción rechazada te comería la
 * baya igual.
 */
static void probarDespensa() {
    bloque("la despensa se gasta");

    const Inventario inicial = inventarioInicial();
    revisarEnteros(static_cast<uint64_t>(inicial.cuanto("baya")), 3, "inicial", "bayas");
    revisarEnteros(static_cast<uint64_t>(inicial.cuanto("cristal")), 0, "inicial", "cristales");
    revisarEnteros(static_cast<uint64_t>(inicial.total()), 7, "inicial", "total");

    // Consumir descuenta de a una y se planta en cero.
    Inventario i = inicial;
    revisar(i.consumir("baya"), "consumir", "tendría que haber bayas");
    revisarEnteros(static_cast<uint64_t>(i.cuanto("baya")), 2, "consumir", "quedan 2");
    revisar(i.consumir("baya"), "consumir", "tendría que haber bayas");
    revisar(i.consumir("baya"), "consumir", "tendría que haber bayas");
    revisar(!i.consumir("baya"), "consumir", "sin bayas no se puede consumir");
    revisarEnteros(static_cast<uint64_t>(i.cuanto("baya")), 0, "consumir", "no baja de cero");

    // El cristal arranca en cero: es lo raro y tiene que sentirse así.
    revisar(!inicial.hay("cristal"), "cristal", "arranca sin cristales");

    // Un id que no existe no rompe ni inventa comida.
    Inventario j = inicial;
    revisar(!j.consumir("piedra"), "id inexistente", "no debería consumir nada");
    revisarEnteros(static_cast<uint64_t>(j.total()), 7, "id inexistente", "el total no cambió");

    // Y el circuito completo: la copia se descarta si la acción falla. Se simula
    // con una criatura de expedición, que rechaza toda acción.
    const int64_t base = 1786406400000LL;
    CreatureState c = createCreature(0xA3F091C477BE2D08ULL, base, -180);
    c.expedicion = Expedicion{"patio", base, base + 900'000};

    Inventario pendiente = inicial;
    revisar(pendiente.consumir("baya"), "acción rechazada", "la copia se descuenta");
    const ActionResult r = alimentar(c, "baya", base);
    revisar(!r.ok, "acción rechazada", "de expedición no se puede comer");
    // La despensa REAL no se tocó: `pendiente` es una copia que se descarta.
    revisarEnteros(static_cast<uint64_t>(inicial.cuanto("baya")), 3, "acción rechazada",
                   "la despensa original quedó intacta");
}

/**
 * El botín de las expediciones.
 *
 * Se decide cuando la criatura SALE, a partir del genoma y del momento de
 * salida. Eso es lo que impide cerrar y reabrir hasta que salga un cristal, y
 * solo funciona si las dos plataformas sortean exactamente igual.
 *
 * El sorteo recorre los pesos restando hasta cruzar el cero, así que el ORDEN de
 * la tabla decide qué sale en cada tirada. Es la razón por la que en el C++ los
 * pesos van en un vector y no en un map: alfabetizados darían otro botín.
 */
static void probarBotines() {
    bloque("botín de expediciones");

    for (const auto& v : vectores::BOTINES) {
        char ctx[128];
        std::snprintf(ctx, sizeof(ctx), "seed %016llX en %s, salida %lld",
                      static_cast<unsigned long long>(v.seed), v.destinoId,
                      static_cast<long long>(v.salidaMs));

        const Destino* destino = destinoPorId(v.destinoId);
        if (destino == nullptr) {
            revisar(false, ctx, "el destino no existe en el C++");
            continue;
        }

        const Botin b = resolverBotin(v.seed, *destino, v.salidaMs);

        // Los alimentos se comparan en ORDEN, no como conjunto: el orden en que
        // aparecen es el orden en que salieron sorteados, y si eso difiere el
        // sorteo tomó otro camino aunque el total coincida.
        std::string obtenidos;
        for (const auto& [id, cantidad] : b.alimentos.items()) {
            if (cantidad <= 0) continue;
            if (!obtenidos.empty()) obtenidos += ",";
            obtenidos += id + ":" + std::to_string(cantidad);
        }
        const std::string detalle =
            "alimentos: C++ dio \"" + obtenidos + "\", el TS da \"" + v.alimentos + "\"";
        revisar(obtenidos == v.alimentos, ctx, detalle.c_str());

        revisarEnteros(b.semilla.has_value() ? 1 : 0, v.traeSemilla, ctx, "trae semilla");
        if (b.semilla.has_value() && v.traeSemilla != 0) {
            revisarEnteros(*b.semilla, v.semilla, ctx, "el genoma encontrado");
        }

        const std::string frase = describirBotin(b);
        const std::string detalleFrase =
            "frase: C++ dio \"" + frase + "\", el TS da \"" + v.frase + "\"";
        revisar(frase == v.frase, ctx, detalleFrase.c_str());
    }
}

/**
 * Las reglas de salida, y la que evita que el juego se trabe.
 *
 * El patio no pide etapa ni cuesta energía. Es la garantía de que una criatura
 * sin comida y sin energía siempre tenga algo que hacer para conseguir más — sin
 * eso, la economía se cierra sobre sí misma y la partida queda muerta.
 */
static void probarSalidas() {
    bloque("reglas de las expediciones");

    const int64_t base = 1786406400000LL;
    const Destino* patio = destinoPorId("patio");
    const Destino* bosque = destinoPorId("bosque");
    const Destino* ruinas = destinoPorId("ruinas");

    // Una criatura recién nacida, sin energía. El peor caso posible.
    CreatureState bebe = createCreature(0xA3F091C477BE2D08ULL, base, -180);
    bebe.stats.energia = 0.0;

    revisar(puedeSalir(bebe, *patio).puede, "bebé sin energía",
            "el patio TIENE que estar disponible o el juego se traba");
    revisar(!puedeSalir(bebe, *bosque).puede, "bebé", "el bosque pide juvenil");
    revisar(!puedeSalir(bebe, *ruinas).puede, "bebé", "las ruinas piden adulto");

    // Adulta pero agotada: los destinos que cuestan energía se cierran, el patio no.
    CreatureState adulta = bebe;
    adulta.etapa = Stage::Adulto;
    adulta.stats.energia = 10.0;
    revisar(puedeSalir(adulta, *patio).puede, "adulta agotada", "el patio sigue abierto");
    revisar(!puedeSalir(adulta, *bosque).puede, "adulta agotada", "el bosque pide 15 de energía");

    // En letargo no sale a ningún lado.
    CreatureState dormida = adulta;
    dormida.letargico = true;
    dormida.stats.energia = 100.0;
    revisar(!puedeSalir(dormida, *patio).puede, "en letargo", "no puede salir");

    // Enviar cobra la energía y anota la vuelta.
    CreatureState fuerte = adulta;
    fuerte.stats.energia = 80.0;
    const CreatureState enviada = enviar(fuerte, *bosque, base);
    revisarDobles(enviada.stats.energia, 65.0, "enviar", "descuenta el costo");
    revisar(enviada.expedicion.has_value(), "enviar", "queda anotada la expedición");
    revisarEnteros(static_cast<uint64_t>(enviada.expedicion->regresoMs),
                   static_cast<uint64_t>(base + 90 * 60'000), "enviar", "vuelve a los 90 minutos");
    revisar(!puedeSalir(enviada, *patio).puede, "ya afuera", "no puede salir dos veces");

    // Y no vuelve antes de tiempo.
    revisar(!recibir(enviada, base + 89 * 60'000).volvio, "a los 89 minutos", "todavía no volvió");
    const Regreso r = recibir(enviada, base + 90 * 60'000);
    revisar(r.volvio, "a los 90 minutos", "ya tendría que estar de vuelta");
    revisar(!r.criatura.expedicion.has_value(), "al volver", "deja de estar de expedición");
    // Volver cuenta como atención: estuvo trabajando, no abandonada.
    revisarEnteros(static_cast<uint64_t>(r.criatura.ticksSinCuidado), 0, "al volver",
                   "se reinicia el contador de abandono");

    // El patio SIEMPRE trae algo. Es la promesa que sostiene toda la economía.
    for (int64_t salida = 0; salida < 40; ++salida) {
        const Botin b = resolverBotin(0xA3F091C477BE2D08ULL, *patio, base + salida * 1000);
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "patio, salida %lld", static_cast<long long>(salida));
        revisar(b.alimentos.total() >= 1, ctx, "el patio nunca puede volver vacío");
    }
}

/**
 * El atlas de tiles del mundo.
 *
 * ATENCIÓN: esto NO es un test de paridad, y la diferencia importa.
 *
 * Todo lo demás en este archivo compara contra el TypeScript, que dice cuál es
 * la respuesta correcta. El mundo navegable existe solo del lado nativo, así que
 * acá no hay referencia: se comprueban PROPIEDADES —que los tiles sean opacos,
 * que se distingan entre sí, que el atlas sea determinista— en vez de igualdad.
 *
 * Es una red más floja. Vale tenerlo presente: un tile feo pasa estos tests sin
 * problema, y solo se ve mirándolo.
 */
static void probarAtlas() {
    bloque("atlas de tiles (sin paridad — solo propiedades)");

    const Atlas a = generarAtlas();

    revisarEnteros(static_cast<uint64_t>(a.width),
                   static_cast<uint64_t>(COLUMNAS_ATLAS * TILE), "atlas", "ancho");
    revisarEnteros(static_cast<uint64_t>(a.height),
                   static_cast<uint64_t>(FILAS_ATLAS * TILE), "atlas", "alto");
    revisarEnteros(a.data.size(), static_cast<uint64_t>(a.width) * a.height * 4, "atlas",
                   "bytes");

    // Un píxel del atlas, por fila y columna de la grilla.
    auto alfa = [&](int col, int fila, int x, int y) {
        const size_t i = (static_cast<size_t>(fila * TILE + y) * a.width +
                          static_cast<size_t>(col * TILE + x)) * 4 + 3;
        return a.data[i];
    };

    // -- la máscara cero es un agujero, y tiene que serlo ------------------
    //
    // Es lo que deja ver la capa de abajo. Si tuviera un solo píxel opaco, cada
    // celda sin esta capa taparía al agua o al pasto de abajo con un cuadrado, y
    // el mundo entero volvería a ser una grilla.
    for (int c = 0; c < static_cast<int>(Capa::CANTIDAD); ++c) {
        bool vacia = true;
        for (int y = 0; y < TILE && vacia; ++y) {
            for (int x = 0; x < TILE; ++x) {
                if (alfa(0, c, x, y) != 0) { vacia = false; break; }
            }
        }
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "capa %d", c);
        revisar(vacia, ctx, "la máscara cero tiene tinta: taparía la capa de abajo");
    }

    // -- y la quince es maciza ---------------------------------------------
    //
    // Un tile con las cuatro esquinas presentes está en el interior del material,
    // lejos de cualquier borde. Un agujero ahí se ve como un punto del color
    // equivocado en medio de un campo, y es de las cosas más difíciles de
    // rastrear mirando: parece suciedad del render.
    for (int c = 0; c < static_cast<int>(Capa::CANTIDAD); ++c) {
        int huecos = 0;
        for (int y = 0; y < TILE; ++y) {
            for (int x = 0; x < TILE; ++x) {
                if (alfa(15, c, x, y) != 255) ++huecos;
            }
        }
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "capa %d", c);
        char detalle[96];
        std::snprintf(detalle, sizeof(detalle), "la máscara llena tiene %d píxeles no opacos",
                      huecos);
        revisar(huecos == 0, ctx, detalle);
    }

    // -- cuánto cubre cada máscara crece con la cantidad de esquinas --------
    //
    // Es la propiedad que dice que la cobertura bilineal está bien: una máscara
    // con tres esquinas tiene que pintar más que una con dos, y esa más que una
    // con una. Si estuviera al revés —o fuera igual— los bordes irían para el
    // lado contrario y el mundo se vería como un negativo.
    for (int c = 0; c < static_cast<int>(Capa::CANTIDAD); ++c) {
        int cubierto[16] = {};
        for (int m = 0; m < 16; ++m) {
            for (int y = 0; y < TILE; ++y) {
                for (int x = 0; x < TILE; ++x) {
                    if (alfa(m, c, x, y) != 0) ++cubierto[m];
                }
            }
        }

        for (int m = 0; m < 16; ++m) {
            const int esquinas = ((m & 1) ? 1 : 0) + ((m & 2) ? 1 : 0) + ((m & 4) ? 1 : 0) +
                                 ((m & 8) ? 1 : 0);
            for (int n = 0; n < 16; ++n) {
                const int otras = ((n & 1) ? 1 : 0) + ((n & 2) ? 1 : 0) + ((n & 4) ? 1 : 0) +
                                  ((n & 8) ? 1 : 0);
                if (esquinas >= otras) continue;

                char ctx[80];
                std::snprintf(ctx, sizeof(ctx), "capa %d, máscaras %d y %d", c, m, n);
                char detalle[128];
                std::snprintf(detalle, sizeof(detalle),
                              "%d esquinas cubren %d px y %d esquinas cubren %d", esquinas,
                              cubierto[m], otras, cubierto[n]);
                revisar(cubierto[m] <= cubierto[n], ctx, detalle);
            }
        }
    }

    // -- las capas se distinguen entre sí -----------------------------------
    //
    // Con la máscara llena, que es donde se ve el color del material sin bordes
    // de por medio. Dos capas del mismo color son dos capas que no sirven.
    auto promedio = [&](int col, int fila) {
        long r = 0, g = 0, b = 0;
        for (int y = 0; y < TILE; ++y) {
            for (int x = 0; x < TILE; ++x) {
                const size_t i = (static_cast<size_t>(fila * TILE + y) * a.width +
                                  static_cast<size_t>(col * TILE + x)) * 4;
                r += a.data[i];
                g += a.data[i + 1];
                b += a.data[i + 2];
            }
        }
        const long n = TILE * TILE;
        return std::array<long, 3>{r / n, g / n, b / n};
    };

    for (int c = 0; c < static_cast<int>(Capa::CANTIDAD); ++c) {
        for (int d = c + 1; d < static_cast<int>(Capa::CANTIDAD); ++d) {
            const auto pc = promedio(15, c);
            const auto pd = promedio(15, d);
            const long dist = std::labs(pc[0] - pd[0]) + std::labs(pc[1] - pd[1]) +
                              std::labs(pc[2] - pd[2]);
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "capas %d y %d", c, d);
            char detalle[96];
            std::snprintf(detalle, sizeof(detalle), "se parecen demasiado (distancia %ld)", dist);
            revisar(dist >= 12, ctx, detalle);
        }
    }

    // -- los objetos no son cuadrados ---------------------------------------
    //
    // Un árbol tiene que tener transparencia en las esquinas: si fuera un tile
    // macizo, un bosque volvería a ser una mancha rectangular y no habríamos
    // ganado nada.
    for (int v = 0; v < VARIANTES_ARBOL + VARIANTES_PIEDRA; ++v) {
        int opacos = 0;
        for (int y = 0; y < TILE; ++y) {
            for (int x = 0; x < TILE; ++x) {
                if (alfa(v, FILA_OBJETOS, x, y) != 0) ++opacos;
            }
        }
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "objeto %d", v);
        revisar(opacos > 40, ctx, "el objeto casi no tiene dibujo");
        revisar(opacos < TILE * TILE - 20, ctx, "el objeto es un cuadrado macizo");
        // Las cuatro esquinas del tile, libres: es donde se ve el suelo de abajo.
        revisar(alfa(v, FILA_OBJETOS, 0, 0) == 0, ctx, "la esquina del objeto está pintada");
    }

    // Y las variantes de árbol tienen que ser distintas entre sí: con una sola,
    // un bosque se lee como un sello repetido.
    for (int i = 0; i < VARIANTES_ARBOL; ++i) {
        for (int j = i + 1; j < VARIANTES_ARBOL; ++j) {
            int distintos = 0;
            for (int y = 0; y < TILE; ++y) {
                for (int x = 0; x < TILE; ++x) {
                    if (alfa(i, FILA_OBJETOS, x, y) != alfa(j, FILA_OBJETOS, x, y)) ++distintos;
                }
            }
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "árboles %d y %d", i, j);
            revisar(distintos > 8, ctx, "las dos variantes son casi el mismo dibujo");
        }
    }

    // -- determinista --------------------------------------------------------
    const Atlas otro = generarAtlas();
    revisar(a.data == otro.data, "atlas", "dos llamadas dieron atlas distintos");

    // -- la solidez, que no cambió -------------------------------------------
    revisar(esSolido(Tile::Agua), "solidez", "el agua tendría que frenar");
    revisar(esSolido(Tile::Piedra), "solidez", "la piedra tendría que frenar");
    revisar(esSolido(Tile::Arbol), "solidez", "los árboles tendrían que frenar");
    revisar(!esSolido(Tile::Pasto), "solidez", "por el pasto se camina");
    revisar(!esSolido(Tile::Camino), "solidez", "por el camino se camina");
    revisar(!esSolido(Tile::PastoAlto), "solidez", "por el pasto alto se camina");
    revisar(!esSolido(Tile::Arena), "solidez", "por la arena se camina");
    revisar(!esSolido(Tile::Musgo), "solidez", "por el musgo se camina");
}


/**
 * La máscara de la grilla dual.
 *
 * Es una función de cuatro tiles y una capa, así que se prueba sola, sin mundo y
 * sin atlas. Lo que se comprueba son las tres cosas de las que depende que los
 * bordes cierren.
 */
static void probarGrillaDual() {
    bloque("grilla dual");

    static const Tile TODOS[] = {Tile::Pasto,  Tile::Camino,    Tile::Agua,  Tile::Piedra,
                                 Tile::Arbol,  Tile::PastoAlto, Tile::Arena, Tile::Musgo};

    // -- siempre en rango ---------------------------------------------------
    for (Tile a : TODOS) {
        for (Tile b : TODOS) {
            for (Tile c : TODOS) {
                for (Tile d : TODOS) {
                    for (int k = 0; k < static_cast<int>(Capa::CANTIDAD); ++k) {
                        const int m = mascaraDeEsquinas(a, b, c, d, static_cast<Capa>(k));
                        revisar(m >= 0 && m < 16, "rango", "la máscara se fue de [0, 16)");
                    }
                }
            }
        }
    }

    // -- una región uniforme da la máscara llena, y solo esa ----------------
    //
    // Es la que atrapa un desfase: si las esquinas se leyeran corridas un tile,
    // el interior de un campo daría máscaras parciales y el mundo entero sería
    // bordes.
    for (Tile t : TODOS) {
        const Capa suya = capaDeSuelo(t);
        for (int k = 0; k <= static_cast<int>(suya); ++k) {
            const int m = mascaraDeEsquinas(t, t, t, t, static_cast<Capa>(k));
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "tile %d en capa %d", static_cast<int>(t), k);
            revisarEnteros(static_cast<uint64_t>(m), 15, ctx, "máscara de una región uniforme");
        }
        // Y en una capa POR ENCIMA de la suya, cero: ese material no está ahí.
        for (int k = static_cast<int>(suya) + 1; k < static_cast<int>(Capa::CANTIDAD); ++k) {
            const int m = mascaraDeEsquinas(t, t, t, t, static_cast<Capa>(k));
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "tile %d en capa %d", static_cast<int>(t), k);
            revisarEnteros(static_cast<uint64_t>(m), 0, ctx, "máscara de una capa que no está");
        }
    }

    // -- cada bit corresponde a su esquina ----------------------------------
    //
    // Con tres esquinas de agua y una de pasto, el bit que se apaga en la capa
    // del pasto tiene que ser el de esa esquina. Si estuvieran cruzados, las
    // costas saldrían reflejadas y nadie se daría cuenta mirando un lago
    // redondo.
    {
        const Tile A = Tile::Agua;
        const Tile P = Tile::Pasto;
        revisarEnteros(static_cast<uint64_t>(mascaraDeEsquinas(P, A, A, A, Capa::Pasto)), 1,
                       "esquinas", "solo arriba-izquierda");
        revisarEnteros(static_cast<uint64_t>(mascaraDeEsquinas(A, P, A, A, Capa::Pasto)), 2,
                       "esquinas", "solo arriba-derecha");
        revisarEnteros(static_cast<uint64_t>(mascaraDeEsquinas(A, A, P, A, Capa::Pasto)), 4,
                       "esquinas", "solo abajo-izquierda");
        revisarEnteros(static_cast<uint64_t>(mascaraDeEsquinas(A, A, A, P, Capa::Pasto)), 8,
                       "esquinas", "solo abajo-derecha");
    }

    // -- las capas se apilan, no se excluyen --------------------------------
    //
    // Un tile de camino cuenta como presente para el camino Y para el pasto y el
    // agua de abajo. Sin eso, un sendero sobre pasto le haría un agujero al
    // pasto y se vería el agua a través del piso.
    for (Tile t : TODOS) {
        const int suya = static_cast<int>(capaDeSuelo(t));
        for (int k = 0; k <= suya; ++k) {
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "tile %d", static_cast<int>(t));
            revisar(perteneceA(t, static_cast<Capa>(k)), ctx,
                    "no cuenta para una capa de más abajo que la suya");
        }
    }

    // El agua es el fondo del mundo: todo cuenta para ella, así que esa capa se
    // dibuja siempre llena y nunca deja ver el vacío de atrás.
    for (Tile t : TODOS) {
        revisar(perteneceA(t, Capa::Agua), "fondo", "hay un tile que no cuenta para el agua");
    }
}


/**
 * La tipografía. TAMPOCO tiene paridad — se comprueban propiedades.
 *
 * Igual que con los tiles no hay un TypeScript que diga cuál es la respuesta
 * correcta, así que estos tests no pueden decir si la fuente es LINDA. Lo que sí
 * pueden decir, y es la mitad del trabajo, es si está completa y si es
 * coherente: que no falte ningún carácter que el juego imprima, que no haya dos
 * letras dibujadas igual, y que ninguna se salga de la caja.
 *
 * El de cobertura es el que importa. Una fuente a la que le falta la "n con
 * tilde" no falla ruidosamente: dibuja un cuadrado vacío, y eso aparece en
 * producción, no acá.
 */
static void probarFuente() {
    bloque("tipografía (sin paridad — solo propiedades)");

    const std::vector<Glifo>& glifos = fuenteGlifos();

    // -- cobertura ----------------------------------------------------------
    // Todo el ASCII imprimible, más lo que el castellano necesita, más los
    // cuatro símbolos que la interfaz usa de verdad. Esta lista es un contrato:
    // si alguien escribe un mensaje con un carácter que no está acá, el test no
    // se entera — pero al menos garantiza el piso.
    static const char32_t REQUERIDOS[] = {
        U'á', U'é', U'í', U'ó', U'ú', U'ü', U'ñ',
        U'Á', U'É', U'Í', U'Ó', U'Ú', U'Ü', U'Ñ', U'Â',
        U'¿', U'¡', U'·', U'—', U'→', U'◆',
    };

    for (char32_t c = U' '; c <= U'~'; ++c) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "ASCII U+%04X", static_cast<unsigned>(c));
        int x = 0, y = 0;
        revisar(fuenteUbicacion(c, x, y), ctx, "falta el glifo");
    }
    for (char32_t c : REQUERIDOS) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "U+%04X", static_cast<unsigned>(c));
        int x = 0, y = 0;
        revisar(fuenteUbicacion(c, x, y), ctx, "falta el glifo");
    }

    // -- el espacio es el único vacío ---------------------------------------
    for (const Glifo& g : glifos) {
        if (g.codigo == U' ') {
            revisar(g.vacio(), "espacio", "el espacio tendría que estar en blanco");
            continue;
        }
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "U+%04X", static_cast<unsigned>(g.codigo));
        revisar(!g.vacio(), ctx, "el glifo está en blanco");
    }

    // -- nada se sale de la caja --------------------------------------------
    // Cada fila usa cinco bits. Un bit de más significa tinta pisando el glifo
    // de al lado en el atlas, que en pantalla se ve como una letra con basura.
    const uint8_t mascara = static_cast<uint8_t>((1u << Fuente::ANCHO) - 1u);
    for (const Glifo& g : glifos) {
        for (int f = 0; f < Fuente::ALTO; ++f) {
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "U+%04X fila %d", static_cast<unsigned>(g.codigo), f);
            revisar((g.filas[f] & ~mascara) == 0, ctx, "hay tinta fuera de las cinco columnas");
        }
    }

    // -- no hay dos glifos iguales ------------------------------------------
    // Este es el que caza los errores de copiar y pegar. Una fuente escrita a
    // mano tiene noventa entradas casi iguales; que la O y el 0 salgan
    // idénticos es el accidente más fácil de cometer y el más difícil de ver.
    bool repetidos = false;
    for (size_t i = 0; i < glifos.size(); ++i) {
        if (glifos[i].codigo == U' ') continue;
        for (size_t j = i + 1; j < glifos.size(); ++j) {
            if (glifos[j].codigo == U' ') continue;
            bool iguales = true;
            for (int f = 0; f < Fuente::ALTO && iguales; ++f) {
                if (glifos[i].filas[f] != glifos[j].filas[f]) iguales = false;
            }
            if (iguales) {
                char ctx[80];
                std::snprintf(ctx, sizeof(ctx), "U+%04X y U+%04X",
                              static_cast<unsigned>(glifos[i].codigo),
                              static_cast<unsigned>(glifos[j].codigo));
                revisar(false, ctx, "dos glifos dibujados igual");
                repetidos = true;
            }
        }
    }
    revisar(!repetidos, "glifos", "hay glifos repetidos");

    // -- las acentuadas son su base más algo --------------------------------
    // Si acentuar() no hiciera nada, todo lo de arriba pasaría igual: la "a con
    // tilde" sería una "a" y nadie se enteraría hasta ver la pantalla. Esto
    // comprueba que la marca está, que está ARRIBA del cuerpo, y que no borró
    // nada.
    struct Par { char32_t acentuada; char32_t base; };
    static const Par PARES[] = {
        {U'á', U'a'}, {U'é', U'e'}, {U'ó', U'o'}, {U'ú', U'u'},
        {U'ñ', U'n'}, {U'ü', U'u'},
        {U'Á', U'A'}, {U'É', U'E'}, {U'Ó', U'O'}, {U'Ú', U'U'}, {U'Ñ', U'N'},
    };
    for (const Par& par : PARES) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "U+%04X", static_cast<unsigned>(par.acentuada));

        const Glifo* con = nullptr;
        const Glifo* sin = nullptr;
        for (const Glifo& g : glifos) {
            if (g.codigo == par.acentuada) con = &g;
            if (g.codigo == par.base) sin = &g;
        }
        if (con == nullptr || sin == nullptr) {
            revisar(false, ctx, "falta la acentuada o su base");
            continue;
        }

        bool distinta = false;
        bool cuerpoIntacto = true;
        int primeraBase = Fuente::ALTO;
        for (int f = 0; f < Fuente::ALTO; ++f) {
            if (con->filas[f] != sin->filas[f]) distinta = true;
            // El cuerpo no se toca: donde la base tiene tinta, la acentuada
            // también.
            if ((sin->filas[f] & con->filas[f]) != sin->filas[f]) cuerpoIntacto = false;
            if (sin->filas[f] != 0 && primeraBase == Fuente::ALTO) primeraBase = f;
        }
        // Y la marca tiene que estar por encima de donde empieza la base.
        bool marcaArriba = false;
        for (int f = 0; f < primeraBase; ++f) {
            if (con->filas[f] != 0) marcaArriba = true;
        }
        revisar(distinta, ctx, "salió igual que su base: la marca no se aplicó");
        revisar(cuerpoIntacto, ctx, "la marca le comió tinta al cuerpo");
        revisar(marcaArriba, ctx, "la marca no quedó por encima del cuerpo");
    }

    // -- la i con tilde no conserva su punto --------------------------------
    // El error tipográfico clásico: poner la tilde arriba y dejarle el punto
    // abajo. Se ve mal y se ve enseguida.
    {
        const Glifo* punto = nullptr;
        const Glifo* tildada = nullptr;
        for (const Glifo& g : glifos) {
            if (g.codigo == U'i') punto = &g;
            if (g.codigo == U'í') tildada = &g;
        }
        revisar(punto != nullptr && tildada != nullptr, "i con tilde", "faltan los glifos");
        if (punto != nullptr && tildada != nullptr) {
            int conTinta = 0;
            for (int f = 0; f < Fuente::ALTO; ++f) {
                if (tildada->filas[f] != 0) ++conTinta;
            }
            // Cuerpo (5) + marca (2) = 7. Si conservara el punto habría 8 o más.
            revisar(conTinta <= 7, "i con tilde", "parece conservar el punto de la i");
        }
    }

    // -- los signos de apertura son sus pares dados vuelta -------------------
    struct Vuelta { char32_t abre; char32_t cierra; };
    static const Vuelta VUELTAS[] = {{U'¿', U'?'}, {U'¡', U'!'}};
    for (const Vuelta& v : VUELTAS) {
        const Glifo* abre = nullptr;
        const Glifo* cierra = nullptr;
        for (const Glifo& g : glifos) {
            if (g.codigo == v.abre) abre = &g;
            if (g.codigo == v.cierra) cierra = &g;
        }
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "U+%04X", static_cast<unsigned>(v.abre));
        revisar(abre != nullptr && cierra != nullptr, ctx, "faltan los glifos");
        if (abre == nullptr || cierra == nullptr) continue;

        bool ok = true;
        for (int f = 0; f < Fuente::ALTO && ok; ++f) {
            uint8_t esperado = 0;
            const uint8_t origen = cierra->filas[Fuente::ALTO - 1 - f];
            for (int b = 0; b < Fuente::ANCHO; ++b) {
                if (origen & (1u << b)) {
                    esperado |= static_cast<uint8_t>(1u << (Fuente::ANCHO - 1 - b));
                }
            }
            if (abre->filas[f] != esperado) ok = false;
        }
        revisar(ok, ctx, "no es el signo de cierre rotado 180 grados");
    }

    // -- el atlas ------------------------------------------------------------
    const ImagenFuente img = fuenteAtlas();
    revisarEnteros(static_cast<uint64_t>(img.ancho),
                   static_cast<uint64_t>(Fuente::COLUMNAS * Fuente::ANCHO), "atlas de fuente",
                   "ancho");
    revisarEnteros(static_cast<uint64_t>(img.alto),
                   static_cast<uint64_t>(fuenteFilasAtlas() * Fuente::ALTO), "atlas de fuente",
                   "alto");
    revisarEnteros(img.rgba.size(), static_cast<uint64_t>(img.ancho) * img.alto * 4,
                   "atlas de fuente", "bytes");

    // Tinta blanca o transparencia total: nada en el medio. Un píxel a media
    // opacidad delataría antialiasing, que es justo lo que esta fuente no tiene.
    bool limpia = true;
    for (size_t i = 0; i + 3 < img.rgba.size() && limpia; i += 4) {
        const uint8_t a = img.rgba[i + 3];
        if (a != 0 && a != 255) limpia = false;
        if (a == 255 && (img.rgba[i] != 255 || img.rgba[i + 1] != 255 || img.rgba[i + 2] != 255)) {
            limpia = false;
        }
    }
    revisar(limpia, "atlas de fuente", "hay tinta que no es blanca opaca");

    // -- determinista --------------------------------------------------------
    const ImagenFuente otra = fuenteAtlas();
    revisar(img.rgba == otra.rgba, "atlas de fuente", "dos llamadas dieron atlas distintos");

    // -- y cada glifo está donde dice estar ----------------------------------
    // Une las dos mitades: fuenteUbicacion() le dice a GDScript de dónde
    // recortar, y si eso no coincide con lo que se pintó, cada letra sale con un
    // pedazo de la de al lado.
    for (const Glifo& g : glifos) {
        if (g.vacio()) continue;
        int x = 0, y = 0;
        if (!fuenteUbicacion(g.codigo, x, y)) continue;

        bool coincide = true;
        for (int f = 0; f < Fuente::ALTO && coincide; ++f) {
            for (int c = 0; c < Fuente::ANCHO; ++c) {
                const bool hayTinta = (g.filas[f] & (1u << (Fuente::ANCHO - 1 - c))) != 0;
                const size_t p = (static_cast<size_t>(y + f) * img.ancho + (x + c)) * 4 + 3;
                if (hayTinta != (img.rgba[p] == 255)) {
                    coincide = false;
                    break;
                }
            }
        }
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "U+%04X", static_cast<unsigned>(g.codigo));
        revisar(coincide, ctx, "el recorte del atlas no coincide con el glifo");
    }
}

/**
 * La cruza: dos genomas dan uno nuevo.
 *
 * Se comparan las cuatro salidas y no solo el hijo, porque cada una atrapa un
 * error distinto:
 *
 *   - el `seed` atrapa un PRNG mal sembrado o campos en otro orden;
 *   - la cadena de herencia atrapa que un gen venga del padre equivocado con el
 *     hijo saliendo igual de casualidad;
 *   - `mutaciones` atrapa un bucle que corta antes de los 64 bits;
 *   - la frase atrapa los umbrales de `describirHerencia`.
 *
 * Y además se comprueba la SIMETRÍA, que no sale de los vectores: cruzar A con B
 * tiene que dar exactamente lo mismo que cruzar B con A. Es una propiedad del
 * algoritmo —por eso ordena los seeds antes de sembrar— y si se rompiera, el
 * hijo dependería de en qué orden el jugador tocó las dos criaturas.
 */
static void probarCruzas() {
    bloque("cruza");

    for (const vectores::VectorCruza& v : vectores::CRUZAS) {
        char ctx[128];
        std::snprintf(ctx, sizeof(ctx), "cruza(%016llx, %016llx, %lld)",
                      static_cast<unsigned long long>(v.a),
                      static_cast<unsigned long long>(v.b),
                      static_cast<long long>(v.nonce));

        const Cruza c = cruzar(v.a, v.b, v.nonce);

        revisarEnteros(c.seed, v.hijo, ctx, "hijo");
        revisarEnteros(static_cast<uint64_t>(c.mutaciones),
                       static_cast<uint64_t>(v.mutaciones), ctx, "mutaciones");

        // Un carácter por campo: A, B o M. Comparar la cadena entera dice de una
        // cuál campo salió del padre equivocado, en vez de "el hijo no coincide".
        std::string herencia;
        for (Origen o : c.herencia) {
            herencia += (o == Origen::A) ? 'A' : (o == Origen::B) ? 'B' : 'M';
        }
        const std::string detalleH =
            "herencia: C++ dio \"" + herencia + "\", el TS da \"" + v.herencia + "\"";
        revisar(herencia == v.herencia, ctx, detalleH.c_str());

        const std::string frase = describirHerencia(c);
        const std::string detalleF =
            "frase: C++ dio \"" + frase + "\", el TS da \"" + v.descripcion + "\"";
        revisar(frase == v.descripcion, ctx, detalleF.c_str());

        // La simetría. No viene de los vectores: es una propiedad.
        const Cruza alReves = cruzar(v.b, v.a, v.nonce);
        revisarEnteros(alReves.seed, c.seed, ctx, "hijo al cruzar en el otro orden");
        revisarEnteros(static_cast<uint64_t>(alReves.mutaciones),
                       static_cast<uint64_t>(c.mutaciones), ctx,
                       "mutaciones al cruzar en el otro orden");
    }

    // Los catorce campos tienen que cubrir los 64 bits sin huecos ni solapes. Si
    // no, un bit mutado no encontraría campo al que culpar y la cadena de
    // herencia quedaría distinta de la del TS sin que el hijo cambie.
    int cubiertos = 0;
    for (int bit = 0; bit < 64; ++bit) {
        int cuantos = 0;
        for (const CampoGenoma& campo : camposGenoma()) {
            if (bit >= campo.offset && bit < campo.offset + campo.bits) ++cuantos;
        }
        if (cuantos == 1) ++cubiertos;
    }
    revisarEnteros(static_cast<uint64_t>(cubiertos), 64, "layout",
                   "bits cubiertos por exactamente un campo");

    // -- elegibilidad ------------------------------------------------------
    //
    // El orden de los chequeos es lo que se prueba acá, no cada caso por
    // separado: solo se informa el primer motivo, así que una criatura en
    // letargo Y con poca salud tiene que decir lo del letargo. Invertir dos
    // chequeos no cambia quién puede cruzar, cambia lo que se le explica al
    // jugador — y eso no lo atrapa ningún vector.
    const int64_t base = 1786406400000LL;
    CreatureState adulta = createCreature(0xA3F091C477BE2D08ULL, base, -180);
    adulta.etapa = Stage::Adulto;
    adulta.stats.salud = 90.0;
    adulta.stats.vinculo = 40.0;

    revisar(elegibilidad(adulta, base).puede, "elegibilidad", "una adulta sana no puede cruzar");

    {
        CreatureState bebe = adulta;
        bebe.etapa = Stage::Bebe;
        bebe.letargico = true;
        bebe.stats.salud = 10.0;
        const Elegibilidad e = elegibilidad(bebe, base);
        revisar(!e.puede, "elegibilidad", "una bebé no tendría que poder cruzar");
        revisar(e.motivo == "Todavía no terminó de crecer.", "elegibilidad",
                ("la etapa manda sobre el resto; dio: " + e.motivo).c_str());
    }
    {
        CreatureState dormida = adulta;
        dormida.letargico = true;
        dormida.stats.salud = 10.0;
        const Elegibilidad e = elegibilidad(dormida, base);
        revisar(e.motivo == "Está en letargo. Primero hay que reconectar con ella.",
                "elegibilidad", ("el letargo manda sobre la salud; dio: " + e.motivo).c_str());
    }
    {
        CreatureState floja = adulta;
        floja.stats.salud = 10.0;
        floja.stats.vinculo = 0.0;
        const Elegibilidad e = elegibilidad(floja, base);
        revisar(e.motivo == "No está lo bastante sana.", "elegibilidad",
                ("la salud manda sobre el vínculo; dio: " + e.motivo).c_str());
    }
    {
        CreatureState distante = adulta;
        distante.stats.vinculo = 5.0;
        const Elegibilidad e = elegibilidad(distante, base);
        revisar(e.motivo == "Todavía no hay suficiente vínculo con ella.", "elegibilidad",
                ("faltaba el vínculo; dio: " + e.motivo).c_str());
    }

    // -- el cooldown, redondeado hacia arriba -------------------------------
    //
    // Con división entera, la última hora entera diría "faltan 0 h", que es
    // mentirle al jugador. El TS usa Math.ceil y por eso acá va std::ceil sobre
    // un double: los tres casos de abajo son justamente los que distinguen las
    // dos implementaciones.
    struct CasoCooldown { int64_t pasadoMs; const char* motivo; };
    const CasoCooldown CASOS[] = {
        {0, "Necesita descansar. Faltan 8 h."},
        {CRUZA_COOLDOWN_MS - 1, "Necesita descansar. Faltan 1 h."},
        {CRUZA_COOLDOWN_MS - 3600000, "Necesita descansar. Faltan 1 h."},
        {CRUZA_COOLDOWN_MS - 3600001, "Necesita descansar. Faltan 2 h."},
    };
    for (const CasoCooldown& caso : CASOS) {
        CreatureState cruzada = marcarCruzada(adulta, base);
        const Elegibilidad e = elegibilidad(cruzada, base + caso.pasadoMs);
        char ctxc[96];
        std::snprintf(ctxc, sizeof(ctxc), "cooldown a los %lld ms",
                      static_cast<long long>(caso.pasadoMs));
        revisar(!e.puede, ctxc, "tendría que estar en cooldown");
        revisar(e.motivo == caso.motivo, ctxc,
                ("dio: \"" + e.motivo + "\", se esperaba: \"" + caso.motivo + "\"").c_str());
    }

    // Justo al cumplirse el cooldown ya puede: el TS compara con < y no con <=.
    {
        CreatureState cruzada = marcarCruzada(adulta, base);
        revisar(elegibilidad(cruzada, base + CRUZA_COOLDOWN_MS).puede, "cooldown",
                "al cumplirse las ocho horas tendría que poder cruzar");
    }

    // marcarCruzada no toca la original.
    revisar(!adulta.ultimaCruzaMs.has_value(), "marcarCruzada",
            "le escribió encima a la criatura original");

    // -- el par ------------------------------------------------------------
    {
        CreatureState otra = adulta;
        otra.id = "otra";
        revisar(puedenCruzar(adulta, otra, base).puede, "puedenCruzar",
                "dos adultas sanas y distintas tendrían que poder");

        const Elegibilidad sola = puedenCruzar(adulta, adulta, base);
        revisar(!sola.puede, "puedenCruzar", "no puede cruzar consigo misma");
        revisar(sola.motivo == "Hace falta otra criatura.", "puedenCruzar",
                ("dio: " + sola.motivo).c_str());

        // El motivo tiene que ser el de la que falla, no uno genérico: es lo que
        // le dice al jugador a cuál de las dos tiene que cuidar.
        CreatureState floja = otra;
        floja.stats.salud = 10.0;
        const Elegibilidad e = puedenCruzar(adulta, floja, base);
        revisar(e.motivo == "No está lo bastante sana.", "puedenCruzar",
                ("no informó el motivo de la que falla; dio: " + e.motivo).c_str());
    }
}

/**
 * El codex, registrando criaturas una atrás de otra.
 *
 * Lo que se compara no es una llamada suelta sino el estado ACUMULADO: los tres
 * ordenamientos —linajes por número, formas por su id en texto, rarezas por id—
 * solo se pueden equivocar cuando hay más de un elemento, y una lista de uno
 * está ordenada de cualquier manera.
 *
 * El orden de las formas es el que más fácil se rompe: el enum va Indefinida,
 * Petreo, Vaporoso, Coloso… y alfabéticamente el primero es "coloso". Ordenar
 * por el valor del enum produce un archivo distinto del que escribe la web, y
 * ningún validador se quejaría — simplemente dejarían de coincidir.
 */
static void probarCodex() {
    bloque("codex");

    // Los mismos seeds y formas que usó el generador, en el mismo orden. Si esta
    // lista se desincroniza del TS, los vectores dejan de tener sentido: por eso
    // la primera comprobación de cada paso es `totalRegistradas`, que se rompe
    // enseguida si las dos secuencias no van a la par.
    static const Form FORMAS_CODEX[] = {
        Form::Indefinida, Form::Coloso,   Form::Petreo,     Form::Oraculo, Form::Vaporoso,
        Form::Guardian,   Form::Errante,  Form::Coloso,     Form::Indefinida, Form::Petreo,
    };
    const size_t CANT_FORMAS = sizeof(FORMAS_CODEX) / sizeof(FORMAS_CODEX[0]);

    // El generador usa `[...BORDES.slice(0, 6), ...seeds.slice(0, 30)]`, y
    // `generarSeeds` arranca justamente con los BORDES: por eso los primeros
    // seis genomas del vector SON los seis primeros bordes, y alcanza con
    // recorrer GENOMAS dos veces en vez de repetir la lista de bordes acá.
    std::vector<Seed> semillas;
    for (size_t i = 0; i < 6; ++i) semillas.push_back(vectores::GENOMAS[i].seed);
    for (size_t i = 0; i < 30; ++i) semillas.push_back(vectores::GENOMAS[i].seed);

    Codex codex;
    size_t paso = 0;

    // Une un vector de textos con comas, como el join() del TS.
    auto unir = [](const std::vector<std::string>& xs) {
        std::string salida;
        for (size_t i = 0; i < xs.size(); ++i) {
            if (i > 0) salida += ",";
            salida += xs[i];
        }
        return salida;
    };

    auto comparar = [&](const Registro& r, int registradas) {
        if (paso >= sizeof(vectores::CODEXS) / sizeof(vectores::CODEXS[0])) return;
        const vectores::VectorCodex& v = vectores::CODEXS[paso++];

        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "paso %d", registradas);

        revisarEnteros(static_cast<uint64_t>(registradas),
                       static_cast<uint64_t>(v.registradas), ctx, "criaturas registradas");
        revisarEnteros(static_cast<uint64_t>(r.codex.totalRegistradas),
                       static_cast<uint64_t>(v.totalRegistradas), ctx, "totalRegistradas");

        std::vector<std::string> linajes;
        for (int l : r.codex.linajes) linajes.push_back(std::to_string(l));
        const std::string sl = unir(linajes);
        revisar(sl == v.linajes, ctx,
                ("linajes: C++ dio \"" + sl + "\", el TS da \"" + v.linajes + "\"").c_str());

        std::vector<std::string> formas;
        for (Form f : r.codex.formas) formas.push_back(std::string(formId(f)));
        const std::string sf = unir(formas);
        revisar(sf == v.formas, ctx,
                ("formas: C++ dio \"" + sf + "\", el TS da \"" + v.formas + "\"").c_str());

        const std::string sr = unir(r.codex.rarezas);
        revisar(sr == v.rarezas, ctx,
                ("rarezas: C++ dio \"" + sr + "\", el TS da \"" + v.rarezas + "\"").c_str());

        revisarEnteros(static_cast<uint64_t>(r.nuevos.size()),
                       static_cast<uint64_t>(v.nuevos), ctx, "descubrimientos nuevos");

        std::vector<std::string> detalles;
        for (const Descubrimiento& d : r.nuevos) {
            detalles.push_back(std::string(nombreTipo(d.tipo)) + ":" + d.id);
        }
        const std::string sd = unir(detalles);
        revisar(sd == v.detalleNuevos, ctx,
                ("novedades: C++ dio \"" + sd + "\", el TS da \"" + v.detalleNuevos + "\"")
                    .c_str());

        const ProgresoCodex p = progresoCodex(r.codex);
        revisarEnteros(static_cast<uint64_t>(p.linajes.vistos),
                       static_cast<uint64_t>(v.vistosLinajes), ctx, "progreso: linajes vistos");
        revisarEnteros(static_cast<uint64_t>(p.formas.vistos),
                       static_cast<uint64_t>(v.vistosFormas), ctx, "progreso: formas vistas");
        revisarEnteros(static_cast<uint64_t>(p.rarezas.vistos),
                       static_cast<uint64_t>(v.vistosRarezas), ctx, "progreso: rarezas vistas");
        revisarEnteros(static_cast<uint64_t>(p.porcentaje),
                       static_cast<uint64_t>(v.porcentaje), ctx, "progreso: porcentaje");
    };

    int registradas = 0;
    for (size_t i = 0; i < semillas.size(); ++i) {
        const Registro r = registrar(codex, semillas[i], FORMAS_CODEX[i % CANT_FORMAS]);
        codex = r.codex;
        comparar(r, ++registradas);

        // La misma criatura otra vez, en los mismos dos puntos que el generador:
        // el total sube y no puede aparecer ninguna novedad.
        if (i == 3 || i == 12) {
            const Registro otra = registrar(codex, semillas[i], FORMAS_CODEX[i % CANT_FORMAS]);
            codex = otra.codex;
            revisar(otra.nuevos.empty(), "repetida",
                    "registrar la misma criatura dos veces descubrió algo nuevo");
            comparar(otra, ++registradas);
        }
    }

    // -- propiedades que no salen de los vectores ---------------------------

    // "Indefinida" no se anota nunca. Es la ausencia de un descubrimiento, no
    // uno: si se colara, el codex diría que conseguiste una forma por el solo
    // hecho de que la criatura nació.
    {
        const Registro r = registrar(Codex{}, 0xA3F091C477BE2D08ULL, Form::Indefinida);
        revisar(r.codex.formas.empty(), "indefinida",
                "la forma indefinida se anotó como descubrimiento");
    }

    // El total sube siempre, aunque no haya novedad: cuenta criaturas
    // registradas, no descubrimientos.
    {
        Codex c;
        for (int i = 0; i < 5; ++i) {
            c = registrar(c, 0xA3F091C477BE2D08ULL, Form::Coloso).codex;
        }
        revisarEnteros(static_cast<uint64_t>(c.totalRegistradas), 5, "total",
                       "cinco registros de la misma criatura");
    }

    // Y el codex nunca pierde nada: registrar solo agrega.
    {
        Codex c;
        size_t antes = 0;
        for (size_t i = 0; i < semillas.size(); ++i) {
            c = registrar(c, semillas[i], FORMAS_CODEX[i % CANT_FORMAS]).codex;
            const size_t ahora = c.linajes.size() + c.formas.size() + c.rarezas.size();
            revisar(ahora >= antes, "monotonía", "el codex perdió algo que ya tenía");
            antes = ahora;
        }
    }
}

/**
 * El mundo infinito. SIN PARIDAD — propiedades, igual que los tiles.
 *
 * Cuatro cosas, y cada una atrapa una manera distinta de arruinarlo:
 *
 *   1. La costura, que atrapa un generador con estado.
 *   2. El determinismo, que atrapa uno que dependa del orden en que se pregunta.
 *   3. La variedad, que atrapa el mundo de pasto infinito.
 *   4. La caminabilidad, que atrapa el mundo lindo e injugable.
 *
 * Las cuatro se pueden pasar con un mundo horrible. Para eso está el PNG.
 */
static void probarMundo() {
    bloque("mundo infinito (sin paridad — solo propiedades)");

    // Tres semillas distintas: una del ejemplo, una de bordes y una cualquiera.
    // Un generador que ignore la semilla pasa todo lo demás sin despeinarse.
    const Seed SEMILLAS[] = {0xA3F091C477BE2D08ULL, 0ULL, 0xFEDCBA9876543210ULL};

    // -- 1. La costura -----------------------------------------------------
    //
    // El mismo tile del mundo, preguntado como parte de dos chunks vecinos.
    // Si el generador guardara cualquier estado entre tile y tile, acá se
    // partiría el mundo en cuadrículas visibles.
    for (Seed semilla : SEMILLAS) {
        for (int32_t cx = -2; cx <= 2; ++cx) {
            for (int32_t cy = -2; cy <= 2; ++cy) {
                const Chunk izq = generarChunk(semilla, cx, cy);
                const Chunk der = generarChunk(semilla, cx + 1, cy);
                const Chunk abajo = generarChunk(semilla, cx, cy + 1);

                bool cierraX = true;
                bool cierraY = true;
                for (int i = 0; i < CHUNK; ++i) {
                    // El tile justo a la derecha del borde derecho de `izq` es el
                    // primero de `der`, y los dos tienen que coincidir con lo que
                    // diga `tileEnMundo` para esa coordenada de mundo.
                    const int32_t wx = (cx + 1) * CHUNK;
                    const int32_t wy = cy * CHUNK + i;
                    if (der.en(0, i) != static_cast<uint8_t>(tileEnMundo(semilla, wx, wy))) {
                        cierraX = false;
                    }

                    const int32_t bx = cx * CHUNK + i;
                    const int32_t by = (cy + 1) * CHUNK;
                    if (abajo.en(i, 0) != static_cast<uint8_t>(tileEnMundo(semilla, bx, by))) {
                        cierraY = false;
                    }
                }

                char ctx[96];
                std::snprintf(ctx, sizeof(ctx), "chunk (%d, %d)", cx, cy);
                revisar(cierraX, ctx, "la costura vertical no cierra con el chunk de la derecha");
                revisar(cierraY, ctx, "la costura horizontal no cierra con el de abajo");
            }
        }
    }

    // -- 2. Determinismo ----------------------------------------------------
    //
    // Y no solo "dos llamadas seguidas dan lo mismo": se pregunta la MISMA
    // región en dos órdenes distintos. Un generador que acumule algo pasa la
    // primera versión y falla esta.
    for (Seed semilla : SEMILLAS) {
        std::vector<uint8_t> derecho;
        for (int32_t y = -40; y < 40; ++y) {
            for (int32_t x = -40; x < 40; ++x) {
                derecho.push_back(static_cast<uint8_t>(tileEnMundo(semilla, x, y)));
            }
        }

        bool iguales = true;
        size_t i = 0;
        for (int32_t y = -40; y < 40 && iguales; ++y) {
            for (int32_t x = -40; x < 40; ++x) {
                if (derecho[i++] != static_cast<uint8_t>(tileEnMundo(semilla, x, y))) {
                    iguales = false;
                    break;
                }
            }
        }
        revisar(iguales, "determinismo", "la misma región dio distinto la segunda vez");

        // Al revés, de la última coordenada a la primera.
        bool alReves = true;
        for (int32_t y = 39; y >= -40 && alReves; --y) {
            for (int32_t x = 39; x >= -40; --x) {
                const size_t pos = static_cast<size_t>(y + 40) * 80 + static_cast<size_t>(x + 40);
                if (derecho[pos] != static_cast<uint8_t>(tileEnMundo(semilla, x, y))) {
                    alReves = false;
                    break;
                }
            }
        }
        revisar(alReves, "determinismo", "preguntar en otro orden dio otro mundo");
    }

    // Y semillas distintas tienen que dar mundos distintos. Un generador que se
    // olvide de mezclar la semilla pasa TODO lo demás.
    {
        int diferencias = 0;
        for (int32_t y = 0; y < 60; ++y) {
            for (int32_t x = 0; x < 60; ++x) {
                if (tileEnMundo(SEMILLAS[0], x, y) != tileEnMundo(SEMILLAS[1], x, y)) {
                    ++diferencias;
                }
            }
        }
        revisar(diferencias > 600, "dos semillas",
                "dos semillas distintas dieron casi el mismo mundo");
    }

    // -- 3. Variedad --------------------------------------------------------
    //
    // Sobre una región grande, ningún tile puede comerse el mundo ni faltar del
    // todo. El fallo típico de un generador mal calibrado es pasto hasta el
    // horizonte, y eso pasa la costura y el determinismo con nota perfecta.
    for (Seed semilla : SEMILLAS) {
        int cuenta[static_cast<size_t>(Tile::CANTIDAD)] = {};
        const int LADO = 256;
        for (int32_t y = 0; y < LADO; ++y) {
            for (int32_t x = 0; x < LADO; ++x) {
                ++cuenta[static_cast<size_t>(tileEnMundo(semilla, x, y))];
            }
        }
        const int total = LADO * LADO;

        // Los siete tiles de exterior tienen que aparecer todos.
        const Tile ESPERADOS[] = {Tile::Pasto,  Tile::Agua,      Tile::Piedra,
                                  Tile::Arbol,  Tile::PastoAlto, Tile::Arena,
                                  Tile::Musgo};
        for (Tile t : ESPERADOS) {
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "tile %d", static_cast<int>(t));
            revisar(cuenta[static_cast<size_t>(t)] > 0, ctx,
                    "no aparece ni una vez en 256x256 tiles");
        }

        // Y ninguno puede pasar del 55% del mundo.
        for (size_t t = 0; t < static_cast<size_t>(Tile::CANTIDAD); ++t) {
            if (cuenta[t] * 100 <= total * 55) continue;
            char detalle[128];
            std::snprintf(detalle, sizeof(detalle), "el tile %zu ocupa el %d%% del mundo", t,
                          cuenta[t] * 100 / total);
            revisar(false, "variedad", detalle);
        }
        revisar(true, "variedad", "ningún tile domina");

        // Los interiores NO pueden aparecer afuera: son de otro vocabulario.
        const Tile ADENTRO[] = {Tile::Piso, Tile::Pared, Tile::Alfombra, Tile::Pedestal};
        for (Tile t : ADENTRO) {
            revisar(cuenta[static_cast<size_t>(t)] == 0, "variedad",
                    "se generó un tile de interior a la intemperie");
        }
    }

    // -- 4. Caminabilidad ---------------------------------------------------
    //
    // El que de verdad importa. Un mundo con lagos y roquedales bien puestos se
    // ve precioso y puede estar partido en islas: llegás a un lago, lo rodeás, y
    // te encontrás con que el bosque de al lado es una pared maciza.
    //
    // Se inunda desde el origen sobre una región grande y se cuenta cuánto del
    // suelo caminable quedó alcanzable. Lo que no llega no es necesariamente un
    // error —una isla en medio de un lago es legítima— pero si la mayor parte
    // del suelo queda afuera, el mundo no se puede recorrer.
    for (Seed semilla : SEMILLAS) {
        const int LADO = 192;
        const int MITAD = LADO / 2;

        std::vector<uint8_t> solido(static_cast<size_t>(LADO) * LADO);
        int caminables = 0;
        for (int y = 0; y < LADO; ++y) {
            for (int x = 0; x < LADO; ++x) {
                const Tile t = tileEnMundo(semilla, x - MITAD, y - MITAD);
                const bool s = esSolido(t);
                solido[static_cast<size_t>(y) * LADO + static_cast<size_t>(x)] = s ? 1 : 0;
                if (!s) ++caminables;
            }
        }

        // Se arranca desde el primer tile caminable cerca del centro, que es
        // donde va a estar el pueblo.
        int inicio = -1;
        for (int r = 0; r < MITAD && inicio < 0; ++r) {
            for (int dy = -r; dy <= r && inicio < 0; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    const int x = MITAD + dx;
                    const int y = MITAD + dy;
                    if (x < 0 || y < 0 || x >= LADO || y >= LADO) continue;
                    if (!solido[static_cast<size_t>(y) * LADO + static_cast<size_t>(x)]) {
                        inicio = y * LADO + x;
                        break;
                    }
                }
            }
        }
        revisar(inicio >= 0, "caminabilidad", "no hay ni un tile caminable cerca del centro");
        if (inicio < 0) continue;

        std::vector<uint8_t> visto(static_cast<size_t>(LADO) * LADO, 0);
        std::vector<int> pila;
        pila.push_back(inicio);
        visto[static_cast<size_t>(inicio)] = 1;
        int alcanzados = 0;

        while (!pila.empty()) {
            const int actual = pila.back();
            pila.pop_back();
            ++alcanzados;

            const int x = actual % LADO;
            const int y = actual / LADO;
            const int VECINOS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& v : VECINOS) {
                const int nx = x + v[0];
                const int ny = y + v[1];
                if (nx < 0 || ny < 0 || nx >= LADO || ny >= LADO) continue;
                const size_t idx = static_cast<size_t>(ny) * LADO + static_cast<size_t>(nx);
                if (visto[idx] || solido[idx]) continue;
                visto[idx] = 1;
                pila.push_back(static_cast<int>(idx));
            }
        }

        const int porcentaje = caminables > 0 ? alcanzados * 100 / caminables : 0;
        char detalle[160];
        std::snprintf(detalle, sizeof(detalle),
                      "desde el centro se llega al %d%% del suelo caminable (%d de %d)",
                      porcentaje, alcanzados, caminables);
        revisar(porcentaje >= 80, "caminabilidad", detalle);

        // Y el suelo caminable tiene que ser la mayor parte del mundo: un mundo
        // que es 70% agua se recorre entero y no se puede jugar igual.
        char d2[128];
        std::snprintf(d2, sizeof(d2), "solo el %d%% del mundo se camina",
                      caminables * 100 / (LADO * LADO));
        revisar(caminables * 100 / (LADO * LADO) >= 55, "caminabilidad", d2);
    }

    // -- Las coordenadas de chunk, incluidas las negativas -------------------
    //
    // `-1 / 32` da 0 en C++, no -1: si `chunkDe` no corrigiera eso, el chunk 0
    // mediría el doble y el mundo quedaría partido justo en el origen.
    struct CasoChunk { int32_t coord; int32_t chunk; int32_t dentro; };
    static const CasoChunk CASOS[] = {
        {0, 0, 0},    {31, 0, 31},   {32, 1, 0},   {33, 1, 1},
        {-1, -1, 31}, {-32, -1, 0},  {-33, -2, 31}, {-64, -2, 0},
    };
    for (const CasoChunk& c : CASOS) {
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "coordenada %d", c.coord);
        revisarEnteros(static_cast<uint64_t>(static_cast<uint32_t>(chunkDe(c.coord))),
                       static_cast<uint64_t>(static_cast<uint32_t>(c.chunk)), ctx, "chunk");
        revisarEnteros(static_cast<uint64_t>(dentroDelChunk(c.coord)),
                       static_cast<uint64_t>(c.dentro), ctx, "posición dentro del chunk");
    }

    // Y las dos funciones tienen que reconstruir la coordenada original.
    for (int32_t c = -200; c <= 200; ++c) {
        revisarEnteros(static_cast<uint64_t>(static_cast<uint32_t>(chunkDe(c) * CHUNK + dentroDelChunk(c))),
                       static_cast<uint64_t>(static_cast<uint32_t>(c)), "ida y vuelta",
                       "chunk y offset no reconstruyen la coordenada");
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
                "%zu guardados, %zu botines de expedición, %zu cruzas,\n"
                "%zu pasos de codex\n\n",
                sizeof(vectores::GENOMAS) / sizeof(vectores::GENOMAS[0]),
                sizeof(vectores::EVOLUCIONES) / sizeof(vectores::EVOLUCIONES[0]),
                sizeof(vectores::PARSEOS) / sizeof(vectores::PARSEOS[0]),
                sizeof(vectores::HASHES) / sizeof(vectores::HASHES[0]),
                sizeof(vectores::SIMULACIONES) / sizeof(vectores::SIMULACIONES[0]),
                sizeof(vectores::RAMPAS) / sizeof(vectores::RAMPAS[0]),
                sizeof(vectores::SPRITES) / sizeof(vectores::SPRITES[0]),
                sizeof(vectores::ACCIONES) / sizeof(vectores::ACCIONES[0]),
                sizeof(vectores::SAVES) / sizeof(vectores::SAVES[0]),
                sizeof(vectores::BOTINES) / sizeof(vectores::BOTINES[0]),
                sizeof(vectores::CRUZAS) / sizeof(vectores::CRUZAS[0]),
                sizeof(vectores::CODEXS) / sizeof(vectores::CODEXS[0]));

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
    probarDespensa();
    probarBotines();
    probarSalidas();
    probarAtlas();
    probarGrillaDual();
    probarFuente();
    probarCruzas();
    probarCodex();
    probarMundo();
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
