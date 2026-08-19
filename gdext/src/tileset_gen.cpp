/**
 * tileset_gen.cpp — ver tileset_gen.h.
 *
 * Cada capa de terreno se dibuja dieciséis veces, una por combinación de esquinas
 * presentes. Los objetos —árboles, piedras— se dibujan enteros y con variantes.
 * Todo sale de un PRNG sembrado por índice, así que el atlas es idéntico en cada
 * corrida y en cada máquina.
 *
 * ---
 *
 * LA COBERTURA ES BILINEAL, Y ESO ES LO QUE REDONDEA LOS BORDES.
 *
 * Para cada píxel del tile se suma, por cada esquina presente, cuánto "pesa" esa
 * esquina en ese punto: `(1-du)·(1-dv)`. Si el total pasa de la mitad, hay tinta.
 *
 * Con las cuatro esquinas la suma da uno en todos lados y el tile queda lleno.
 * Con dos esquinas de un mismo lado la cuenta se simplifica a una recta y sale un
 * borde derecho. Y con UNA sola esquina queda `(1-u)·(1-v) ≥ ½`, que es una
 * hipérbola: un borde curvo, sin haber escrito en ningún lado la palabra círculo.
 *
 * Esa curva es toda la diferencia entre un mundo que se lee como terreno y uno
 * que se lee como una planilla.
 */

#include "tileset_gen.h"
#include "palette.h"
#include "rng.h"

#include <algorithm>
#include <cmath>

