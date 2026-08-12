#pragma once
/**
 * palette.h — Port de src/core/palette.ts
 *
 * Paletas derivadas del genoma, construidas en OKLCH.
 *
 * Por qué OKLCH y no HSL: en HSL, el amarillo y el azul con la misma
 * "lightness" declarada se ven clarísimamente distintos de claros. Generando
 * cientos de paletas al azar eso significa que una parte importante sale lavada
 * o ilegible. OKLCH es perceptualmente uniforme, así que TODA criatura generada
 * tiene contraste utilizable entre su base, su sombra y su luz.
 */

#include "evolution.h"
#include "genome.h"
#include "pixel_buffer.h"
#include "traits.h"

#include <array>
#include <string_view>
#include <vector>

namespace petbits {

/** Rampa de 5 colores: contorno, sombra, base, luz, acento. */
using Ramp = std::array<Rgb, 5>;

inline constexpr size_t RAMP_OUTLINE = 0;
inline constexpr size_t RAMP_SHADOW = 1;
inline constexpr size_t RAMP_BASE = 2;
inline constexpr size_t RAMP_LIGHT = 3;
inline constexpr size_t RAMP_ACCENT = 4;

/**
 * OKLCH → RGB de 8 bits.
 *
 * Si el color cae fuera del gamut sRGB se le baja el croma por búsqueda binaria
 * en vez de recortar los canales. Recortar desplaza el tono —un rojo saturado
 * fuera de gamut se vuelve naranja—; bajar croma conserva tono y luminosidad,
 * que es lo que hace que la rampa siga leyéndose como la misma familia.
 */
Rgb oklchToRgb(double lightness, double chroma, double hueDeg);

std::string_view paletteModeName(const Genes& genes);

/** Construye la rampa de 5 colores de una criatura. */
Ramp buildRamp(const Genes& genes, const std::vector<Trait>& traits, Form form = Form::Indefinida);

} // namespace petbits
