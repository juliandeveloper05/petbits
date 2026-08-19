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

// ---------------------------------------------------------------------------
// La grilla dual
// ---------------------------------------------------------------------------
//
// EL PROBLEMA. Dibujando un tile por celda del mundo, dos materiales se tocan en
// un escalón de dieciséis píxeles. Un parche de pasto oscuro adentro de pasto
// claro se lee como un rectángulo, una costa se lee como una escalera, y el mundo
// entero se lee como la grilla que es.
//
// LA TÉCNICA. El tile que se DIBUJA no coincide con el del mundo: va corrido
// medio tile, y su aspecto lo deciden los CUATRO tiles del mundo que toca en sus
// esquinas. Cuatro esquinas por dos estados —este material está o no está— son
// dieciséis combinaciones, y con dibujar esas dieciséis alcanza.
//
// POR QUÉ ESTA Y NO UN AUTOTILE CLÁSICO. Un blob autotile mira los ocho vecinos y
// necesita CUARENTA Y SIETE variantes por material. La grilla dual necesita 16.
// Y además los bordes cierran solos: cada esquina la comparten cuatro tiles
// dibujados vecinos, así que si los cuatro la leen igual —y la leen igual, es la
// misma celda del mundo— no hay nada que casar.
//
// CÓMO SE APILAN VARIOS MATERIALES. Por capas, de abajo hacia arriba. Cada capa
// se pinta con su propia máscara y es transparente donde no está. Una esquina
// cuenta como presente para la capa C si el tile de ahí es de C *o de cualquier
// capa de más arriba* — si no, un camino sobre pasto le haría un agujero al
// pasto.

/**
 * Las capas de terreno, de abajo hacia arriba.
 *
 * El orden ES la prioridad de dibujo y no es arbitrario: el agua abajo de todo
 * porque es el fondo del mundo, y el camino arriba de todo porque lo hizo alguien
 * y se apoya sobre lo que ya había.
 */
enum class Capa : uint8_t {
    Agua = 0,
    Arena,
    Pasto,
    Musgo,
    PastoAlto,
    Camino,
    CANTIDAD,
};

/**
 * A qué capa pertenece el SUELO de ese tile.
 *
 * Suelo, no tile: abajo de un árbol hay pasto y abajo de una piedra hay musgo.
 * Los árboles y las piedras no son terreno — son objetos que se apoyan encima, y
 * por eso no llevan transición: un árbol no se funde con el de al lado.
 */
Capa capaDeSuelo(Tile t);

/** ¿Ese tile cuenta como presente para esa capa? Es `capaDeSuelo(t) >= c`. */
bool perteneceA(Tile t, Capa c);

/**
 * La máscara de las cuatro esquinas, en [0, 16).
 *
 * Bit 0 = arriba-izquierda, 1 = arriba-derecha, 2 = abajo-izquierda,
 * 3 = abajo-derecha. Cero es "esta capa no aparece" y quince es "llena".
 */
int mascaraDeEsquinas(Tile arribaIzq, Tile arribaDer, Tile abajoIzq, Tile abajoDer, Capa c);

// ---------------------------------------------------------------------------
// Los objetos
// ---------------------------------------------------------------------------

/** Lo que se apoya encima del terreno, sin transiciones. */
enum class Objeto : uint8_t { Ninguno, Arbol, Piedra };

Objeto objetoDe(Tile t);

/** Cuántos dibujos distintos hay de cada objeto. */
inline constexpr int VARIANTES_ARBOL = 4;
inline constexpr int VARIANTES_PIEDRA = 2;

// ---------------------------------------------------------------------------
// La forma del atlas
// ---------------------------------------------------------------------------
//
// Dejó de ser una fila de N tiles. Ahora es una grilla: una fila por capa con sus
// dieciséis máscaras, una de objetos con sus variantes, y una de tiles llanos
// —los interiores— que no llevan grilla dual porque una sala no tiene costas.

inline constexpr int COLUMNAS_ATLAS = 16;
inline constexpr int FILA_OBJETOS = static_cast<int>(Capa::CANTIDAD);
inline constexpr int FILA_LLANOS = FILA_OBJETOS + 1;
inline constexpr int FILAS_ATLAS = FILA_LLANOS + 1;

/** Dónde cae un objeto en la fila de objetos. */
int columnaDeObjeto(Objeto o, int variante);

/** Y dónde cae un tile llano de interior. */
int columnaLlana(Tile t);

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
