/**
 * Paletas derivadas del genoma, construidas en OKLCH.
 *
 * Por qué OKLCH y no HSL: en HSL, `hsl(60 80% 50%)` (amarillo) y
 * `hsl(240 80% 50%)` (azul) declaran la misma "lightness" pero el amarillo se ve
 * muchísimo más claro. Generando cientos de paletas al azar eso significa que
 * una parte importante sale lavada o ilegible. OKLCH es perceptualmente
 * uniforme: la misma L se ve igual de clara en cualquier tono, así que TODA
 * criatura generada tiene contraste utilizable entre su base, su sombra y su luz.
 */

import type { Genes } from "./genome.ts";
import type { Trait } from "./traits.ts";

export interface Rgb {
  r: number;
  g: number;
  b: number;
}

/** Rampa de 5 colores: contorno, sombra, base, luz, acento. */
export type Ramp = readonly [Rgb, Rgb, Rgb, Rgb, Rgb];

export const RAMP_OUTLINE = 0;
export const RAMP_SHADOW = 1;
export const RAMP_BASE = 2;
export const RAMP_LIGHT = 3;
export const RAMP_ACCENT = 4;

// ---------------------------------------------------------------------------
// Conversión de color
// ---------------------------------------------------------------------------

function clamp(value: number, min: number, max: number): number {
  return value < min ? min : value > max ? max : value;
}

/** OKLab → sRGB lineal. Coeficientes de la definición de Björn Ottosson. */
function oklabToLinearSrgb(lightness: number, a: number, b: number): [number, number, number] {
  const l = (lightness + 0.3963377774 * a + 0.2158037573 * b) ** 3;
  const m = (lightness - 0.1055613458 * a - 0.0638541728 * b) ** 3;
  const s = (lightness - 0.0894841775 * a - 1.291485548 * b) ** 3;

  return [
    4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
    -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
    -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s,
  ];
}

function linearToSrgb(channel: number): number {
  return channel <= 0.0031308 ? 12.92 * channel : 1.055 * channel ** (1 / 2.4) - 0.055;
}

function isInGamut([r, g, b]: [number, number, number]): boolean {
  const eps = 0.0001;
  return r >= -eps && r <= 1 + eps && g >= -eps && g <= 1 + eps && b >= -eps && b <= 1 + eps;
}

/**
 * OKLCH → RGB de 8 bits.
 *
 * Si el color cae fuera del gamut sRGB se le baja el croma por búsqueda binaria
 * en vez de recortar los canales. Recortar desplaza el tono (un rojo saturado
 * fuera de gamut se vuelve naranja); bajar croma conserva tono y luminosidad,
 * que es lo que hace que la rampa siga leyéndose como la misma familia de color.
 */
export function oklchToRgb(lightness: number, chroma: number, hueDeg: number): Rgb {
  const L = clamp(lightness, 0, 1);
  const hueRad = (hueDeg * Math.PI) / 180;
  const cos = Math.cos(hueRad);
  const sin = Math.sin(hueRad);

  const at = (c: number): [number, number, number] => oklabToLinearSrgb(L, c * cos, c * sin);

  let usableChroma = Math.max(0, chroma);
  if (!isInGamut(at(usableChroma))) {
    let low = 0;
    let high = usableChroma;
    for (let i = 0; i < 16; i++) {
      const mid = (low + high) / 2;
      if (isInGamut(at(mid))) low = mid;
      else high = mid;
    }
    usableChroma = low;
  }

  const [lr, lg, lb] = at(usableChroma);
  return {
    r: Math.round(clamp(linearToSrgb(lr), 0, 1) * 255),
    g: Math.round(clamp(linearToSrgb(lg), 0, 1) * 255),
    b: Math.round(clamp(linearToSrgb(lb), 0, 1) * 255),
  };
}

export function rgbToHex({ r, g, b }: Rgb): string {
  return `#${((1 << 24) | (r << 16) | (g << 8) | b).toString(16).slice(1)}`;
}

