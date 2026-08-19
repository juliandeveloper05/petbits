#pragma once
/**
 * world_gen — el terreno infinito.
 *
 * Dada una semilla y un par de coordenadas, devuelve qué hay ahí. Sin estado,
 * sin caché, sin orden de recorrido: `tileEnMundo(s, 100, 40)` da lo mismo se
 * pregunte cuando se pregunte y se haya preguntado lo que se haya preguntado
 * antes.
 *
 * ---
 *
 * ESA AUSENCIA DE ESTADO ES TODO EL DISEÑO.
 *
 * Un mundo por chunks tiene una sola forma de romperse feo: que el chunk (5, 3)
 * salga distinto según si llegaste desde la izquierda o desde arriba. Y la forma
 * más natural de escribirlo —un PRNG sembrado por chunk que se va consumiendo
 * mientras se recorre la grilla— tiene exactamente ese defecto, porque el valor
 * de un tile pasa a depender de cuántos tiles se generaron antes.
 *
 * Acá el ruido se muestrea en coordenadas de MUNDO. El tile (159, 40) se calcula
 * igual esté al final del chunk 4 o al principio del 5, así que las costuras no
 * hay que "arreglarlas": no existen. Es más barato garantizar la propiedad que
 * corregirla después, y el test de costura está para que siga siendo cierto.
 *
 * ---
 *
 * POR QUÉ NO SE USA EL RUIDO DE GODOT.
 *
 * `FastNoiseLite` haría esto en tres líneas y dejaría la generación adentro del
 * motor: sin tests que corran con un compilador y un comando, sin poder mirar
 * una región desde la herramienta que compone PNG, y con el mundo dependiendo de
 * la versión de Godot que tengas instalada.
 *
 * El ruido de valor de acá se apoya en `splitmix64`, que ya existe, ya está
 * verificado contra el TypeScript y no va a cambiar. El mundo de una semilla es
 * el mismo hoy, en otra máquina y dentro de tres versiones del motor.
 *
 * ---
 *
 * ESTOS TESTS NO SON DE PARIDAD.
 *
 * No hay TypeScript contra qué comparar: el mundo existe solo del lado nativo,
 * igual que los tiles y la tipografía. Se comprueban PROPIEDADES —costura,
 * determinismo, variedad, caminabilidad— y conviene tener presente que es una
 * red más floja: un mundo feo las pasa todas.
 */

#include "genome.h"
#include "tileset_gen.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace petbits {

/**
 * Cuántos tiles de lado tiene un chunk.
 *
 * 32 sobre un viewport de 30 × 17 tiles: alcanza para que el anillo de 3 × 3 que
 * mantiene cargado el juego cubra la pantalla con margen de sobra en las cuatro
 * direcciones, y es chico como para que construir uno nuevo al cruzar un borde
 * no se note.
 */
inline constexpr int CHUNK = 32;

/** Un pedazo de mundo ya resuelto, listo para volcar en un TileMapLayer. */
struct Chunk {
    int32_t cx = 0;
    int32_t cy = 0;
    /** Fila por fila, `CHUNK * CHUNK` índices de `Tile`. */
    std::array<uint8_t, static_cast<size_t>(CHUNK) * CHUNK> tiles{};

    uint8_t en(int x, int y) const {
        return tiles[static_cast<size_t>(y) * CHUNK + static_cast<size_t>(x)];
    }
};

/**
 * Los biomas. No son tiles: son la razón por la que un tile está donde está.
 *
 * Se exponen aparte porque la interfaz los necesita para decir dónde estás
 * parado, y deducirlo del tile no alcanza — el pasto aparece en la pradera y en
 * el claro de un bosque, y no es lo mismo.
 */
enum class Bioma : uint8_t {
    Lago,
    Costa,
    Pradera,
    Bosque,
    Humedal,
    Roquedal,
    CANTIDAD,
};

std::string_view nombreBioma(Bioma b);

/** Qué bioma hay en esa coordenada de mundo. */
Bioma biomaEn(Seed semilla, int32_t x, int32_t y);

