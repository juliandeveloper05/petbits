#include "world_gen.h"

#include "rng.h"

#include <array>
#include <cmath>

namespace petbits {
namespace {

// ---------------------------------------------------------------------------
// Ruido de valor
// ---------------------------------------------------------------------------

/**
 * Un valor en [0, 1) para un punto de la grilla entera.
 *
 * Los tres números se mezclan y se vuelven a mezclar en vez de sumarse: sumarlos
 * haría que `(x=1, y=0)` y `(x=0, y=1)` cayeran en el mismo lugar, y el mundo
 * saldría simétrico respecto de la diagonal. Se ve enseguida y no se puede
 * arreglar después.
 *
 * El casteo a `uint32_t` antes de subir a 64 bits es lo que hace que las
 * coordenadas negativas funcionen: en C++ esa conversión está definida como
 * módulo 2^32, así que -1 entra como 0xFFFFFFFF y no como un signo perdido.
 */
double valorEn(Seed semilla, int32_t x, int32_t y, uint64_t campo) {
    const uint64_t ejes = static_cast<uint64_t>(static_cast<uint32_t>(x)) |
                          (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32);

    uint64_t h = splitmix64(semilla ^ (campo * 0x9E3779B97F4A7C15ULL));
    h = splitmix64(h ^ ejes);
    h = splitmix64(h);

    // 2^53 es donde un double deja de poder representar todos los enteros. Se
    // usan los 53 bits altos y no los bajos: los bajos de un hash son los que
    // peor se mezclan.
    return static_cast<double>(h >> 11) / 9007199254740992.0;
}

/** La curva de Hermite. Sin ella el ruido se ve como una grilla de rombos. */
double suavizar(double t) { return t * t * (3.0 - 2.0 * t); }

double interpolar(double a, double b, double t) { return a + (b - a) * t; }

/** Ruido de valor: se hashean las cuatro esquinas de la celda y se interpola. */
double ruido(Seed semilla, double x, double y, uint64_t campo) {
    const double px = std::floor(x);
    const double py = std::floor(y);
    const int32_t ix = static_cast<int32_t>(px);
    const int32_t iy = static_cast<int32_t>(py);

    const double fx = suavizar(x - px);
    const double fy = suavizar(y - py);

    const double a = valorEn(semilla, ix, iy, campo);
    const double b = valorEn(semilla, ix + 1, iy, campo);
    const double c = valorEn(semilla, ix, iy + 1, campo);
    const double d = valorEn(semilla, ix + 1, iy + 1, campo);

    return interpolar(interpolar(a, b, fx), interpolar(c, d, fx), fy);
}

/**
 * Varias octavas encimadas: formas grandes con detalle chico adentro.
 *
 * Con una sola octava el terreno queda blando, como manchas de acuarela. Con
 * cuatro aparecen las costas irregulares y los claros en el bosque, que es lo
 * que hace que un mundo parezca hecho y no promediado.
 *
 * El resultado se normaliza contra la suma de amplitudes, así que siempre cae en
 * [0, 1) y los umbrales de bioma de más abajo significan lo mismo aunque se
 * cambie la cantidad de octavas.
 */
double fbm(Seed semilla, double x, double y, uint64_t campo, int octavas) {
    double total = 0.0;
    double amplitud = 1.0;
    double frecuencia = 1.0;
    double suma = 0.0;

    for (int i = 0; i < octavas; ++i) {
        // Cada octava usa su propio campo: si compartieran hash, las octavas
        // estarían correlacionadas y encimarlas no agregaría detalle, solo
        // contraste.
        total += ruido(semilla, x * frecuencia, y * frecuencia, campo + static_cast<uint64_t>(i) * 7919ULL) *
                 amplitud;
        suma += amplitud;
        amplitud *= 0.5;
        frecuencia *= 2.0;
    }
    return total / suma;
}

/**
 * Estira el rango alrededor del medio.
 *
 * Promediar octavas concentra el resultado cerca de 0.5 —es el teorema central
 * del límite haciendo su trabajo— y eso arruina los umbrales: la primera versión
 * de este generador daba 56% de pasto y CERO piedra, porque `altura > 0.795`
 * prácticamente no ocurría nunca.
 *
 * No se arregla moviendo los umbrales hacia el centro: eso los vuelve
 * hipersensibles, y mover uno un uno por ciento cambia el mundo entero. Se
 * arregla devolviéndole al ruido el rango que el promedio le sacó.
 */
double contraste(double v, double k) {
    const double estirado = 0.5 + (v - 0.5) * k;
    return estirado < 0.0 ? 0.0 : (estirado > 1.0 ? 1.0 : estirado);
}

// ---------------------------------------------------------------------------
// Las escalas
// ---------------------------------------------------------------------------
//
// Cuántos tiles mide una forma del terreno. Son los números que deciden si el
// mundo se siente grande o se siente ruidoso, y los únicos que conviene tocar
// mirando el PNG en vez de razonando.

/** Los lagos y las mesetas: formas de unos ochenta tiles, dos chunks y medio. */
constexpr double ESCALA_ALTURA = 84.0;

/** Los bosques y los pastizales: más chicos, para que quepan varios adentro. */
constexpr double ESCALA_HUMEDAD = 72.0;

/**
 * El moteado que decide qué árbol de un bosque existe y cuál no.
 *
 * Empezó en 6.5 y el mundo se leía como sopa de manchitas: cada bioma picoteado
 * de tiles sueltos, sin arboledas ni claros con forma. A 9.5 el detalle sigue
 * rompiendo la uniformidad pero se agrupa, que es lo que hace que un bosque
 * parezca un bosque y no una textura.
 */
constexpr double ESCALA_DETALLE = 9.5;

// Identificadores de campo. Cualquier par de números distintos sirve; estos
// están separados a propósito para que la mezcla no los acerque.
constexpr uint64_t CAMPO_ALTURA = 0x1A17D1AULL;
constexpr uint64_t CAMPO_HUMEDAD = 0xB0DEC0DEULL;
constexpr uint64_t CAMPO_DETALLE = 0xD37A11EULL;
constexpr uint64_t CAMPO_HITO = 0x81707ULL;

// ---------------------------------------------------------------------------
// Tierra firme alrededor del origen
// ---------------------------------------------------------------------------
//
// El pueblo va en el (0, 0), y el ruido no sabe eso. Al mirar las primeras tres
// semillas renderizadas, DOS tenían el origen adentro de un lago: el pueblo
// habría quedado bajo el agua, o —peor— sería una isla rodeada de un tile sólido
// por los cuatro costados. Ningún test lo dijo, porque los cuatro preguntan por
// el mundo en general y ninguno por ese punto en particular.
//
// Se arregla levantando el terreno cerca del origen, con una campana que se
// apaga sola. No es un parche puesto encima: es la misma idea que usan los
// generadores de islas, al revés. Lejos del pueblo el mundo es el que el ruido
// quiera; cerca, hay suelo.

/** Hasta dónde llega la mano. Más allá de esto el ruido manda solo. */
constexpr double RADIO_TIERRA_FIRME = 150.0;

/** Cuánto se levanta el terreno justo en el origen. */
constexpr double FUERZA_TIERRA_FIRME = 0.30;

double tierraFirme(double x, double y) {
    const double d = std::sqrt(x * x + y * y) / RADIO_TIERRA_FIRME;
    if (d >= 1.0) return 0.0;
    // Coseno elevado: llega a cero con pendiente cero, así que no se ve el borde
    // del círculo. Con una rampa lineal aparecería un anillo en el terreno.
    const double t = 0.5 + 0.5 * std::cos(d * 3.14159265358979323846);
    return FUERZA_TIERRA_FIRME * t * t;
}

// ---------------------------------------------------------------------------
// Los umbrales
// ---------------------------------------------------------------------------
//
// Están calibrados para que ningún bioma se coma el mundo y para que quede
// caminable. El agua y la piedra son sólidas: si se pasan, el mundo se parte en
// islas y el test de caminabilidad falla. Ese test es la red, pero estos números
// son la razón por la que no hace falta usarla.

constexpr double NIVEL_AGUA = 0.285;
constexpr double NIVEL_COSTA = 0.340;
constexpr double NIVEL_ROCA = 0.800;

constexpr double HUMEDAD_BOSQUE = 0.620;
constexpr double HUMEDAD_PASTIZAL = 0.480;
constexpr double HUMEDAD_HUMEDAL = 0.330;

/**
 * Cuánto se estira cada campo.
 *
 * La altura más que la humedad: la altura decide agua y roca, que son los dos
 * extremos, y sin estirar no llegaban nunca. La humedad reparte entre biomas
 * vecinos y no necesita tanto.
 */
constexpr double CONTRASTE_ALTURA = 1.85;
constexpr double CONTRASTE_HUMEDAD = 1.55;

/**
 * Cuánto de un bosque es realmente árbol.
 *
 * Este número NO se eligió a ojo, y ahí está lo interesante. La primera versión
 * puso 0.56 razonando que "poco más de la mitad" dejaría senderos naturales, y
 * el test de caminabilidad contestó que desde el centro se llegaba al 17% del
 * suelo: el mundo estaba partido en pedazos.
 *
 * Hay una constante que lo explica. En una grilla cuadrada, un conjunto de
 * celdas ocupadas al azar deja pasar de un lado al otro solo si las LIBRES pasan
 * de alrededor del 59,3% — el umbral de percolación de sitios. Con 56% de
 * árboles, los claros son el 44%: por debajo del umbral, así que no se conectan
 * entre sí. No importa cuántas veces se regenere ni con qué semilla; es una
 * propiedad de la grilla, no de este código.
 *
 * Con 34% de árboles los claros quedan en 66%, holgadamente por encima, y el
 * bosque se sigue leyendo como bosque. La misma cuenta vale para el roquedal.
 */
constexpr double DENSIDAD_ARBOLES = 0.34;

} // namespace

// ---------------------------------------------------------------------------
// El pueblo
// ---------------------------------------------------------------------------

namespace {

/** Los cinco puntos del pueblo, en coordenadas locales. Van marcados en piedra. */
constexpr int PUNTOS_PUEBLO[5][2] = {{4, 3}, {25, 3}, {25, 13}, {4, 13}, {15, 2}};

/** Traza un camino en L, primero horizontal y después vertical. */
void trazarCamino(uint8_t* g, int x0, int y0, int x1, int y1) {
    const int desdeX = x0 < x1 ? x0 : x1;
    const int hastaX = x0 < x1 ? x1 : x0;
    for (int x = desdeX; x <= hastaX; ++x) {
        if (x > 0 && x < PUEBLO_ANCHO - 1 && y0 > 0 && y0 < PUEBLO_ALTO - 1) {
            g[y0 * PUEBLO_ANCHO + x] = static_cast<uint8_t>(Tile::Camino);
        }
    }
    const int desdeY = y0 < y1 ? y0 : y1;
    const int hastaY = y0 < y1 ? y1 : y0;
    for (int y = desdeY; y <= hastaY; ++y) {
        if (x1 > 0 && x1 < PUEBLO_ANCHO - 1 && y > 0 && y < PUEBLO_ALTO - 1) {
            g[y * PUEBLO_ANCHO + x1] = static_cast<uint8_t>(Tile::Camino);
        }
    }
}

/**
 * La grilla del pueblo, armada una sola vez.
 *
 * A mano y no procedural: es el lugar donde el jugador arranca y tiene que
 * leerse como un sitio pensado, no como ruido. Los caminos van de la plaza a
 * cada entrada, que es lo que enseña sin decir nada que esas son las salidas.
 */
const std::array<uint8_t, PUEBLO_ANCHO * PUEBLO_ALTO>& grillaPueblo() {
    static const std::array<uint8_t, PUEBLO_ANCHO * PUEBLO_ALTO> g = [] {
        std::array<uint8_t, PUEBLO_ANCHO * PUEBLO_ALTO> m{};
        m.fill(static_cast<uint8_t>(Tile::Pasto));

        // El borde de árboles, CON HUECOS.
        //
        // Antes encerraba el pueblo por completo, y tenía sentido cuando el mapa
        // era todo lo que había: era la pared sin necesidad de paredes
        // invisibles. Ahora hay mundo del otro lado, y un pueblo amurallado
        // convertiría el mundo infinito en un fondo de pantalla.
        //
        // Los huecos van en el medio de cada lado y miden cinco tiles: menos se
        // lee como un error del terreno, más deja de leerse como una salida.
        const int medioX = PUEBLO_ANCHO / 2;
        const int medioY = PUEBLO_ALTO / 2;
        for (int x = 0; x < PUEBLO_ANCHO; ++x) {
            const bool hueco = (x >= medioX - 2 && x <= medioX + 2);
            if (!hueco) {
                m[0 * PUEBLO_ANCHO + x] = static_cast<uint8_t>(Tile::Arbol);
                m[(PUEBLO_ALTO - 1) * PUEBLO_ANCHO + x] = static_cast<uint8_t>(Tile::Arbol);
            }
        }
        for (int y = 0; y < PUEBLO_ALTO; ++y) {
            const bool hueco = (y >= medioY - 2 && y <= medioY + 2);
            if (!hueco) {
                m[y * PUEBLO_ANCHO + 0] = static_cast<uint8_t>(Tile::Arbol);
                m[y * PUEBLO_ANCHO + (PUEBLO_ANCHO - 1)] = static_cast<uint8_t>(Tile::Arbol);
            }
        }

        // La plaza, en el centro.
        for (int y = 7; y < 11; ++y) {
            for (int x = 12; x < 19; ++x) {
                m[y * PUEBLO_ANCHO + x] = static_cast<uint8_t>(Tile::Camino);
            }
        }

        // Un caminito de la plaza a cada entrada.
        for (const auto& p : PUNTOS_PUEBLO) {
            trazarCamino(m.data(), 15, 8, p[0], p[1]);
        }

        // Y cuatro caminos más, de la plaza a cada hueco del borde: si no, las
        // salidas serían agujeros en el bosque y no puertas.
        trazarCamino(m.data(), 15, 8, medioX, 1);
        trazarCamino(m.data(), 15, 8, medioX, PUEBLO_ALTO - 2);
        trazarCamino(m.data(), 15, 8, 1, medioY);
        trazarCamino(m.data(), 15, 8, PUEBLO_ANCHO - 2, medioY);

        // Un estanque, para que no sea todo verde y camino.
        for (int y = 11; y < 15; ++y) {
            for (int x = 19; x < 24; ++x) {
                m[y * PUEBLO_ANCHO + x] = static_cast<uint8_t>(Tile::Agua);
            }
        }
        for (int x = 19; x < 24; ++x) {
            m[10 * PUEBLO_ANCHO + x] = static_cast<uint8_t>(Tile::Arena);
        }

        // Pasto alto en dos manchones.
        constexpr int MANCHONES[7][2] = {{7, 4}, {8, 4}, {7, 5}, {21, 4}, {22, 4}, {21, 5}, {22, 5}};
        for (const auto& p : MANCHONES) {
            m[p[1] * PUEBLO_ANCHO + p[0]] = static_cast<uint8_t>(Tile::PastoAlto);
        }

        // Y las entradas, marcadas en piedra.
        for (const auto& p : PUNTOS_PUEBLO) {
            m[p[1] * PUEBLO_ANCHO + p[0]] = static_cast<uint8_t>(Tile::Piedra);
        }

        return m;
    }();
    return g;
}

} // namespace

bool enElPueblo(int32_t x, int32_t y) {
    return x >= PUEBLO_X && x < PUEBLO_X + PUEBLO_ANCHO && y >= PUEBLO_Y &&
           y < PUEBLO_Y + PUEBLO_ALTO;
}

namespace {
/** Cuánto se adentra cada senda en el mundo, y cuánto mide de ancho. */
constexpr int32_t LARGO_SENDA = 10;
constexpr int32_t MEDIO_SENDA = 1;
} // namespace

bool enUnaSenda(int32_t x, int32_t y) {
    const int32_t cx = PUEBLO_X + PUEBLO_ANCHO / 2;
    const int32_t cy = PUEBLO_Y + PUEBLO_ALTO / 2;

    // Norte y sur: una franja vertical alineada con el hueco de arriba y el de
    // abajo. Este y oeste, lo mismo en horizontal.
    if (x >= cx - MEDIO_SENDA && x <= cx + MEDIO_SENDA) {
        if (y < PUEBLO_Y && y >= PUEBLO_Y - LARGO_SENDA) return true;
        if (y >= PUEBLO_Y + PUEBLO_ALTO && y < PUEBLO_Y + PUEBLO_ALTO + LARGO_SENDA) return true;
    }
    if (y >= cy - MEDIO_SENDA && y <= cy + MEDIO_SENDA) {
        if (x < PUEBLO_X && x >= PUEBLO_X - LARGO_SENDA) return true;
        if (x >= PUEBLO_X + PUEBLO_ANCHO && x < PUEBLO_X + PUEBLO_ANCHO + LARGO_SENDA) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------

std::string_view nombreBioma(Bioma b) {
    switch (b) {
        case Bioma::Lago:     return "el agua";
        case Bioma::Costa:    return "la orilla";
        case Bioma::Pradera:  return "la pradera";
        case Bioma::Bosque:   return "el bosque";
        case Bioma::Humedal:  return "el humedal";
        case Bioma::Roquedal: return "el roquedal";
        case Bioma::CANTIDAD: break;
    }
    return "algún lado";
}

Bioma biomaEn(Seed semilla, int32_t x, int32_t y) {
    const double fx = static_cast<double>(x);
    const double fy = static_cast<double>(y);

    const double cerca = tierraFirme(fx, fy);
    double altura = contraste(fbm(semilla, fx / ESCALA_ALTURA, fy / ESCALA_ALTURA, CAMPO_ALTURA, 4),
                              CONTRASTE_ALTURA) +
                    cerca;

    // Levantar el terreno saca al pueblo del agua y lo mete en el roquedal: la
    // primera versión de este arreglo dejó una de las tres semillas con el
    // origen sobre un pedregal, que es sólido y tampoco se camina. El techo es
    // la otra mitad del piso.
    //
    // Solo se acota adentro del radio. Más allá el mundo puede tener todas las
    // montañas que quiera — la garantía es sobre el barrio del jugador, no sobre
    // el mundo.
    if (cerca > 0.0) {
        const double techo = NIVEL_ROCA - 0.02;
        if (altura > techo) altura = techo;
    }

    if (altura < NIVEL_AGUA) return Bioma::Lago;
    if (altura < NIVEL_COSTA) return Bioma::Costa;
    if (altura > NIVEL_ROCA) return Bioma::Roquedal;

    const double humedad =
        contraste(fbm(semilla, fx / ESCALA_HUMEDAD, fy / ESCALA_HUMEDAD, CAMPO_HUMEDAD, 3),
                  CONTRASTE_HUMEDAD);
    if (humedad > HUMEDAD_BOSQUE) return Bioma::Bosque;
    if (humedad > HUMEDAD_PASTIZAL) return Bioma::Pradera;
    if (humedad > HUMEDAD_HUMEDAL) return Bioma::Pradera;
    return Bioma::Humedal;
}

Tile tileEnMundo(Seed semilla, int32_t x, int32_t y) {
    // El pueblo primero. No es un caso especial que el walker tenga que
    // recordar: acá adentro, el mundo simplemente es el pueblo.
    if (enElPueblo(x, y)) {
        const int lx = x - PUEBLO_X;
        const int ly = y - PUEBLO_Y;
        return static_cast<Tile>(grillaPueblo()[ly * PUEBLO_ANCHO + lx]);
    }

    // Las sendas: diez tiles saliendo de cada hueco del borde del pueblo.
    //
    // Solo aparecen cuando hacen falta. Si del otro lado del hueco hay pradera,
    // la senda no existe y salís al pasto — que es como tiene que ser. Se dibuja
    // únicamente cuando el mundo puso algo sólido en la puerta, y ahí hace lo que
    // haría cualquiera que viviera ahí: un camino.
    if (enUnaSenda(x, y)) {
        const Bioma b = biomaEn(semilla, x, y);
        if (b == Bioma::Lago || b == Bioma::Roquedal || b == Bioma::Bosque) {
            return Tile::Camino;
        }
    }

    const double fx = static_cast<double>(x);
    const double fy = static_cast<double>(y);

    switch (biomaEn(semilla, x, y)) {
        case Bioma::Lago:
            return Tile::Agua;

        case Bioma::Costa:
            return Tile::Arena;

        case Bioma::Roquedal: {
            // Ni siquiera un roquedal es piedra maciza: se le abren pasillos con
            // el mismo moteado que aclara los bosques. Un macizo de tiles
            // sólidos es lo que encierra un mundo.
            const double detalle =
                ruido(semilla, fx / ESCALA_DETALLE, fy / ESCALA_DETALLE, CAMPO_DETALLE);
            // Mismo razonamiento que con los árboles: por encima del 40% de
            // piedra el roquedal deja de tener pasillos y se vuelve un tapón.
            return detalle > 0.62 ? Tile::Piedra : Tile::Musgo;
        }

        case Bioma::Bosque: {
            const double detalle =
                ruido(semilla, fx / ESCALA_DETALLE, fy / ESCALA_DETALLE, CAMPO_DETALLE);
            if (detalle > 1.0 - DENSIDAD_ARBOLES) return Tile::Arbol;
            // Lo que no es árbol es sotobosque, no césped de jardín.
            return detalle > 0.22 ? Tile::PastoAlto : Tile::Pasto;
        }

        case Bioma::Humedal: {
            const double detalle =
                ruido(semilla, fx / ESCALA_DETALLE, fy / ESCALA_DETALLE, CAMPO_DETALLE);
            return detalle > 0.5 ? Tile::Musgo : Tile::Pasto;
        }

        case Bioma::Pradera:
        case Bioma::CANTIDAD:
            break;
    }

    // La pradera: pasto con manchones de pasto alto, para que no sea una
    // alfombra de un solo tile.
    const double detalle = ruido(semilla, fx / ESCALA_DETALLE, fy / ESCALA_DETALLE, CAMPO_DETALLE);
    return detalle > 0.52 ? Tile::PastoAlto : Tile::Pasto;
}

// ---------------------------------------------------------------------------
// Qué hay para encontrar
// ---------------------------------------------------------------------------

namespace {
/**
 * Cada cuántos tiles, más o menos, hay un hito.
 *
 * Uno cada dos mil suena a poquísimo y en la práctica es uno cada cuarenta y
 * cinco tiles de lado — o sea, más o menos uno por pantalla y media. Si fueran
 * más, dejarían de ser un hallazgo; si fueran menos, nadie encontraría ninguno
 * y la mecánica no existiría.
 */
constexpr double RAREZA_HITO = 0.9995;
} // namespace

Hallazgo hallazgoEn(Seed semilla, int32_t x, int32_t y) {
    // En el pueblo no hay nada que juntar. Es el lugar seguro, y llenarlo de
    // recursos convertiría el mundo en un rodeo: darías vueltas en la plaza.
    if (enElPueblo(x, y)) return Hallazgo::Nada;

    const Tile t = tileEnMundo(semilla, x, y);

    // El hito va primero: es más raro, y un hito sobre pasto alto tiene que
    // leerse como hito.
    if (!esSolido(t) && valorEn(semilla, x, y, CAMPO_HITO) > RAREZA_HITO) {
        return Hallazgo::Hito;
    }

    if (t == Tile::PastoAlto) return Hallazgo::Forraje;

    // La veta no está EN la piedra —la piedra es sólida, no se puede pisar— sino
    // en el musgo que la rodea, que es por donde se camina en un roquedal.
    if (t == Tile::Musgo && biomaEn(semilla, x, y) == Bioma::Roquedal) return Hallazgo::Veta;

    return Hallazgo::Nada;
}

Seed semillaDeHito(Seed semilla, int32_t x, int32_t y) {
    // Se mezcla la posición con la semilla del mundo y se vuelve a mezclar. Sin
    // la segunda pasada, hitos vecinos darían genomas parecidos y todas las
    // criaturas de una zona saldrían del mismo linaje.
    const uint64_t ejes = static_cast<uint64_t>(static_cast<uint32_t>(x)) |
                          (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32);
    return splitmix64(splitmix64(semilla ^ 0x5EEDULL) ^ ejes);
}

Chunk generarChunk(Seed semilla, int32_t cx, int32_t cy) {
    Chunk chunk;
    chunk.cx = cx;
    chunk.cy = cy;

    // Sin ningún estado entre tile y tile. Es literalmente `tileEnMundo` mil
    // veinticuatro veces, y eso es lo que garantiza que las costuras cierren.
    for (int y = 0; y < CHUNK; ++y) {
        for (int x = 0; x < CHUNK; ++x) {
            const int32_t wx = cx * CHUNK + x;
            const int32_t wy = cy * CHUNK + y;
            chunk.tiles[static_cast<size_t>(y) * CHUNK + static_cast<size_t>(x)] =
                static_cast<uint8_t>(tileEnMundo(semilla, wx, wy));
        }
    }
    return chunk;
}

int32_t chunkDe(int32_t coordenada) {
    // División hacia abajo. `-1 / 32` da 0 en C++ porque la división entera
    // trunca hacia el cero, y eso haría que el chunk 0 midiera 63 tiles de ancho
    // en vez de 32, con el mundo partido justo en el origen.
    return (coordenada >= 0) ? (coordenada / CHUNK) : -(((-coordenada) + CHUNK - 1) / CHUNK);
}

int32_t dentroDelChunk(int32_t coordenada) {
    const int32_t r = coordenada % CHUNK;
    return (r < 0) ? r + CHUNK : r;
}

} // namespace petbits
