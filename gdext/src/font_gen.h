#pragma once
/**
 * font_gen — la tipografía del juego, dibujada píxel por píxel en código.
 *
 * Una bitmap font de 5×7 en una caja de 5×11, que es la forma que tenían las
 * fuentes de consola portátil de los noventa: trazo de un píxel, sin
 * antialiasing, sin curvas, ancho fijo.
 *
 * ---
 *
 * POR QUÉ ESTÁ ACÁ Y NO ES UN .TTF DESCARGADO.
 *
 * En este proyecto no hay un solo asset dibujado a mano: los sprites, las
 * paletas y los tiles del mundo salen de código. Meter un archivo de fuente
 * rompería esa regla, y además traería una licencia de un tercero al repo por
 * la única cosa que aparece en absolutamente todas las pantallas.
 *
 * Y hay una razón práctica: una fuente vectorial escalada a 11 píxeles se ve
 * blanda. El pixel art de 32×32 y el texto tienen que estar en la misma grilla
 * o la pantalla se parte en dos estéticas.
 *
 * ---
 *
 * LA CAJA, Y POR QUÉ TIENE ONCE FILAS Y NO OCHO.
 *
 *   fila 0-1   zona de acentos de las MAYÚSCULAS
 *   fila 2-3   zona de acentos de las minúsculas / tope de mayúsculas y altas
 *   fila 4-8   altura de x (el cuerpo de a, e, o…)
 *   fila 9-10  descendentes (g, j, p, q, y)
 *
 * La línea de base está debajo de la fila 8: ascenso 9, descenso 2.
 *
 * Una fuente de consola clásica entra en 8×8, pero el castellano no. Con ocho
 * filas hay que elegir entre descendentes de un píxel —una `p` que parece una
 * `o` con un palito— o acentos pegados a la letra. Once filas no es capricho: es
 * lo que cuesta que "Nébula está de expedición" se lea bien.
 *
 * ---
 *
 * LOS ACENTOS NO SE DIBUJAN, SE COMPONEN.
 *
 * `á` es la `a` con una tilde estampada dos filas más arriba de donde empieza su
 * cuerpo; `¿` es el `?` rotado 180°, que es literalmente lo que es. Dibujar cada
 * variante a mano serían veinte glifos más y veinte oportunidades de que la
 * tilde de la `ó` quede un píxel más alta que la de la `á`. Componer garantiza
 * que no.
 *
 * La única excepción es la `í`, que necesita perder su punto antes de recibir la
 * tilde. Para eso hay una `i` sin punto interna, que no se expone como glifo.
 */

#include <cstdint>
#include <vector>

namespace petbits {

/** Las medidas de la caja. Cambiarlas obliga a redibujar todo, no es un ajuste. */
struct Fuente {
    static constexpr int ANCHO = 5;    ///< columnas de tinta por glifo
    static constexpr int ALTO = 11;    ///< filas de la caja
    static constexpr int AVANCE = 6;   ///< cuánto se corre el cursor: 5 + 1 de aire
    static constexpr int ASCENSO = 9;  ///< filas por encima de la línea de base
    static constexpr int DESCENSO = 2; ///< filas por debajo
    static constexpr int COLUMNAS = 16; ///< glifos por fila en el atlas
};

/**
 * Un glifo: su punto de código y once filas de cinco bits.
 *
 * El bit más significativo de los cinco (0b10000) es la columna de la izquierda.
 */
struct Glifo {
    char32_t codigo = 0;
    uint8_t filas[Fuente::ALTO] = {};

    bool vacio() const;
};

/** Todos los glifos, ordenados por punto de código. */
const std::vector<Glifo>& fuenteGlifos();

/** Dónde cae un glifo en el atlas, en píxeles. Devuelve false si no existe. */
bool fuenteUbicacion(char32_t codigo, int& x, int& y);

/** Cuántas filas de glifos tiene el atlas. */
int fuenteFilasAtlas();

/** El atlas: tinta blanca opaca sobre transparente, RGBA8 fila por fila. */
struct ImagenFuente {
    int ancho = 0;
    int alto = 0;
    std::vector<uint8_t> rgba;
};
ImagenFuente fuenteAtlas();

} // namespace petbits
