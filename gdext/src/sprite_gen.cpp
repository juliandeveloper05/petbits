/**
 * sprite_gen.cpp — ver sprite_gen.h.
 *
 * Traducción de spriteGen.ts. Se conservó el orden de los pasos y los números
 * mágicos tal cual: cada uno está calibrado mirando sesenta criaturas a la vez
 * en el laboratorio de la web, y moverlos "para que quede más prolijo" cambia
 * criaturas que ya existen.
 */

#include "sprite_gen.h"
#include "js_math.h"
#include "palette.h"
#include "rng.h"
#include "traits.h"

#include <algorithm>
#include <cmath>

namespace petbits {

/** Eje de simetría: cae entre las columnas 15 y 16. */
static constexpr double AXIS = (SPRITE_SIZE - 1) / 2.0;

/** Línea de piso. Todas las etapas se apoyan acá, así no "flotan" distinto. */
static constexpr double GROUND = 28.0;

static constexpr Rgb SCLERA = {248, 248, 252};

struct Shape {
    double cy;
    double rx;
    double ry;
    /** Exponente de la superelipse: 2 = elipse, 4 = casi rectángulo. */
    double n;
};

struct Archetype {
    Shape head;
    Shape body;
    bool legs;
};

static const Archetype ARCHETYPES[] = {
    /* blob     */ {{14, 10, 9, 2.4}, {21, 9, 7, 2.6}, false},
    /* chibi    */ {{11, 9, 8, 2.2}, {22, 7, 6, 2.4}, true},
    /* alto     */ {{9, 6, 6, 2.2}, {20, 7, 8, 2.6}, true},
    /* redondo  */ {{14, 11, 10, 3}, {20, 10, 8, 3}, false},
    /* esbelto  */ {{10, 6, 7, 2}, {20, 5, 8, 2.2}, true},
    /* cuadrado */ {{12, 8, 7, 4}, {21, 8, 7, 4}, true},
    /* gota     */ {{12, 7, 8, 2}, {21, 9, 7, 2.8}, false},
    /* ancho    */ {{11, 9, 7, 2.6}, {20, 11, 8, 2.4}, true},
};
static constexpr size_t ARCHETYPES_COUNT = sizeof(ARCHETYPES) / sizeof(ARCHETYPES[0]);

struct StageScale {
    double overall;
    double headBoost;
};
static constexpr StageScale STAGE_SCALE[] = {
    {0.66, 1.2},  // bebe
    {0.84, 1.07}, // juvenil
    {1.0, 1.0},   // adulto
};

/**
 * Cómo deforma el cuerpo cada forma evolutiva.
 *
 * Es lo que hace visible la evolución ramificada: sin esto, "crianzas opuestas
 * dan adultos distintos" sería una etiqueta en una ficha y nada más.
 *
 * La rama pétrea engorda el cuerpo y sube el exponente de la superelipse, que
 * la vuelve más cuadrada y pesada. La vaporosa afina y redondea. Los apéndices
 * se suman por OR a los del genoma, así que la forma agrega rasgos sin borrar
 * los que la criatura ya traía.
 */
struct FormModifier {
    double body;
    double head;
    double exponent;
    int appendages;
};
static constexpr FormModifier FORM_MODIFIERS[] = {
    {1.0, 1.0, 0.0, 0b0000},     // indefinida
    {1.14, 0.94, 0.9, 0b0010},   // petreo
    {0.86, 1.08, -0.3, 0b0001},  // vaporoso
    // El rango va de 0.78 a 1.24 en el cuerpo. Un rango más chico no se lee a
    // 32×32: dos píxeles de diferencia pasan desapercibidos.
    {1.24, 0.88, 1.7, 0b1010},   // coloso
    {1.16, 1.02, 1.1, 0b0011},   // guardian
    {0.8, 1.0, -0.5, 0b1101},    // errante
    {0.78, 1.2, -0.25, 0b0100},  // oraculo
};

// ---------------------------------------------------------------------------
// Geometría
// ---------------------------------------------------------------------------

static bool inSuperellipse(int x, int y, const Shape& s) {
    const double dx = std::abs((x - AXIS) / s.rx);
    const double dy = std::abs((y - s.cy) / s.ry);
    return std::pow(dx, s.n) + std::pow(dy, s.n) <= 1.0;
}

/** Escala una forma respecto de la línea de piso, para que siga apoyada. */
static Shape scaleShape(const Shape& s, double scale) {
    return Shape{GROUND - (GROUND - s.cy) * scale, s.rx * scale, s.ry * scale, s.n};
}

static Archetype archetypeFor(const Genes& genes) {
    Archetype base = ARCHETYPES[genes.bodyShape % ARCHETYPES_COUNT];
    // El bit alto de bodyShape invierte si tiene patas: duplica la variedad sin
    // duplicar la tabla.
    if (genes.bodyShape >= 8) base.legs = !base.legs;
    return base;
}

// ---------------------------------------------------------------------------
// Apéndices — se agregan a la máscara para que compartan color y contorno
// ---------------------------------------------------------------------------

static void addTriangle(Mask& mask, double tipX, double baseY, double width, double height) {
    for (int i = 0; i < static_cast<int>(height); ++i) {
        const double halfWidth = ((width * (height - i)) / height) * 0.5;
        const int medio = jsRoundI(halfWidth);
        for (int dx = -medio; dx <= medio; ++dx) {
            mask.setMirrored(jsRoundI(tipX + dx), jsRoundI(baseY - i));
        }
    }
}

static void addEllipse(Mask& mask, double cx, double cy, double rx, double ry) {
    for (int y = static_cast<int>(std::floor(cy - ry)); y <= static_cast<int>(std::ceil(cy + ry));
         ++y) {
        for (int x = static_cast<int>(std::floor(cx - rx));
             x <= static_cast<int>(std::ceil(cx + rx)); ++x) {
            const double dx = (x - cx) / rx;
            const double dy = (y - cy) / ry;
            if (dx * dx + dy * dy <= 1.0) mask.setMirrored(x, y);
        }
    }
}

/**
 * Fila más alta que puede ocupar el cuerpo.
 *
 * El contorno se dibuja POR FUERA de la silueta, así que hace falta dejarle una
 * fila libre arriba. Sin esto, una oreja alta sobre una cabeza alta llega a la
 * fila 0, queda sin contorno y el sprite se ve recortado contra el borde.
 */
static constexpr double MIN_BODY_Y = 2.0;
static constexpr double MIN_BODY_X = 2.0;

static double clampApexBase(double baseY, double height) {
    return std::max(baseY, MIN_BODY_Y + height - 1.0);
}

/**
 * Corre un apéndice lateral hacia adentro si se sale por el costado.
 *
 * Alas y aletas se dibujan POR FUERA del cuerpo, así que en un cuerpo ancho su
 * borde izquierdo caía en la columna 0 y quedaba recortado contra el marco. Al
 * empujarlo hacia adentro puede solaparse con el cuerpo, lo que visualmente lo
 * deja pegado al costado — preferible a que aparezca cortado.
 */
static double clampLateral(double centerX, double halfWidth) {
    return std::max(centerX, MIN_BODY_X + halfWidth);
}

/**
 * Semiancho máximo del cuerpo.
 *
 * Con el eje en 15.5, un radio de 13 deja la silueta entre las columnas 2 y 29,
 * con lugar para el contorno.
 */
static constexpr double MAX_HALF_WIDTH = 13.0;

static Shape fitShape(const Shape& s, double exponentDelta) {
    Shape r = s;
    r.rx = std::min(s.rx, MAX_HALF_WIDTH);
    // Piso de 1.8: por debajo la superelipse se afina en punta y deja de leerse
    // como un cuerpo.
    r.n = std::max(1.8, s.n + exponentDelta);
    const double overflow = MIN_BODY_Y - (s.cy - s.ry);
    r.cy = overflow > 0 ? s.cy + overflow : s.cy;
    return r;
}

static void addAppendages(Mask& mask, int appendages, const Shape& head, const Shape& body,
                          bool legs) {
    // bit 0 — orejas
    if ((appendages & 1) != 0) {
        addTriangle(mask, AXIS - head.rx * 0.55, clampApexBase(head.cy - head.ry * 0.6, 5), 4, 5);
    }
    // bit 1 — cuernos
    if ((appendages & 2) != 0) {
        addTriangle(mask, AXIS - head.rx * 0.42, clampApexBase(head.cy - head.ry * 0.8, 5), 2, 5);
    }
    // bit 2 — alas
    if ((appendages & 4) != 0) {
        addEllipse(mask, clampLateral(AXIS - body.rx - 1.5, 3), body.cy - 1, 3, 4.5);
    }
    // bit 3 — aletas laterales (una "cola" espejada serían dos, así que va aleta)
    if ((appendages & 8) != 0) {
        addTriangle(mask, clampLateral(AXIS - body.rx - 1, 1.5), body.cy + 3, 3, 3);
    }

    if (legs) {
        const int legX = jsRoundI(AXIS - body.rx * 0.5);
        const int legTop = jsRoundI(body.cy + body.ry * 0.6);
        for (int y = legTop; y <= static_cast<int>(GROUND) + 1; ++y) {
            mask.setMirrored(legX, y);
            mask.setMirrored(legX - 1, y);
        }
    }
}

// ---------------------------------------------------------------------------
// Patrones
// ---------------------------------------------------------------------------

static void applyPattern(PixelBuffer& buffer, const Mask& mask, const Genes& genes, Seed seed,
                         const Shape& head, const Shape& body, const Ramp& ramp) {
    const int style = genes.pattern % 8;
    const Rgb color = genes.pattern >= 8 ? ramp[RAMP_ACCENT] : ramp[RAMP_SHADOW];
    const double midY = (head.cy + body.cy) / 2.0;

    const auto paint = [&](int x, int y) {
        if (mask.has(x, y)) buffer.set(x, y, color);
    };

    switch (style) {
        case 1: {
            // Panza: mancha clara en la mitad baja del cuerpo.
            for (int y = 0; y < SPRITE_SIZE; ++y) {
                for (int x = 0; x < SPRITE_SIZE; ++x) {
                    const double dx = (x - AXIS) / (body.rx * 0.62);
                    const double dy = (y - (body.cy + 1)) / (body.ry * 0.72);
                    if (dx * dx + dy * dy <= 1.0 && mask.has(x, y)) {
                        buffer.set(x, y, ramp[RAMP_LIGHT]);
                    }
                }
            }
            break;
        }
        case 2: {
            // Rayas horizontales.
            for (int y = 0; y < SPRITE_SIZE; ++y) {
                if (y % 5 >= 2) continue;
                for (int x = 0; x < SPRITE_SIZE; ++x) paint(x, y);
            }
            break;
        }
        case 3: {
            // Franja vertical por el eje.
            for (int y = 0; y < SPRITE_SIZE; ++y) {
                for (int x = jsRoundI(AXIS - 1); x <= jsRoundI(AXIS + 2); ++x) paint(x, y);
            }
            break;
        }
        case 4: {
            // Manchas en posiciones deterministas derivadas del genoma.
            Rng rng = rngFor(seed, "manchas");
            const int64_t spots = rng.rango(3, 5);
            for (int64_t i = 0; i < spots; ++i) {
                const int64_t cx = rng.rango(4, SPRITE_SIZE - 5);
                const int64_t cy = rng.rango(jsRoundI(head.cy), static_cast<int>(GROUND) - 2);
                const int64_t radius = rng.rango(1, 2);
                for (int64_t y = cy - radius; y <= cy + radius; ++y) {
                    for (int64_t x = cx - radius; x <= cx + radius; ++x) {
                        const int64_t ddx = x - cx;
                        const int64_t ddy = y - cy;
                        if (ddx * ddx + ddy * ddy <= radius * radius) {
                            paint(static_cast<int>(x), static_cast<int>(y));
                        }
                    }
                }
            }
            break;
        }
        case 5: {
            // Bicolor: mitad de abajo en otro color.
            for (int y = jsRoundI(midY); y < SPRITE_SIZE; ++y) {
                for (int x = 0; x < SPRITE_SIZE; ++x) paint(x, y);
            }
            break;
        }
        case 6: {
            // Cachetes.
            const int cheekY = jsRoundI(head.cy + head.ry * 0.2);
            const int cheekX = jsRoundI(AXIS - head.rx * 0.62);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (std::abs(dx) + std::abs(dy) > 2) continue;
                    if (mask.has(cheekX + dx, cheekY + dy)) {
                        buffer.set(cheekX + dx, cheekY + dy, ramp[RAMP_ACCENT]);
                    }
                    const int mirrored = SPRITE_SIZE - 1 - (cheekX + dx);
                    if (mask.has(mirrored, cheekY + dy)) {
                        buffer.set(mirrored, cheekY + dy, ramp[RAMP_ACCENT]);
                    }
                }
            }
            break;
        }
        case 7: {
            // Degradado con tramado (dithering) en la mitad baja.
            for (int y = jsRoundI(midY); y < SPRITE_SIZE; ++y) {
                for (int x = 0; x < SPRITE_SIZE; ++x) {
                    if ((x + y) % 2 == 0) paint(x, y);
                }
            }
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Cara
// ---------------------------------------------------------------------------

/** Ancho en píxeles de cada estilo de ojo, para poder centrar el par. */
static constexpr int EYE_WIDTHS[] = {1, 2, 2, 3, 3, 3, 2, 2};

static void drawEye(PixelBuffer& buffer, double cx, double cy, int style, const Ramp& ramp) {
    const Rgb dark = ramp[RAMP_OUTLINE];
    const int x = jsRoundI(cx);
    const int y = jsRoundI(cy);

    switch (style) {
        case 0:
            buffer.set(x, y, dark);
            buffer.set(x, y + 1, dark);
            break;
        case 1:
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) buffer.set(x + dx, y + dy, dark);
            break;
        case 2:
            for (int dy = 0; dy < 3; ++dy)
                for (int dx = 0; dx < 2; ++dx) buffer.set(x + dx, y + dy, dark);
            buffer.set(x, y, SCLERA);
            break;
        case 3:
            for (int dy = 0; dy < 3; ++dy) {
                for (int dx = 0; dx < 3; ++dx) buffer.set(x + dx, y + dy, SCLERA);
            }
            for (int dy = 1; dy < 3; ++dy) {
                for (int dx = 1; dx < 3; ++dx) buffer.set(x + dx, y + dy, dark);
            }
            buffer.set(x, y, SCLERA);
            break;
        case 4:
            // Ojo cerrado y contento: "^"
            buffer.set(x, y + 1, dark);
            buffer.set(x + 1, y, dark);
            buffer.set(x + 2, y + 1, dark);
            break;
        case 5:
            // Mirada afilada.
            buffer.set(x, y + 1, dark);
            buffer.set(x + 1, y + 1, dark);
            buffer.set(x + 2, y, dark);
            break;
        case 6:
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) buffer.set(x + dx, y + dy, dark);
            buffer.set(x, y + 3, dark);
            break;
        default:
            buffer.set(x, y, dark);
            buffer.set(x + 1, y, dark);
            buffer.set(x, y + 2, dark);
            buffer.set(x + 1, y + 2, dark);
            break;
    }
}