namespace petbits {

bool esSolido(Tile t) {
    switch (t) {
        case Tile::Agua:
        case Tile::Piedra:
        case Tile::Arbol:
        case Tile::Pared:
        case Tile::Pedestal:
            return true;
        default:
            return false;
    }
}

Capa capaDeSuelo(Tile t) {
    switch (t) {
        case Tile::Agua:      return Capa::Agua;
        case Tile::Arena:     return Capa::Arena;
        case Tile::Musgo:     return Capa::Musgo;
        case Tile::PastoAlto: return Capa::PastoAlto;
        case Tile::Camino:    return Capa::Camino;

        // Abajo de un árbol hay pasto y abajo de una piedra hay musgo. Si el
        // árbol fuera su propia capa, un bosque sería una mancha lisa con los
        // troncos encima en vez de árboles parados sobre el suelo.
        case Tile::Arbol:     return Capa::Pasto;
        case Tile::Piedra:    return Capa::Musgo;

        default:              return Capa::Pasto;
    }
}

bool perteneceA(Tile t, Capa c) {
    return static_cast<int>(capaDeSuelo(t)) >= static_cast<int>(c);
}

int mascaraDeEsquinas(Tile arribaIzq, Tile arribaDer, Tile abajoIzq, Tile abajoDer, Capa c) {
    int m = 0;
    if (perteneceA(arribaIzq, c)) m |= 1;
    if (perteneceA(arribaDer, c)) m |= 2;
    if (perteneceA(abajoIzq, c)) m |= 4;
    if (perteneceA(abajoDer, c)) m |= 8;
    return m;
}

Objeto objetoDe(Tile t) {
    if (t == Tile::Arbol) return Objeto::Arbol;
    if (t == Tile::Piedra) return Objeto::Piedra;
    return Objeto::Ninguno;
}

int columnaDeObjeto(Objeto o, int variante) {
    if (o == Objeto::Arbol) return variante % VARIANTES_ARBOL;
    if (o == Objeto::Piedra) return VARIANTES_ARBOL + (variante % VARIANTES_PIEDRA);
    return 0;
}

int columnaLlana(Tile t) {
    switch (t) {
        case Tile::Piso:     return 0;
        case Tile::Pared:    return 1;
        case Tile::Alfombra: return 2;
        case Tile::Pedestal: return 3;
        default:             return 0;
    }
}

namespace {

/**
 * La receta de cada capa, en OKLCH.
 *
 * Se declaran tono, luminosidad y croma en vez de RGB para que todo comparta el
 * espacio de color de las criaturas. Un verde elegido a ojo en RGB no tiene por
 * qué convivir con un cuerpo generado en OKLCH; elegido acá, sí.
 */
struct Receta {
    double hue;
    double luz;
    double croma;
    /** Cuánto varía la luz en el moteado. */
    double variacion;
};

constexpr Receta RECETAS_CAPA[static_cast<size_t>(Capa::CANTIDAD)] = {
    /* Agua      */ {236, 0.52, 0.110, 0.026},
    /* Arena     */ {80, 0.82, 0.055, 0.022},
    // Los tres verdes estan separados en luz Y en tono. Con solo la luz, a
    // media distancia se leen como el mismo verde con sombra; con el tono
    // tambien, se leen como tres cosas distintas.
    /* Pasto     */ {132, 0.70, 0.078, 0.028},
    /* Musgo     */ {162, 0.54, 0.070, 0.032},
    /* PastoAlto */ {120, 0.58, 0.105, 0.030},
    /* Camino    */ {66, 0.71, 0.050, 0.025},
};

constexpr Receta RECETAS_LLANAS[4] = {
    /* Piso     */ {58, 0.64, 0.038, 0.022},
    /* Pared    */ {40, 0.33, 0.030, 0.020},
    /* Alfombra */ {28, 0.48, 0.085, 0.030},
    /* Pedestal */ {250, 0.62, 0.012, 0.035},
};

/**
 * Cuánto de este tile cubre la capa, en [0, 1].
 *
 * La suma bilineal de las esquinas presentes. Ver el comentario de arriba del
 * archivo: es de acá de donde salen los bordes curvos.
 */
double cobertura(int mascara, double u, double v) {
    double total = 0.0;
    // Las cuatro esquinas, en el orden de los bits: (0,0), (1,0), (0,1), (1,1).
    constexpr double CU[4] = {0.0, 1.0, 0.0, 1.0};
    constexpr double CV[4] = {0.0, 0.0, 1.0, 1.0};
    for (int i = 0; i < 4; ++i) {
        if ((mascara & (1 << i)) == 0) continue;
        total += (1.0 - std::fabs(u - CU[i])) * (1.0 - std::fabs(v - CV[i]));
    }
    return total;
}

/** El color base de una receta, con su moteado. */
Rgb tonoDe(const Receta& r, Rng& rng, double desplazamiento = 0.0) {
    const double d = (rng.next() - 0.5) * 2.0 * r.variacion;
    return oklchToRgb(r.luz + d + desplazamiento, r.croma, r.hue);
}

/**
 * Dibuja una máscara de una capa.
 *
 * El borde lleva un ribete más claro de un píxel. Es lo que hace que una costa se
 * lea como costa y no como un cambio de color: sin él, dos verdes parecidos se
 * tocan y no se ve dónde termina uno.
 */
void dibujarMascara(PixelBuffer& buffer, int x0, int y0, Capa capa, int mascara) {
    const Receta& r = RECETAS_CAPA[static_cast<size_t>(capa)];

    // Sembrado por capa y máscara: el atlas sale igual siempre, y agregar una
    // capa al final no cambia el moteado de las que ya estaban.
    Rng rng = rngFor(0xB17B175ULL, "capa:" + std::to_string(static_cast<int>(capa)) + ":" +
                                       std::to_string(mascara));

    // El ribete: apenas mas claro que el material, no un contorno.
    //
    // La primera version usaba +0.075 de luz y dibujaba una linea visible
    // alrededor de cada parche: el mundo se veia como un mapa vectorial con
    // bordes trazados. Un borde de terreno no tiene contorno — tiene un cambio
    // de luz, y eso es +0.03.
    const Rgb ribete = oklchToRgb(r.luz + 0.030, r.croma * 0.92, r.hue + 3);

    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            // El +0.5 centra la muestra en el píxel. Sin él, la cobertura se
            // evalúa en la esquina y el borde queda corrido medio píxel — que a
            // dieciséis píxeles de tile se nota.
            const double u = (x + 0.5) / TILE;
            const double v = (y + 0.5) / TILE;

            const double c = cobertura(mascara, u, v);
            if (c < 0.5) continue;

            // Cerca del borde, el ribete. La franja es angosta a propósito: más
            // ancha y el tile entero se vuelve contorno.
            const bool enElBorde = (c < 0.60) && (mascara != 15);
            buffer.set(x0 + x, y0 + y, enElBorde ? ribete : tonoDe(r, rng));
        }
    }
}