// ---------------------------------------------------------------------------
// Modos de paleta
// ---------------------------------------------------------------------------

interface PaletteMode {
  name: string;
  baseLightness: number;
  chroma: number;
  accentHueShift: number;
  /** Si está, el tono del genoma se remapea dentro de esta banda. */
  hueBand?: readonly [number, number];
}

export const PALETTE_MODES: readonly PaletteMode[] = [
  { name: "Pastel", baseLightness: 0.82, chroma: 0.068, accentHueShift: 42 },
  { name: "Neón", baseLightness: 0.7, chroma: 0.19, accentHueShift: 150 },
  { name: "Tierra", baseLightness: 0.58, chroma: 0.085, accentHueShift: -34, hueBand: [22, 105] },
  { name: "Mono", baseLightness: 0.64, chroma: 0.018, accentHueShift: 0 },
  { name: "Dúo", baseLightness: 0.66, chroma: 0.145, accentHueShift: 180 },
  // Guiño a la consola verde fósforo que ya tenía el proyecto.
  { name: "Fósforo", baseLightness: 0.74, chroma: 0.135, accentHueShift: 18, hueBand: [118, 148] },
  { name: "Caramelo", baseLightness: 0.77, chroma: 0.155, accentHueShift: 62 },
  { name: "Abismo", baseLightness: 0.46, chroma: 0.105, accentHueShift: 196 },
];

export function paletteModeName(genes: Genes): string {
  return PALETTE_MODES[genes.paletteMode % PALETTE_MODES.length]?.name ?? "Pastel";
}

/**
 * Construye la rampa de 5 colores de una criatura.
 *
 * Las sombras se desplazan hacia tonos fríos y ganan croma; las luces se
 * desplazan hacia cálidos y lo pierden. Es la misma regla que usa el pixel art
 * hecho a mano, y es la diferencia entre una rampa que parece diseñada y una que
 * parece el mismo color con más y menos brillo.
 */
export function buildRamp(genes: Genes, traits: readonly Trait[] = []): Ramp {
  const mode = PALETTE_MODES[genes.paletteMode % PALETTE_MODES.length] ?? PALETTE_MODES[0];
  if (!mode) throw new Error("No hay modos de paleta definidos");

  let hue = (genes.hue / 256) * 360;
  if (mode.hueBand) {
    const [from, to] = mode.hueBand;
    hue = from + (genes.hue / 256) * (to - from);
  }

  let baseL = mode.baseLightness;
  let chroma = mode.chroma;
  let accentShift = mode.accentHueShift;

  // Las rarezas se ven. Un "Vacío" no es solo una etiqueta en la ficha:
  // se nota de lejos porque está despigmentado.
  const has = (id: string): boolean => traits.some((trait) => trait.id === id);
  if (has("vacio")) {
    chroma *= 0.22;
    baseL = Math.min(0.9, baseL + 0.1);
  }
  if (has("saturado")) {
    chroma *= 1.65;
    baseL = Math.max(0.34, baseL - 0.09);
  }
  if (has("primordial")) {
    chroma *= 1.12;
  }
  if (has("espejo") || has("pangrama")) {
    accentShift = 180;
    chroma *= 1.3;
  }

  chroma = clamp(chroma, 0, 0.36);
  baseL = clamp(baseL, 0.3, 0.9);

  return [
    oklchToRgb(Math.max(0.16, baseL - 0.44), chroma * 0.55, hue - 6),
    oklchToRgb(baseL - 0.17, chroma * 1.12, hue - 11),
    oklchToRgb(baseL, chroma, hue),
    oklchToRgb(Math.min(0.97, baseL + 0.14), chroma * 0.8, hue + 13),
    oklchToRgb(clamp(baseL + 0.04, 0, 1), chroma * 1.3, hue + accentShift),
  ] as const;
}
