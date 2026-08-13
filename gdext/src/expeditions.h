#pragma once
/**
 * expeditions.h — Port de src/core/expeditions.ts
 *
 * La criatura sale a buscar cosas y vuelve más tarde. Son las dos mitades que le
 * faltaban al juego:
 *
 * 1. El lado de la OFERTA de la economía. Con despensa pero sin forma de
 *    conseguir comida, quedarse sin nada es un callejón sin salida — y eso es
 *    exactamente lo que le pasaba al nativo hasta ahora.
 * 2. Un motivo concreto para volver mañana. Algo que corre mientras no estás y
 *    te espera cuando volvés.
 *
 * ---
 *
 * LA REGLA QUE EVITA EL BLOQUEO: el patio siempre está disponible.
 *
 * Es la trampa clásica de una economía cerrada: si la comida se acaba y para
 * conseguir más hace falta comida, el jugador queda trabado sin nada que hacer.
 * El patio no pide etapa, no cuesta energía y siempre trae algo. Es aburrido a
 * propósito — es un piso, no una estrategia.
 */

#include "evolution.h"
#include "inventory.h"
#include "simulation.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace petbits {

/** Un alimento y su peso en el sorteo del botín. */
struct PesoAlimento {
    std::string_view id;
    int peso;
};

struct Destino {
    std::string_view id;
    std::string_view nombre;
    /**
     * Cómo se dice "volvió de ___".
     *
     * Va escrito y no armado concatenando: "de" + "el patio" da "de el patio".
     * La contracción del castellano no sale de pegar strings.
     */
    std::string_view desde;
    std::string_view descripcion;
    int64_t duracionMs;
    double costoEnergia;
    Stage etapaMinima;
    /** Cuántos alimentos trae, mínimo y máximo. */
    int64_t itemsMin;
    int64_t itemsMax;
    /**
     * Los pesos, EN ORDEN.
     *
     * No es un map a propósito. El sorteo recorre la lista restando pesos hasta
     * cruzar el cero, así que el orden decide qué sale para cada tirada. Con un
     * map ordenado alfabéticamente el mismo genoma daría otro botín que en la
     * web, y el botín tiene que ser el mismo en las dos plataformas.
     */
    std::vector<PesoAlimento> pesos;
    /** Probabilidad de volver con una semilla. */
    double chanceSemilla;
};

const std::vector<Destino>& destinos();

/** Busca un destino por id. nullptr si no existe. */
const Destino* destinoPorId(std::string_view id);

struct Botin {
    Inventario alimentos;
    /** Genoma encontrado, para incubar. Vacío si no trajo ninguno. */
    std::optional<Seed> semilla;
};

struct PuedeSalir {
    bool puede;
    /** Solo si no puede: por qué. */
    std::string motivo;
};

bool estaFuera(const CreatureState& criatura);
PuedeSalir puedeSalir(const CreatureState& criatura, const Destino& destino);

/** Manda a la criatura. Devuelve un estado nuevo; no toca el que recibe. */
CreatureState enviar(const CreatureState& criatura, const Destino& destino, int64_t nowMs);

bool yaVolvio(const CreatureState& criatura, int64_t nowMs);

/** Cuánto falta para que vuelva, en milisegundos. */
int64_t faltaParaVolver(const CreatureState& criatura, int64_t nowMs);

/**
 * Qué trae de vuelta.
 *
 * Determinista a partir del genoma y del momento de salida, así que el botín
 * queda decidido cuando SALE, no cuando volvés a mirar. Sin eso, cerrar y
 * reabrir hasta que salga un cristal sería la estrategia óptima.
 */
Botin resolverBotin(Seed seed, const Destino& destino, int64_t salidaMs);

struct Regreso {
    bool volvio;
    CreatureState criatura;
    const Destino* destino;
    Botin botin;
};

/**
 * Recibe a la criatura si ya volvió.
 *
 * `volvio` en false si sigue afuera o si nunca salió. Volver cuenta como
 * atención: estuvo trabajando para vos, no abandonada.
 */
Regreso recibir(const CreatureState& criatura, int64_t nowMs);

/** Resume el botín en una frase. */
std::string describirBotin(const Botin& botin);

} // namespace petbits
