#pragma once
/**
 * tileset_gen.h — Los tiles del mundo, generados por código.
 *
 * Igual que las criaturas: no hay un PNG dibujado a mano en ninguna parte. El
 * atlas sale de la misma maquinaria OKLCH que las paletas, así que el pasto y
 * una criatura de linaje Musgo comparten el espacio de color y se ven de la
 * misma familia.
 *
 * ---
 *
 * ESTE MÓDULO NO TIENE CONTRAPARTE EN LA WEB, y eso cambia cómo se verifica.
 *
 * Todo lo demás en gdext/ es un port: hay un TypeScript que dice cuál es la
 * respuesta correcta, y los tests comparan contra él. Acá no hay contra qué
 * comparar — el mundo navegable existe solo del lado nativo.
 *
 * Así que sus tests son tests de verdad y no vectores de paridad: comprueban
 * propiedades (que cada tile sea opaco, que los bordes del camino peguen con el
 * pasto, que dos llamadas den lo mismo) en vez de igualdad contra una
 * referencia. Es más débil, y es lo que hay. Conviene tenerlo presente al
 * agregar cosas acá.
 */

#include "pixel_buffer.h"

#include <cstdint>
#include <vector>

namespace petbits {

/** Lado de un tile, en píxeles. La resolución del juego es 480×270: 30×17 tiles. */
inline constexpr int TILE = 16;

/**
 * Los tipos de tile.
 *
 * El orden es el del atlas y NO se reordena: los mapas guardan el índice, así
 * que mover una entrada cambia todos los mapas ya escritos.
 */
enum class Tile : uint8_t {
    Pasto = 0,
    Camino,
    Agua,
    Piedra,
    Arbol,
    PastoAlto,
    Arena,
    Musgo,

    // --- interiores ---
    //
    // Van AL FINAL y no en su lugar "lógico" entre los de afuera. El índice de
    // cada tile es lo que guardan las grillas de los mapas, así que insertar uno
    // en el medio los reescribe todos en silencio: el pueblo seguiría cargando y
    // el pasto sería agua.
    Piso,      ///< Tablas de madera. Se camina.
    Pared,     ///< Sólida. Es el borde de un interior.
    Alfombra,  ///< Se camina. Marca por dónde se pasa.
    Pedestal,  ///< Sólido. Es donde se apoya una criatura para cruzar.

    CANTIDAD,
};

/** ¿Se puede caminar por encima? */
bool esSolido(Tile t);

/**
 * El atlas completo: todos los tiles en una fila, RGBA.
 *
 * Una sola imagen y no una por tile porque es lo que espera el TileSet de
 * Godot, que recorta de un atlas por coordenada.
 */
struct Atlas {
    int width;
    int height;
    std::vector<uint8_t> data;
};

Atlas generarAtlas();

} // namespace petbits