// ---------------------------------------------------------------------------
// El pueblo
// ---------------------------------------------------------------------------
//
// El pueblo es parte del mundo, no una excepción que `Mundo.gd` tenga que
// recordar. `tileEnMundo` devuelve sus tiles cuando la coordenada cae adentro de
// su rectángulo, así que caminás hasta el borde y seguís caminando sin que nadie
// haga nada especial.
//
// Su grilla vive acá y no en GDScript por eso mismo: si viviera del otro lado,
// el generador no podría verla y habría que componer los dos afuera, con un caso
// especial en el walker. Lo que sí se queda en GDScript son los PUNTOS —las
// entradas, el NPC—, que son datos de juego y no terreno.

/** Dónde arranca el pueblo, en coordenadas de mundo. Está centrado en el origen. */
inline constexpr int32_t PUEBLO_X = -15;
inline constexpr int32_t PUEBLO_Y = -8;
inline constexpr int32_t PUEBLO_ANCHO = 30;
inline constexpr int32_t PUEBLO_ALTO = 17;

/** ¿Esa coordenada de mundo cae adentro del pueblo? */
bool enElPueblo(int32_t x, int32_t y);

/**
 * ¿Cae en una de las cuatro sendas que salen del pueblo?
 *
 * El borde del pueblo tiene un hueco en cada lado, y del otro lado del hueco el
 * mundo pone lo que quiere: un lago, un roquedal, una arboleda maciza.
 * Cualquiera de las tres deja la puerta tapada.
 *
 * No es hipotético. El test de salidas falló una de cada tres corridas —solo
 * cuando el seed al azar ponía agua justo ahí— y ese es el peor tipo de falla:
 * la que aparece en el juego de alguien y nunca en el tuyo.
 */
bool enUnaSenda(int32_t x, int32_t y);

// ---------------------------------------------------------------------------
// Qué hay para encontrar
// ---------------------------------------------------------------------------
//
// Sin esto, un mundo infinito es un fondo de pantalla muy grande: caminás,
// cambia el color, y no pasa nada más. Lo que sigue es lo mínimo para que
// caminar sea una de las dos economías del juego —la que da lo que se consigue
// ESTANDO ahí— en vez de una competencia perdida contra las expediciones.

/** Qué se puede sacar de un lugar. */
enum class Hallazgo : uint8_t {
    Nada,
    /// Pasto alto: comida. Es lo que hace que caminar valga aunque no encuentres
    /// nada raro.
    Forraje,
    /// Un afloramiento de piedra: mineral, que es la comida de la rama pétrea.
    Veta,
    /// Un hito: algo que alguien dejó. Da un genoma para incubar, y es lo que
    /// conecta el mundo con el criadero sin pasar por las expediciones.
    Hito,
};

/**
 * Qué hay en esa coordenada.
 *
 * Es una función del mundo y no del jugador: dos personas con la misma semilla
 * encuentran el hito en el mismo lugar, y pueden decirse "está al noreste del
 * lago". Que ya lo hayas levantado o no es otra cosa, y se guarda aparte.
 */
Hallazgo hallazgoEn(Seed semilla, int32_t x, int32_t y);

/**
 * El genoma que deja un hito.
 *
 * Sale de la posición, no de un sorteo: el hito del (312, -88) tiene la misma
 * criatura adentro para todo el mundo con esa semilla. Es lo mismo que hace que
 * las rarezas sean verificables en vez de un premio oculto.
 */
Seed semillaDeHito(Seed semilla, int32_t x, int32_t y);

/**
 * Qué tile hay en esa coordenada de mundo.
 *
 * Total: cualquier par de enteros de 32 bits tiene respuesta, incluidos los
 * negativos. El mundo no tiene borde ni origen privilegiado.
 */
Tile tileEnMundo(Seed semilla, int32_t x, int32_t y);

/** Resuelve un chunk entero. Es `tileEnMundo` mil veces, y nada más. */
Chunk generarChunk(Seed semilla, int32_t cx, int32_t cy);

/**
 * De coordenada de mundo a coordenada de chunk, con división hacia abajo.
 *
 * `-1 / 32` en C++ da 0, no -1: la división entera trunca hacia el cero y eso
 * partiría el mundo en dos justo en el origen, con el chunk 0 midiendo el doble.
 * Esta función redondea siempre hacia abajo, que es lo que hace que el mundo sea
 * uniforme a los dos lados del cero.
 */
int32_t chunkDe(int32_t coordenada);

/** Y la coordenada dentro del chunk, siempre en [0, CHUNK). */
int32_t dentroDelChunk(int32_t coordenada);

} // namespace petbits