/** Manchas de un tono más oscuro, para que un campo grande no sea liso. */
void manchones(PixelBuffer& buffer, int x0, int y0, Capa capa, int mascara, Rng& rng,
               int cuantos) {
    const Receta& r = RECETAS_CAPA[static_cast<size_t>(capa)];
    const Rgb oscuro = oklchToRgb(r.luz - 0.055, r.croma * 1.1, r.hue - 5);

    for (int i = 0; i < cuantos; ++i) {
        const int cx = static_cast<int>(rng.intMenorQue(TILE));
        const int cy = static_cast<int>(rng.intMenorQue(TILE));
        const int radio = 1 + static_cast<int>(rng.intMenorQue(2));

        for (int y = cy - radio; y <= cy + radio; ++y) {
            for (int x = cx - radio; x <= cx + radio; ++x) {
                if (x < 0 || y < 0 || x >= TILE || y >= TILE) continue;
                const int dx = x - cx;
                const int dy = y - cy;
                if (dx * dx + dy * dy > radio * radio) continue;
                // Solo adentro de la capa: un manchón que se salga de la máscara
                // pinta encima de lo que hay abajo.
                if (cobertura(mascara, (x + 0.5) / TILE, (y + 0.5) / TILE) < 0.5) continue;
                buffer.set(x0 + x, y0 + y, oscuro);
            }
        }
    }
}

/** Briznas de pasto: líneas verticales cortas. */
void briznas(PixelBuffer& buffer, int x0, int y0, Capa capa, int mascara, Rng& rng,
             int cuantas) {
    const Receta& r = RECETAS_CAPA[static_cast<size_t>(capa)];
    const Rgb hoja = oklchToRgb(r.luz + 0.085, r.croma * 1.15, r.hue + 8);

    for (int i = 0; i < cuantas; ++i) {
        const int x = static_cast<int>(rng.intMenorQue(TILE));
        const int y = static_cast<int>(rng.intMenorQue(TILE - 3));
        const int alto = 2 + static_cast<int>(rng.intMenorQue(2));
        for (int d = 0; d < alto; ++d) {
            if (cobertura(mascara, (x + 0.5) / TILE, (y + d + 0.5) / TILE) < 0.5) continue;
            buffer.set(x0 + x, y0 + y + d, hoja);
        }
    }
}

/** El agua: dos tonos y una línea de espuma pegada a la orilla. */
void detalleAgua(PixelBuffer& buffer, int x0, int y0, int mascara, Rng& rng) {
    const Receta& r = RECETAS_CAPA[static_cast<size_t>(Capa::Agua)];
    const Rgb hondo = oklchToRgb(r.luz - 0.085, r.croma * 1.05, r.hue - 6);
    const Rgb brillo = oklchToRgb(r.luz + 0.16, r.croma * 0.55, r.hue + 6);

    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            const double c = cobertura(mascara, (x + 0.5) / TILE, (y + 0.5) / TILE);
            if (c < 0.5) continue;
            // Bien adentro de la máscara, más hondo. Es lo que le da volumen a un
            // lago: sin esto es una mancha azul de un solo tono.
            if (c > 0.86) buffer.set(x0 + x, y0 + y, hondo);
        }
    }

    // Dos rayas horizontales claras: leen como reflejo sin necesidad de animar.
    for (int y : {4, 11}) {
        const int desde = static_cast<int>(rng.intMenorQue(5));
        const int largo = 4 + static_cast<int>(rng.intMenorQue(5));
        for (int x = desde; x < std::min(TILE, desde + largo); ++x) {
            if (cobertura(mascara, (x + 0.5) / TILE, (y + 0.5) / TILE) < 0.72) continue;
            buffer.set(x0 + x, y0 + y, brillo);
        }
    }
}

/**
 * Una copa de árbol.
 *
 * Redonda, con la luz arriba a la izquierda — la misma dirección que los sprites
 * de las criaturas. Que las dos cosas se iluminen desde el mismo lado es lo que
 * hace que convivan en una pantalla.
 *
 * Cada variante corre el centro y el tamaño. Con una sola, un bosque se lee como
 * un sello repetido, que es exactamente lo que pasaba antes.
 */
