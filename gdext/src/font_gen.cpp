#include "font_gen.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace petbits {
namespace {

/**
 * Un glifo tal como se escribe: su código, en qué fila de la caja empieza, y el
 * dibujo en filas de cinco caracteres donde '#' es tinta.
 *
 * Escribirlo así y no como números hexadecimales es a propósito. Un bitmap font
 * es lo más parecido a un dibujo que hay en este repo, y el diff de un cambio
 * tiene que dejar ver qué se movió: `0x1F` no dice nada, "#####" sí.
 */
struct Trazo {
    char32_t codigo;
    int fila;
    std::vector<const char*> arte;
};

/**
 * Las filas de referencia. Cambiar una acá desalinea la fuente entera.
 *
 * ALTA es donde empiezan las mayúsculas, los números y las minúsculas con
 * ascendente (b, d, f, h, k, l, t). BAJA es donde empieza la altura de x.
 */
constexpr int ALTA = 2;
constexpr int BAJA = 4;

// ---------------------------------------------------------------------------
// Las marcas diacríticas
// ---------------------------------------------------------------------------
// Se estampan dos filas por encima de donde arranca el cuerpo de la letra, así
// que la misma tilde sirve para la `á` (cuerpo en la fila 4) y para la `Á`
// (cuerpo en la fila 2) sin escribirla dos veces.

const std::vector<const char*> TILDE_AGUDA = {"...#.", "..#.."};
const std::vector<const char*> TILDE_ENIE = {".##.#", "#..#."};
const std::vector<const char*> DIERESIS = {".....", ".#.#."};
const std::vector<const char*> CIRCUNFLEJO = {"..#..", ".#.#."};

// ---------------------------------------------------------------------------
// El alfabeto
// ---------------------------------------------------------------------------

const std::vector<Trazo>& trazos() {
    static const std::vector<Trazo> t = {
        // -- espacio y puntuación --------------------------------------------
        {U' ', ALTA, {"....."}},
        {U'!', ALTA, {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
        {U'"', ALTA, {".#.#.", ".#.#."}},
        {U'#', 3, {".#.#.", "#####", ".#.#.", "#####", ".#.#."}},
        {U'$', ALTA, {"..#..", ".####", "#.#..", ".###.", "..#.#", "####.", "..#.."}},
        {U'%', ALTA, {"##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##"}},
        {U'&', ALTA, {".##..", "#..#.", "#..#.", ".##..", "#.#.#", "#..#.", ".##.#"}},
        {U'\'', ALTA, {"..#..", "..#.."}},
        {U'(', ALTA, {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
        {U')', ALTA, {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
        {U'*', 3, {"..#..", "#.#.#", ".###.", "#.#.#", "..#.."}},
        {U'+', BAJA, {"..#..", "..#..", "#####", "..#..", "..#.."}},
        {U',', 8, {"..#..", "..#..", ".#..."}},
        {U'-', 6, {".###."}},
        {U'.', 7, {".##..", ".##.."}},
        {U'/', ALTA, {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},

        // -- números ---------------------------------------------------------
        {U'0', ALTA, {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
        {U'1', ALTA, {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
        {U'2', ALTA, {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
        {U'3', ALTA, {"#####", "...#.", "..##.", "....#", "....#", "#...#", ".###."}},
        {U'4', ALTA, {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
        {U'5', ALTA, {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
        {U'6', ALTA, {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
        {U'7', ALTA, {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
        {U'8', ALTA, {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
        {U'9', ALTA, {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},

        {U':', BAJA, {"..#..", ".....", ".....", ".....", "..#.."}},
        {U';', BAJA, {"..#..", ".....", ".....", ".....", "..#..", "..#..", ".#..."}},
        {U'<', 3, {"...#.", "..#..", ".#...", "..#..", "...#."}},
        {U'=', BAJA, {".....", "#####", ".....", "#####", "....."}},
        {U'>', 3, {".#...", "..#..", "...#.", "..#..", ".#..."}},
        {U'?', ALTA, {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
        {U'@', ALTA, {".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".###."}},

        // -- mayúsculas ------------------------------------------------------
        {U'A', ALTA, {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
        {U'B', ALTA, {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
        {U'C', ALTA, {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
        {U'D', ALTA, {"###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."}},
        {U'E', ALTA, {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
        {U'F', ALTA, {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
        {U'G', ALTA, {".###.", "#...#", "#....", "#..##", "#...#", "#...#", ".####"}},
        {U'H', ALTA, {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
        {U'I', ALTA, {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
        {U'J', ALTA, {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
        {U'K', ALTA, {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
        {U'L', ALTA, {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
        {U'M', ALTA, {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
        {U'N', ALTA, {"#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#"}},
        {U'O', ALTA, {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
        {U'P', ALTA, {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
        {U'Q', ALTA, {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
        {U'R', ALTA, {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
        {U'S', ALTA, {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
        {U'T', ALTA, {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
        {U'U', ALTA, {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
        {U'V', ALTA, {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
        {U'W', ALTA, {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"}},
        {U'X', ALTA, {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
        {U'Y', ALTA, {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
        {U'Z', ALTA, {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},

        {U'[', ALTA, {".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."}},
        {U'\\', ALTA, {"#....", "#....", ".#...", "..#..", "...#.", "....#", "....#"}},
        {U']', ALTA, {".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."}},
        {U'^', ALTA, {"..#..", ".#.#.", "#...#"}},
        {U'_', 10, {"#####"}},
        {U'`', ALTA, {".#...", "..#.."}},

        // -- minúsculas ------------------------------------------------------
        {U'a', BAJA, {".###.", "....#", ".####", "#...#", ".####"}},
        {U'b', ALTA, {"#....", "#....", "####.", "#...#", "#...#", "#...#", "####."}},
        {U'c', BAJA, {".###.", "#....", "#....", "#....", ".###."}},
        {U'd', ALTA, {"....#", "....#", ".####", "#...#", "#...#", "#...#", ".####"}},
        {U'e', BAJA, {".###.", "#...#", "#####", "#....", ".###."}},
        {U'f', ALTA, {"..##.", ".#...", ".#...", "####.", ".#...", ".#...", ".#..."}},
        {U'g', BAJA, {".####", "#...#", "#...#", ".####", "....#", "#...#", ".###."}},
        {U'h', ALTA, {"#....", "#....", "####.", "#...#", "#...#", "#...#", "#...#"}},
        {U'i', ALTA, {"..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###."}},
        {U'j', ALTA, {"...#.", ".....", "..##.", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
        {U'k', ALTA, {"#....", "#....", "#..#.", "#.#..", "##...", "#.#..", "#..#."}},
        {U'l', ALTA, {".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
        {U'm', BAJA, {"##.#.", "#.#.#", "#.#.#", "#.#.#", "#...#"}},
        {U'n', BAJA, {"####.", "#...#", "#...#", "#...#", "#...#"}},
        {U'o', BAJA, {".###.", "#...#", "#...#", "#...#", ".###."}},
        {U'p', BAJA, {"####.", "#...#", "#...#", "#...#", "####.", "#....", "#...."}},
        {U'q', BAJA, {".####", "#...#", "#...#", "#...#", ".####", "....#", "....#"}},
        // La `r` tenía un píxel suelto en la punta del hombro. A este tamaño un
        // píxel que no toca nada no se lee como remate: se lee como suciedad.
        {U'r', BAJA, {"#.##.", "##...", "#....", "#....", "#...."}},
        {U's', BAJA, {".####", "#....", ".###.", "....#", "####."}},
        {U't', ALTA, {".#...", ".#...", "####.", ".#...", ".#...", ".#..#", "..##."}},
        {U'u', BAJA, {"#...#", "#...#", "#...#", "#...#", ".####"}},
        {U'v', BAJA, {"#...#", "#...#", "#...#", ".#.#.", "..#.."}},
        {U'w', BAJA, {"#...#", "#...#", "#.#.#", "#.#.#", ".#.#."}},
        {U'x', BAJA, {"#...#", ".#.#.", "..#..", ".#.#.", "#...#"}},
        {U'y', BAJA, {"#...#", "#...#", "#...#", ".####", "....#", "#...#", ".###."}},
        {U'z', BAJA, {"#####", "...#.", "..#..", ".#...", "#####"}},

        {U'{', ALTA, {"..##.", "..#..", "..#..", ".#...", "..#..", "..#..", "..##."}},
        {U'|', ALTA, {"..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
        {U'}', ALTA, {".##..", "..#..", "..#..", "...#.", "..#..", "..#..", ".##.."}},
        {U'~', 5, {".##.#", "#..#."}},

        // -- lo que el juego usa y no es ASCII --------------------------------
        // El punto medio separa campos en toda la interfaz ("Musgo · Tímido") y
        // la flecha aparece en los mensajes de error del lector de JSON.
        {U'·', 5, {".##..", ".##.."}},
        {U'—', 6, {"#####"}},
        {U'→', BAJA, {"..#..", "...#.", "#####", "...#.", "..#.."}},
        {U'◆', BAJA, {"..#..", ".###.", "#####", ".###.", "..#.."}},
    };
    return t;
}

/**
 * La `i` sin su punto.
 *
 * No es un glifo del juego —no se puede escribir— pero es de donde sale la `í`:
 * poner la tilde encima de la `i` normal deja el punto abajo, que es un error
 * tipográfico clásico y se ve enseguida.
 */
const Trazo& iSinPunto() {
    static const Trazo t = {0, BAJA, {".##..", "..#..", "..#..", "..#..", ".###."}};
    return t;
}

// ---------------------------------------------------------------------------
// De dibujo a bits
// ---------------------------------------------------------------------------

Glifo desdeTrazo(const Trazo& t) {
    Glifo g;
    g.codigo = t.codigo;
    for (size_t i = 0; i < t.arte.size(); ++i) {
        const int fila = t.fila + static_cast<int>(i);
        if (fila < 0 || fila >= Fuente::ALTO) {
            continue;
        }
        uint8_t bits = 0;
        const char* linea = t.arte[i];
        for (int c = 0; c < Fuente::ANCHO && linea[c] != '\0'; ++c) {
            if (linea[c] == '#') {
                bits |= static_cast<uint8_t>(1u << (Fuente::ANCHO - 1 - c));
            }
        }
        g.filas[fila] = bits;
    }
    return g;
}

/** La primera fila con tinta. `ALTO` si el glifo está vacío. */
int primeraFila(const Glifo& g) {
    for (int f = 0; f < Fuente::ALTO; ++f) {
        if (g.filas[f] != 0) {
            return f;
        }
    }
    return Fuente::ALTO;
}

/**
 * Estampa una marca diacrítica dos filas por encima del cuerpo.
 *
 * Ese "dos filas por encima del cuerpo" —y no "en la fila 0"— es lo que hace
 * que la misma tilde funcione para la `á` y para la `Á`. Con una posición fija,
 * la de la minúscula quedaría flotando con dos filas de aire en el medio.
 */
Glifo acentuar(const Glifo& base, char32_t codigo, const std::vector<const char*>& marca) {
    Glifo g = base;
    g.codigo = codigo;

    const int tope = primeraFila(base);
    const int desde = tope - static_cast<int>(marca.size());
    for (size_t i = 0; i < marca.size(); ++i) {
        const int fila = desde + static_cast<int>(i);
        if (fila < 0 || fila >= Fuente::ALTO) {
            continue;
        }
        uint8_t bits = 0;
        for (int c = 0; c < Fuente::ANCHO && marca[i][c] != '\0'; ++c) {
            if (marca[i][c] == '#') {
                bits |= static_cast<uint8_t>(1u << (Fuente::ANCHO - 1 - c));
            }
        }
        g.filas[fila] |= bits;
    }
    return g;
}

/**
 * Gira el glifo 180°.
 *
 * `¿` y `¡` no son símbolos aparte: son el `?` y el `!` dados vuelta, que es
 * literalmente su historia tipográfica. Rotarlos sale gratis y garantiza que
 * nunca se despareje del signo que abren.
 */
Glifo rotar(const Glifo& base, char32_t codigo) {
    Glifo g;
    g.codigo = codigo;
    for (int f = 0; f < Fuente::ALTO; ++f) {
        uint8_t origen = base.filas[Fuente::ALTO - 1 - f];
        uint8_t bits = 0;
        for (int c = 0; c < Fuente::ANCHO; ++c) {
            if (origen & (1u << c)) {
                bits |= static_cast<uint8_t>(1u << (Fuente::ANCHO - 1 - c));
            }
        }
        g.filas[f] = bits;
    }
    return g;
}

const Glifo* buscar(const std::vector<Glifo>& lista, char32_t codigo) {
    for (const Glifo& g : lista) {
        if (g.codigo == codigo) {
            return &g;
        }
    }
    return nullptr;
}

std::vector<Glifo> construir() {
    std::vector<Glifo> lista;
    lista.reserve(trazos().size() + 24);
    for (const Trazo& t : trazos()) {
        lista.push_back(desdeTrazo(t));
    }

    // Las acentuadas. Cada par es {resultado, base}.
    struct Derivada {
        char32_t codigo;
        char32_t base;
        const std::vector<const char*>* marca;
    };
    static const Derivada DERIVADAS[] = {
        {U'Á', U'A', &TILDE_AGUDA},  {U'É', U'E', &TILDE_AGUDA},
        {U'Í', U'I', &TILDE_AGUDA},  {U'Ó', U'O', &TILDE_AGUDA},
        {U'Ú', U'U', &TILDE_AGUDA},  {U'Ü', U'U', &DIERESIS},
        {U'Ñ', U'N', &TILDE_ENIE},   {U'Â', U'A', &CIRCUNFLEJO},
        {U'á', U'a', &TILDE_AGUDA},  {U'é', U'e', &TILDE_AGUDA},
        {U'ó', U'o', &TILDE_AGUDA},  {U'ú', U'u', &TILDE_AGUDA},
        {U'ü', U'u', &DIERESIS},     {U'ñ', U'n', &TILDE_ENIE},
    };
    for (const Derivada& d : DERIVADAS) {
        const Glifo* base = buscar(lista, d.base);
        if (base != nullptr) {
            lista.push_back(acentuar(*base, d.codigo, *d.marca));
        }
    }

    // La `í` va aparte: primero pierde el punto, después recibe la tilde.
    lista.push_back(acentuar(desdeTrazo(iSinPunto()), U'í', TILDE_AGUDA));

    // Y los signos de apertura, que son los de cierre al revés.
    const Glifo* interrogacion = buscar(lista, U'?');
    if (interrogacion != nullptr) {
        lista.push_back(rotar(*interrogacion, U'¿'));
    }
    const Glifo* exclamacion = buscar(lista, U'!');
    if (exclamacion != nullptr) {
        lista.push_back(rotar(*exclamacion, U'¡'));
    }

    std::sort(lista.begin(), lista.end(),
              [](const Glifo& a, const Glifo& b) { return a.codigo < b.codigo; });
    return lista;
}

} // namespace

bool Glifo::vacio() const {
    for (int f = 0; f < Fuente::ALTO; ++f) {
        if (filas[f] != 0) {
            return false;
        }
    }
    return true;
}

const std::vector<Glifo>& fuenteGlifos() {
    static const std::vector<Glifo> lista = construir();
    return lista;
}

int fuenteFilasAtlas() {
    const int n = static_cast<int>(fuenteGlifos().size());
    return (n + Fuente::COLUMNAS - 1) / Fuente::COLUMNAS;
}

bool fuenteUbicacion(char32_t codigo, int& x, int& y) {
    const std::vector<Glifo>& lista = fuenteGlifos();
    for (size_t i = 0; i < lista.size(); ++i) {
        if (lista[i].codigo != codigo) {
            continue;
        }
        x = static_cast<int>(i % Fuente::COLUMNAS) * Fuente::ANCHO;
        y = static_cast<int>(i / Fuente::COLUMNAS) * Fuente::ALTO;
        return true;
    }
    return false;
}

ImagenFuente fuenteAtlas() {
    const std::vector<Glifo>& lista = fuenteGlifos();

    ImagenFuente img;
    img.ancho = Fuente::COLUMNAS * Fuente::ANCHO;
    img.alto = fuenteFilasAtlas() * Fuente::ALTO;
    img.rgba.assign(static_cast<size_t>(img.ancho) * img.alto * 4, 0);

    for (size_t i = 0; i < lista.size(); ++i) {
        const int ox = static_cast<int>(i % Fuente::COLUMNAS) * Fuente::ANCHO;
        const int oy = static_cast<int>(i / Fuente::COLUMNAS) * Fuente::ALTO;

        for (int f = 0; f < Fuente::ALTO; ++f) {
            const uint8_t bits = lista[i].filas[f];
            for (int c = 0; c < Fuente::ANCHO; ++c) {
                if ((bits & (1u << (Fuente::ANCHO - 1 - c))) == 0) {
                    continue;
                }
                // Tinta blanca opaca: Godot la multiplica por el color de la
                // fuente, así que el mismo atlas sirve para el verde fósforo del
                // registro y para el ámbar de los avisos.
                const size_t p = (static_cast<size_t>(oy + f) * img.ancho + (ox + c)) * 4;
                img.rgba[p + 0] = 255;
                img.rgba[p + 1] = 255;
                img.rgba[p + 2] = 255;
                img.rgba[p + 3] = 255;
            }
        }
    }
    return img;
}

} // namespace petbits
