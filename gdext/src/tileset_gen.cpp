/**
 * tileset_gen.cpp — ver tileset_gen.h.
 *
 * Cada tile se dibuja con la misma receta: un color base, un moteado
 * determinista para que no quede plano, y algún detalle propio. El moteado sale
 * de un PRNG sembrado con el índice del tile, así que el atlas es idéntico en
 * cada corrida y en cada máquina.
 *
 * Sin ese moteado, doce tiles de pasto seguidos se leen como una alfombra de un
 * solo color. Con él, la textura aparece a pesar de que todos los píxeles salen
 * de tres tonos.
 */

#include "tileset_gen.h"
#include "palette.h"
#include "rng.h"

#include <algorithm>

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

/**
 * La receta de cada tile, en OKLCH.
 *
 * Se declaran tono, luminosidad y croma en vez de RGB para que todo el mundo
 * comparta el espacio de color de las criaturas. Un verde de pasto elegido a
 * ojo en RGB no tiene por qué convivir con un cuerpo generado en OKLCH; elegido
 * acá, sí.
 */
struct Receta {
    double hue;
    double luz;
    double croma;
    /** Cuánto varía la luz en el moteado. */
    double variacion;
};

static constexpr Receta RECETAS[static_cast<size_t>(Tile::CANTIDAD)] = {
    /* Pasto     */ {138, 0.62, 0.075, 0.045},
    /* Camino    */ {68, 0.70, 0.055, 0.035},
    /* Agua      */ {232, 0.58, 0.105, 0.040},
    /* Piedra    */ {250, 0.55, 0.014, 0.050},
    /* Arbol     */ {146, 0.44, 0.090, 0.055},
    /* PastoAlto */ {130, 0.55, 0.095, 0.050},
    /* Arena     */ {80, 0.80, 0.060, 0.030},
    /* Musgo     */ {158, 0.52, 0.070, 0.045},
    // Los interiores son más cálidos y menos saturados que el exterior: la
    // madera contra el pasto es lo que hace que entrar a un lugar se sienta
    // como entrar a un lugar, sin necesidad de una transición.
    /* Piso      */ {58, 0.64, 0.038, 0.022},
    /* Pared     */ {40, 0.33, 0.030, 0.020},
    /* Alfombra  */ {28, 0.48, 0.085, 0.030},
    /* Pedestal  */ {250, 0.62, 0.012, 0.035},
};

/** Pinta el tile entero con el color base y su moteado. */
static void pintarBase(PixelBuffer& buffer, int x0, const Receta& r, Rng& rng) {
    for (int y = 0; y < TILE; ++y) {
        for (int x = 0; x < TILE; ++x) {
            // Tres niveles y no un ruido continuo: el pixel art se lee mejor con
            // pocos tonos definidos que con un degradado suave, que a 16×16
            // termina pareciendo suciedad.
            const int nivel = static_cast<int>(rng.intMenorQue(3)) - 1;
            const double luz = r.luz + nivel * r.variacion;
            buffer.set(x0 + x, y, oklchToRgb(luz, r.croma, r.hue));
        }
    }
}

/** Unas briznas más oscuras, para que el pasto no sea una alfombra. */
static void briznas(PixelBuffer& buffer, int x0, const Receta& r, Rng& rng, int cuantas) {
    const Rgb oscuro = oklchToRgb(r.luz - r.variacion * 2.2, r.croma * 1.2, r.hue - 4);
    for (int i = 0; i < cuantas; ++i) {
        const int x = static_cast<int>(rng.intMenorQue(TILE));
        const int y = static_cast<int>(rng.intMenorQue(TILE - 2));
        buffer.set(x0 + x, y, oscuro);
        buffer.set(x0 + x, y + 1, oscuro);
    }
}