void dibujarArbol(PixelBuffer& buffer, int x0, int y0, int variante) {
    Rng rng = rngFor(0xA2B0111ULL, "arbol:" + std::to_string(variante));

    // La copa llena el tile y se pasa un poco. Es deliberado: en la primera
    // version media unos seis pixeles de radio y un bosque se leia como lunares
    // sobre pasto. Las copas de un bosque se TOCAN — esa masa continua, con
    // huecos donde se ve el suelo, es lo que lo hace leer como bosque.
    const double cx = 8.0 + (rng.next() - 0.5) * 1.6;
    const double cy = 7.6 + (rng.next() - 0.5) * 1.2;
    const double rx = 8.2 + rng.next() * 1.0;
    const double ry = 7.6 + rng.next() * 1.0;

    const Rgb copa = oklchToRgb(0.50, 0.095, 148);
    const Rgb luz = oklchToRgb(0.60, 0.090, 142);
    const Rgb sombra = oklchToRgb(0.38, 0.085, 152);
    const Rgb tronco = oklchToRgb(0.40, 0.055, 62);

    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            const double dx = (x + 0.5 - cx) / rx;
            const double dy = (y + 0.5 - cy) / ry;
            const double d = dx * dx + dy * dy;
            if (d > 1.0) continue;

            // Tres bandas: el reborde oscuro define la silueta contra el pasto, y
            // sin él las copas de un bosque se funden en una mancha.
            if (d > 0.82) {
                buffer.set(x0 + x, y0 + y, sombra);
            } else if (dx + dy < -0.45) {
                buffer.set(x0 + x, y0 + y, luz);
            } else {
                buffer.set(x0 + x, y0 + y, copa);
            }
        }
    }

    // El tronco solo asoma si la copa deja lugar. Con copas grandes casi nunca
    // se ve, y esta bien: desde arriba, un arbol es su copa.
    const int tx = static_cast<int>(cx);
    for (int y = static_cast<int>(cy + ry) - 1; y < TILE; ++y) {
        if (y < 0 || y >= TILE) continue;
        buffer.set(x0 + tx, y0 + y, tronco);
        if (tx + 1 < TILE) buffer.set(x0 + tx + 1, y0 + y, tronco);
    }
}

/** Un afloramiento de piedra, con su cara iluminada. */
void dibujarPiedra(PixelBuffer& buffer, int x0, int y0, int variante) {
    Rng rng = rngFor(0x9ED2A5ULL, "piedra:" + std::to_string(variante));

    const Rgb cara = oklchToRgb(0.66, 0.014, 250);
    const Rgb cuerpo = oklchToRgb(0.55, 0.014, 250);
    const Rgb sombra = oklchToRgb(0.42, 0.016, 248);

    const double cx = 8.0 + (rng.next() - 0.5) * 2.0;
    const double cy = 9.0 + (rng.next() - 0.5) * 1.5;
    const double rx = 5.5 + rng.next() * 1.5;
    const double ry = 4.5 + rng.next() * 1.2;

    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            const double dx = (x + 0.5 - cx) / rx;
            const double dy = (y + 0.5 - cy) / ry;
            const double d = dx * dx + dy * dy;
            if (d > 1.0) continue;
            if (d > 0.80) {
                buffer.set(x0 + x, y0 + y, sombra);
            } else if (dy < -0.25) {
                buffer.set(x0 + x, y0 + y, cara);
            } else {
                buffer.set(x0 + x, y0 + y, cuerpo);
            }
        }
    }
}

