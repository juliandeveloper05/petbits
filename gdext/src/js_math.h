#pragma once
/**
 * Operaciones numéricas con la semántica de JavaScript.
 *
 * Están acá porque el nombre parecido esconde una diferencia real, y el
 * generador de sprites redondea cientos de veces por criatura.
 */

#include <cmath>

namespace petbits {

/**
 * `Math.round` de JavaScript.
 *
 * NO es `std::round`. Las dos redondean el medio para el mismo lado solo
 * mientras el número es positivo:
 *
 *     Math.round(-1.5)  ===  -1      (hacia +infinito)
 *     std::round(-1.5)  ===  -2      (alejándose del cero)
 *
 * El generador de sprites redondea coordenadas que salen de restas —posiciones
 * de ojos, de cachetes, de apéndices— y varias caen del lado negativo en
 * cuerpos chicos. Ahí `std::round` corre un píxel y la criatura del nativo deja
 * de ser la de la web.
 *
 * `floor(x + 0.5)` es exactamente lo que dice la especificación de ECMAScript.
 */
inline double jsRound(double x) {
    return std::floor(x + 0.5);
}

/** Igual, pero devolviendo entero, que es como se usa casi siempre. */
inline int jsRoundI(double x) {
    return static_cast<int>(jsRound(x));
}

} // namespace petbits