static void dibujarTile(PixelBuffer& buffer, Tile t) {
    const size_t i = static_cast<size_t>(t);
    const Receta& r = RECETAS[i];

    // Sembrado por índice de tile: el atlas sale igual en cada corrida, y
    // agregar un tile al final no cambia los que ya estaban.
    Rng rng = rngFor(0xB17B175ULL, "tile:" + std::to_string(i));

    const int x0 = static_cast<int>(i) * TILE;
    pintarBase(buffer, x0, r, rng);

    switch (t) {
        case Tile::Pasto:
            briznas(buffer, x0, r, rng, 5);
            break;

        case Tile::PastoAlto:
            briznas(buffer, x0, r, rng, 14);
            break;

        case Tile::Musgo:
            briznas(buffer, x0, r, rng, 9);
            break;

        case Tile::Agua: {
            // Dos rayas horizontales claras: leen como reflejo sin necesidad de
            // animar nada.
            const Rgb brillo = oklchToRgb(r.luz + 0.16, r.croma * 0.6, r.hue + 6);
            for (int y : {4, 11}) {
                const int desde = static_cast<int>(rng.intMenorQue(5));
                const int largo = 5 + static_cast<int>(rng.intMenorQue(5));
                for (int x = desde; x < std::min(TILE, desde + largo); ++x) {
                    buffer.set(x0 + x, y, brillo);
                }
            }
            break;
        }

        case Tile::Piedra: {
            // Juntas de mampostería, corridas entre hiladas.
            const Rgb junta = oklchToRgb(r.luz - 0.14, r.croma, r.hue);
            for (int x = 0; x < TILE; ++x) {
                buffer.set(x0 + x, 7, junta);
                buffer.set(x0 + x, 15, junta);
            }
            for (int y = 0; y < 8; ++y) buffer.set(x0 + 5, y, junta);
            for (int y = 8; y < 16; ++y) buffer.set(x0 + 11, y, junta);
            break;
        }

        case Tile::Arbol: {
            // Copa redonda con un tronco asomando abajo. El tile es sólido: no
            // se camina por encima.
            const Rgb copa = oklchToRgb(r.luz + 0.06, r.croma, r.hue + 3);
            const Rgb sombra = oklchToRgb(r.luz - 0.10, r.croma, r.hue - 6);
            const Rgb tronco = oklchToRgb(0.42, 0.055, 62);
            for (int y = 0; y < TILE; ++y) {
                for (int x = 0; x < TILE; ++x) {
                    const double dx = (x - 7.5) / 7.0;
                    const double dy = (y - 6.5) / 6.5;
                    if (dx * dx + dy * dy > 1.0) continue;
                    // La luz cae de arriba a la izquierda, igual que en los
                    // sprites de las criaturas. Que las dos cosas se iluminen
                    // desde el mismo lado es lo que hace que convivan.
                    buffer.set(x0 + x, y, (dx + dy < -0.35) ? copa : sombra);
                }
            }
            for (int y = 12; y < TILE; ++y) {
                buffer.set(x0 + 7, y, tronco);
                buffer.set(x0 + 8, y, tronco);
            }
            break;
        }

        case Tile::Piso: {
            // Tablas horizontales, y NADA de juntas verticales.
            //
            // La primera versión las tenía, corridas entre hiladas para que no
            // se leyera como una grilla — y el efecto fue el contrario: con las
            // verticales el piso se leía como mampostería, igual que la pared, y
            // adentro de una sala no se distinguía por dónde se podía caminar.
            // Dos líneas horizontales suaves alcanzan para que haya textura sin
            // que compita con la alfombra ni con los muebles.
            const Rgb junta = oklchToRgb(r.luz - 0.055, r.croma, r.hue - 4);
            for (int x = 0; x < TILE; ++x) {
                buffer.set(x0 + x, 5, junta);
                buffer.set(x0 + x, 11, junta);
            }
            break;
        }

        case Tile::Pared: {
            // Un zócalo claro arriba: sin él, una pared oscura al lado de otra
            // se lee como un agujero y no como un muro.
            const Rgb luz = oklchToRgb(r.luz + 0.14, r.croma * 0.8, r.hue + 4);
            const Rgb junta = oklchToRgb(r.luz - 0.12, r.croma, r.hue);
            for (int x = 0; x < TILE; ++x) {
                buffer.set(x0 + x, 0, luz);
                buffer.set(x0 + x, 1, luz);
                buffer.set(x0 + x, 8, junta);
            }
            for (int y = 2; y < 8; ++y) buffer.set(x0 + 6, y, junta);
            for (int y = 9; y < TILE; ++y) buffer.set(x0 + 12, y, junta);
            break;
        }

        case Tile::Alfombra: {
            // Borde en dos tonos. Como se pone en tiras, el borde marca el
            // recorrido y el centro queda liso.
            const Rgb borde = oklchToRgb(r.luz - 0.14, r.croma * 1.1, r.hue - 8);
            const Rgb hilo = oklchToRgb(r.luz + 0.12, r.croma * 0.7, r.hue + 10);
            for (int i2 = 0; i2 < TILE; ++i2) {
                buffer.set(x0 + i2, 0, borde);
                buffer.set(x0 + i2, TILE - 1, borde);
                buffer.set(x0 + i2, 2, hilo);
                buffer.set(x0 + i2, TILE - 3, hilo);
            }
            break;
        }

        case Tile::Pedestal: {
            // Un bloque con la cara superior clara. Es sólido: la criatura se
            // para al lado, no encima.
            const Rgb cara = oklchToRgb(r.luz + 0.16, r.croma, r.hue);
            const Rgb canto = oklchToRgb(r.luz - 0.18, r.croma, r.hue);
            for (int y = 3; y < TILE - 1; ++y) {
                for (int x = 2; x < TILE - 2; ++x) {
                    buffer.set(x0 + x, y, (y < 7) ? cara : canto);
                }
            }
            for (int x = 2; x < TILE - 2; ++x) buffer.set(x0 + x, 3, canto);
            break;
        }

        default:
            break;
    }
}

Atlas generarAtlas() {
    const int cantidad = static_cast<int>(Tile::CANTIDAD);
    PixelBuffer buffer(cantidad * TILE, TILE);

    for (int i = 0; i < cantidad; ++i) {
        dibujarTile(buffer, static_cast<Tile>(i));
    }

    return Atlas{cantidad * TILE, TILE, buffer.datos()};
}

} // namespace petbits