/** Los tiles de interior, sin grilla dual: una sala no tiene costas. */
void dibujarLlano(PixelBuffer& buffer, int x0, int y0, Tile t) {
    const Receta& r = RECETAS_LLANAS[columnaLlana(t)];
    Rng rng = rngFor(0xC0FEE00ULL, "llano:" + std::to_string(static_cast<int>(t)));

    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            buffer.set(x0 + x, y0 + y, tonoDe(r, rng));
        }
    }

    switch (t) {
        case Tile::Piso: {
            const Rgb junta = oklchToRgb(r.luz - 0.055, r.croma, r.hue - 4);
            for (int x = 0; x < TILE; ++x) {
                buffer.set(x0 + x, y0 + 5, junta);
                buffer.set(x0 + x, y0 + 11, junta);
            }
            break;
        }
        case Tile::Pared: {
            const Rgb luz = oklchToRgb(r.luz + 0.14, r.croma * 0.8, r.hue + 4);
            const Rgb junta = oklchToRgb(r.luz - 0.12, r.croma, r.hue);
            for (int x = 0; x < TILE; ++x) {
                buffer.set(x0 + x, y0 + 0, luz);
                buffer.set(x0 + x, y0 + 1, luz);
                buffer.set(x0 + x, y0 + 8, junta);
            }
            for (int y = 2; y < 8; ++y) buffer.set(x0 + 6, y0 + y, junta);
            for (int y = 9; y < TILE; ++y) buffer.set(x0 + 12, y0 + y, junta);
            break;
        }
        case Tile::Alfombra: {
            const Rgb borde = oklchToRgb(r.luz - 0.14, r.croma * 1.1, r.hue - 8);
            const Rgb hilo = oklchToRgb(r.luz + 0.12, r.croma * 0.7, r.hue + 10);
            for (int i = 0; i < TILE; ++i) {
                buffer.set(x0 + i, y0 + 0, borde);
                buffer.set(x0 + i, y0 + TILE - 1, borde);
                buffer.set(x0 + i, y0 + 2, hilo);
                buffer.set(x0 + i, y0 + TILE - 3, hilo);
            }
            break;
        }
        case Tile::Pedestal: {
            const Rgb cara = oklchToRgb(r.luz + 0.16, r.croma, r.hue);
            const Rgb canto = oklchToRgb(r.luz - 0.18, r.croma, r.hue);
            for (int y = 3; y < TILE - 1; ++y) {
                for (int x = 2; x < TILE - 2; ++x) {
                    buffer.set(x0 + x, y0 + y, (y < 7) ? cara : canto);
                }
            }
            for (int x = 2; x < TILE - 2; ++x) buffer.set(x0 + x, y0 + 3, canto);
            break;
        }
        default:
            break;
    }
}

} // namespace

Atlas generarAtlas() {
    const int ancho = COLUMNAS_ATLAS * TILE;
    const int alto = FILAS_ATLAS * TILE;
    PixelBuffer buffer(ancho, alto);

    // Una fila por capa, con sus dieciséis máscaras.
    for (int c = 0; c < static_cast<int>(Capa::CANTIDAD); ++c) {
        const Capa capa = static_cast<Capa>(c);
        for (int m = 0; m < COLUMNAS_ATLAS; ++m) {
            const int x0 = m * TILE;
            const int y0 = c * TILE;

            // La máscara cero es "esta capa no está acá": queda transparente, y
            // eso es lo que deja ver la capa de abajo.
            if (m == 0) continue;

            dibujarMascara(buffer, x0, y0, capa, m);

            Rng rng = rngFor(0x0EC0DA7ULL, "detalle:" + std::to_string(c) + ":" +
                                               std::to_string(m));
            switch (capa) {
                case Capa::Agua:
                    detalleAgua(buffer, x0, y0, m, rng);
                    break;
                case Capa::Pasto:
                    manchones(buffer, x0, y0, capa, m, rng, 3);
                    briznas(buffer, x0, y0, capa, m, rng, 4);
                    break;
                case Capa::PastoAlto:
                    briznas(buffer, x0, y0, capa, m, rng, 12);
                    break;
                case Capa::Musgo:
                    manchones(buffer, x0, y0, capa, m, rng, 5);
                    briznas(buffer, x0, y0, capa, m, rng, 3);
                    break;
                case Capa::Arena:
                    manchones(buffer, x0, y0, capa, m, rng, 2);
                    break;
                default:
                    break;
            }
        }
    }

    // La fila de objetos.
    for (int v = 0; v < VARIANTES_ARBOL; ++v) {
        dibujarArbol(buffer, v * TILE, FILA_OBJETOS * TILE, v);
    }
    for (int v = 0; v < VARIANTES_PIEDRA; ++v) {
        dibujarPiedra(buffer, (VARIANTES_ARBOL + v) * TILE, FILA_OBJETOS * TILE, v);
    }

    // Y la de tiles llanos, para los interiores.
    for (Tile t : {Tile::Piso, Tile::Pared, Tile::Alfombra, Tile::Pedestal}) {
        dibujarLlano(buffer, columnaLlana(t) * TILE, FILA_LLANOS * TILE, t);
    }

    return Atlas{ancho, alto, buffer.datos()};
}

} // namespace petbits
