/**
 * palette.cpp — ver palette.h.
 */

#include "palette.h"
#include "js_math.h"

#include <cmath>

namespace petbits {

static double clampD(double value, double min, double max) {
    return value < min ? min : (value > max ? max : value);
}

/** OKLab → sRGB lineal. Coeficientes de la definición de Björn Ottosson. */
struct LinearRgb {
    double r, g, b;
};

static LinearRgb oklabToLinearSrgb(double lightness, double a, double b) {
    // El TS escribe `(...) ** 3`. Se traduce con std::pow y no con x*x*x a
    // propósito: son dos operaciones distintas y pueden diferir en el último
    // bit. Cuál de las dos elige el motor de JavaScript no es algo que se pueda
    // razonar desde acá — se comprueba, y los vectores de paridad dicen que
    // esta coincide.
    const double l = std::pow(lightness + 0.3963377774 * a + 0.2158037573 * b, 3.0);
    const double m = std::pow(lightness - 0.1055613458 * a - 0.0638541728 * b, 3.0);
    const double s = std::pow(lightness - 0.0894841775 * a - 1.291485548 * b, 3.0);

    return LinearRgb{
        4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s,
    };
}

static double linearToSrgb(double channel) {
    return channel <= 0.0031308 ? 12.92 * channel
                                : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
}

static bool isInGamut(const LinearRgb& c) {
    const double eps = 0.0001;
    return c.r >= -eps && c.r <= 1 + eps && c.g >= -eps && c.g <= 1 + eps && c.b >= -eps &&
           c.b <= 1 + eps;
}

Rgb oklchToRgb(double lightness, double chroma, double hueDeg) {
    const double L = clampD(lightness, 0.0, 1.0);
    const double hueRad = (hueDeg * 3.141592653589793) / 180.0;
    const double cos = std::cos(hueRad);
    const double sin = std::sin(hueRad);

    const auto at = [&](double c) { return oklabToLinearSrgb(L, c * cos, c * sin); };

    double usableChroma = std::max(0.0, chroma);
    if (!isInGamut(at(usableChroma))) {
        double low = 0.0;
        double high = usableChroma;
        for (int i = 0; i < 16; ++i) {
            const double mid = (low + high) / 2.0;
            if (isInGamut(at(mid))) low = mid;
            else high = mid;
        }
        usableChroma = low;
    }

    const LinearRgb lin = at(usableChroma);
    return Rgb{
        static_cast<uint8_t>(jsRound(clampD(linearToSrgb(lin.r), 0.0, 1.0) * 255.0)),
        static_cast<uint8_t>(jsRound(clampD(linearToSrgb(lin.g), 0.0, 1.0) * 255.0)),
        static_cast<uint8_t>(jsRound(clampD(linearToSrgb(lin.b), 0.0, 1.0) * 255.0)),
    };
}

// ---------------------------------------------------------------------------
// Modos de paleta
// ---------------------------------------------------------------------------

struct PaletteMode {
    std::string_view name;
    double baseLightness;
    double chroma;
    double accentHueShift;
    bool tieneBanda;
    double bandaDesde;
    double bandaHasta;
};

static constexpr std::array<PaletteMode, 8> PALETTE_MODES = {{
    {"Pastel", 0.82, 0.068, 42, false, 0, 0},
    {"Neón", 0.7, 0.19, 150, false, 0, 0},
    {"Tierra", 0.58, 0.085, -34, true, 22, 105},
    {"Mono", 0.64, 0.018, 0, false, 0, 0},
    {"Dúo", 0.66, 0.145, 180, false, 0, 0},
    // Guiño a la consola verde fósforo que ya tenía el proyecto.
    {"Fósforo", 0.74, 0.135, 18, true, 118, 148},
    {"Caramelo", 0.77, 0.155, 62, false, 0, 0},
    // Abismo estaba en 0.46 y el test de contraste lo bajó de un hondazo: con
    // esa L la luminancia relativa queda en ~0.097, y el contraste máximo
    // posible contra negro puro es (0.097+0.05)/0.05 = 2.94:1. Ningún contorno,
    // por oscuro que fuera, podía llegar a 3:1.
    {"Abismo", 0.53, 0.105, 196, false, 0, 0},
}};

std::string_view paletteModeName(const Genes& genes) {
    return PALETTE_MODES[genes.paletteMode % PALETTE_MODES.size()].name;
}

/**
 * Cómo corre la paleta cada forma evolutiva.
 *
 * A 32×32 la geometría sola no alcanza para distinguir cuatro adultos: dos
 * píxeles más de ancho no se leen. El color sí se lee al instante, así que la
 * crianza también tiñe. El tono base viene del genoma y no se toca — solo se
 * corren luminosidad y croma, así que la criatura sigue siendo reconociblemente
 * ella.
 */
struct FormPalette {
    double lightness;
    double chroma;
};

static constexpr std::array<FormPalette, 7> FORM_PALETTE = {{
    {0.0, 1.0},    // indefinida
    {-0.05, 1.1},  // petreo
    {0.06, 0.85},  // vaporoso
    {-0.11, 1.3},  // coloso
    {-0.04, 0.8},  // guardian
    {0.09, 1.2},   // errante
    {0.13, 0.65},  // oraculo
}};

Ramp buildRamp(const Genes& genes, const std::vector<Trait>& traits, Form form) {
    const PaletteMode& mode = PALETTE_MODES[genes.paletteMode % PALETTE_MODES.size()];

    double hue = (static_cast<double>(genes.hue) / 256.0) * 360.0;
    if (mode.tieneBanda) {
        hue = mode.bandaDesde +
              (static_cast<double>(genes.hue) / 256.0) * (mode.bandaHasta - mode.bandaDesde);
    }

    double baseL = mode.baseLightness;
    double chroma = mode.chroma;
    double accentShift = mode.accentHueShift;

    // Las rarezas se ven. Un "Vacío" no es solo una etiqueta en la ficha: se
    // nota de lejos porque está despigmentado.
    const auto has = [&](std::string_view id) {
        for (const Trait& t : traits) {
            if (t.id == id) return true;
        }
        return false;
    };

    if (has("vacio")) {
        chroma *= 0.22;
        baseL = std::min(0.9, baseL + 0.1);
    }
    if (has("saturado")) {
        chroma *= 1.65;
        baseL = std::max(0.34, baseL - 0.09);
    }
    if (has("primordial")) {
        chroma *= 1.12;
    }
    if (has("espejo") || has("pangrama")) {
        accentShift = 180;
        chroma *= 1.3;
    }

    // La forma evolutiva corre la paleta al final, sobre lo que ya definieron el
    // genoma y las rarezas.
    const FormPalette& shift = FORM_PALETTE[static_cast<size_t>(form)];
    baseL += shift.lightness;
    chroma *= shift.chroma;

    chroma = clampD(chroma, 0.0, 0.36);
    // El piso de 0.50 no es arbitrario. En OKLab la luminancia relativa va como
    // L³, así que un cuerpo con L = 0.50 tiene Y ≈ 0.125 y su contraste máximo
    // contra negro puro es (0.125 + 0.05) / 0.05 = 3.5:1. Con 0.47 el techo baja
    // a 3.08 y un contorno real —que no es negro puro— ya queda por debajo de
    // 3:1. Las formas oscuras como Coloso rozan este piso.
    baseL = clampD(baseL, 0.5, 0.92);

    return Ramp{{
        oklchToRgb(std::max(0.12, baseL - 0.44), chroma * 0.55, hue - 6),
        oklchToRgb(baseL - 0.17, chroma * 1.12, hue - 11),
        oklchToRgb(baseL, chroma, hue),
        oklchToRgb(std::min(0.97, baseL + 0.14), chroma * 0.8, hue + 13),
        oklchToRgb(clampD(baseL + 0.04, 0.0, 1.0), chroma * 1.3, hue + accentShift),
    }};
}

} // namespace petbits