static void drawFace(PixelBuffer& buffer, const Genes& genes, const Shape& head, const Ramp& ramp,
                     Expression expression) {
    const Rgb dark = ramp[RAMP_OUTLINE];
    // Parpadear es la animación más barata que existe y la que más hace por que
    // algo lea como vivo. Se fuerza el estilo de ojo cerrado sobre el del genoma.
    const int style = expression == Expression::Parpadeo ? 4 : genes.eyes % 8;
    const double eyeY = head.cy - head.ry * 0.15;

    if (style == 6 && genes.eyes >= 8 && expression != Expression::Parpadeo) {
        // Cíclope: un ojo grande centrado en el eje.
        //
        // Va oscuro por fuera y con un brillo blanco adentro, no al revés. Un
        // bloque blanco con pupila chica sobre un cuerpo claro no lee como ojo:
        // lee como un agujero en el sprite.
        //
        // jsRound(AXIS) da 16, así que el bloque de 4 arranca en 14 y queda
        // repartido 14-17 alrededor del eje en 15.5.
        const int left = jsRoundI(AXIS) - 2;
        const int top = jsRoundI(eyeY);
        for (int dy = 0; dy < 5; ++dy) {
            for (int dx = 0; dx < 4; ++dx) {
                // Esquinas recortadas para que la silueta lea redonda.
                if ((dy == 0 || dy == 4) && (dx == 0 || dx == 3)) continue;
                buffer.set(left + dx, top + dy, dark);
            }
        }
        buffer.set(left + 1, top + 1, SCLERA);
        buffer.set(left + 2, top + 1, SCLERA);
        buffer.set(left + 1, top + 2, SCLERA);
    } else {
        const double spread = std::max(2.2, head.rx * 0.44);
        const int width = EYE_WIDTHS[style];
        const int leftX = jsRoundI(AXIS - spread - 1);
        // El ojo derecho va en la posición espejada del izquierdo, pero se
        // DIBUJA igual, sin espejar. Así el par queda centrado sobre el eje y el
        // brillo cae del mismo lado en los dos, coherente con la dirección de
        // luz del sprite.
        //
        // Trasladarlo (leftX + 2·spread) parece equivalente pero no lo es: con
        // ojos de ancho par deja el par corrido un píxel a la izquierda.
        const int rightX = SPRITE_SIZE - 1 - leftX - (width - 1);
        drawEye(buffer, leftX, eyeY, style, ramp);
        drawEye(buffer, rightX, eyeY, style, ramp);
    }

    // Boca
    const int mouthY = jsRoundI(eyeY + std::max(3.0, head.ry * 0.45));
    const int mx = jsRoundI(AXIS);
    switch (genes.mouth % 6) {
        case 1:
            buffer.set(mx, mouthY, dark);
            break;
        case 2:
            buffer.set(mx - 1, mouthY, dark);
            buffer.set(mx, mouthY + 1, dark);
            buffer.set(mx + 1, mouthY, dark);
            break;
        case 3:
            buffer.set(mx - 2, mouthY, dark);
            buffer.set(mx - 1, mouthY + 1, dark);
            buffer.set(mx, mouthY + 1, dark);
            buffer.set(mx + 1, mouthY + 1, dark);
            buffer.set(mx + 2, mouthY, dark);
            break;
        case 4:
            for (int dx = -1; dx <= 1; ++dx) buffer.set(mx + dx, mouthY, dark);
            buffer.set(mx - 1, mouthY + 1, SCLERA);
            buffer.set(mx + 1, mouthY + 1, SCLERA);
            break;
        case 5:
            buffer.set(mx, mouthY, dark);
            buffer.set(mx + 1, mouthY, dark);
            buffer.set(mx, mouthY + 1, dark);
            buffer.set(mx + 1, mouthY + 1, dark);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Entrada principal
// ---------------------------------------------------------------------------

Sprite generateSprite(Seed seed, Stage stage, Form form, Expression expression) {
    const Genes genes = decodeGenome(seed);
    const std::vector<Trait> traits = detectTraits(seed);
    const Ramp ramp = buildRamp(genes, traits, form);

    const Archetype archetype = archetypeFor(genes);
    const StageScale& escala = STAGE_SCALE[static_cast<size_t>(stage)];
    const FormModifier& shift = FORM_MODIFIERS[static_cast<size_t>(form)];

    const double headScale =
        (0.88 + (genes.proportion & 0b11) * 0.06) * escala.overall * escala.headBoost * shift.head;
    const double bodyScale =
        (0.88 + ((genes.proportion >> 2) & 0b11) * 0.06) * escala.overall * shift.body;
    const Shape head = fitShape(scaleShape(archetype.head, headScale), shift.exponent);
    const Shape body = fitShape(scaleShape(archetype.body, bodyScale), shift.exponent);
    const int appendages = genes.appendages | shift.appendages;

    // 1. Silueta, en media grilla y espejada.
    Mask mask(SPRITE_SIZE, SPRITE_SIZE);
    for (int y = 0; y < SPRITE_SIZE; ++y) {
        for (int x = 0; x <= static_cast<int>(std::floor(AXIS)); ++x) {
            if (inSuperellipse(x, y, head) || inSuperellipse(x, y, body)) mask.setMirrored(x, y);
        }
    }
    addAppendages(mask, appendages, head, body, archetype.legs);

    // 2. Relleno base.
    PixelBuffer buffer(SPRITE_SIZE, SPRITE_SIZE);
    for (int y = 0; y < SPRITE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_SIZE; ++x) {
            if (mask.has(x, y)) buffer.set(x, y, ramp[RAMP_BASE]);
        }
    }

    // 3. Patrón.
    applyPattern(buffer, mask, genes, seed, head, body, ramp);

    // 4. Sombreado direccional. Se calcula sobre el buffer YA espejado, así que
    //    la luz cae siempre desde arriba-izquierda y rompe la simetría a
    //    propósito.
    for (int y = 0; y < SPRITE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_SIZE; ++x) {
            if (!mask.has(x, y)) continue;
            if (!mask.has(x, y + 2)) buffer.set(x, y, ramp[RAMP_SHADOW]);
        }
    }
    for (int y = 0; y < SPRITE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_SIZE; ++x) {
            if (!mask.has(x, y)) continue;
            if (!mask.has(x - 1, y - 2)) buffer.set(x, y, ramp[RAMP_LIGHT]);
        }
    }

    // 5. Contorno por fuera de la silueta, para no comerle tamaño al cuerpo.
    for (int y = 0; y < SPRITE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_SIZE; ++x) {
            if (mask.has(x, y)) continue;
            if (mask.touchesBody(x, y)) buffer.set(x, y, ramp[RAMP_OUTLINE]);
        }
    }

    // 6. La cara siempre arriba de todo.
    drawFace(buffer, genes, head, ramp, expression);

    return Sprite{SPRITE_SIZE, SPRITE_SIZE, buffer.datos()};
}

} // namespace petbits
