import { describe, expect, it } from "vitest";
import { decodeGenome } from "../src/core/genome.ts";
import {
  PALETTE_MODES,
  RAMP_BASE,
  RAMP_LIGHT,
  RAMP_OUTLINE,
  RAMP_SHADOW,
  type Rgb,
  buildRamp,
  oklchToRgb,
  rgbToHex,
} from "../src/core/palette.ts";
import { splitmix64 } from "../src/core/rng.ts";
import { detectTraits } from "../src/core/traits.ts";

/** Luminancia relativa según WCAG. */
function luminance({ r, g, b }: Rgb): number {
  const channel = (value: number): number => {
    const c = value / 255;
    return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
  };
  return 0.2126 * channel(r) + 0.7152 * channel(g) + 0.0722 * channel(b);
}

function contrastRatio(a: Rgb, b: Rgb): number {
  const [light, dark] = [luminance(a), luminance(b)].sort((x, y) => y - x);
  return ((light ?? 0) + 0.05) / ((dark ?? 0) + 0.05);
}

function sample(count: number): bigint[] {
  const seeds: bigint[] = [];
  let cursor = 0xa1e7_7e00_0001n;
  for (let i = 0; i < count; i++) {
    cursor = splitmix64(cursor);
    seeds.push(cursor);
  }
  return seeds;
}

describe("conversión OKLCH", () => {
  it("devuelve canales enteros dentro de 0-255", () => {
    for (let lightness = 0; lightness <= 1; lightness += 0.05) {
      for (let hue = 0; hue < 360; hue += 15) {
        const rgb = oklchToRgb(lightness, 0.15, hue);
        for (const value of [rgb.r, rgb.g, rgb.b]) {
          expect(Number.isInteger(value)).toBe(true);
          expect(value).toBeGreaterThanOrEqual(0);
          expect(value).toBeLessThanOrEqual(255);
        }
      }
    }
  });

  it("baja el croma en vez de recortar cuando se sale del gamut", () => {
    // Un croma absurdo tiene que seguir dando un color válido y, sobre todo,
    // conservar el tono: recortar canales convertiría un rojo saturado en naranja.
    const sane = oklchToRgb(0.6, 0.12, 29);
    const absurd = oklchToRgb(0.6, 0.9, 29);
    for (const value of [absurd.r, absurd.g, absurd.b]) {
      expect(value).toBeGreaterThanOrEqual(0);
      expect(value).toBeLessThanOrEqual(255);
    }
    // Mismo tono dominante: el rojo sigue siendo el canal más alto.
    expect(absurd.r).toBeGreaterThan(absurd.g);
    expect(sane.r).toBeGreaterThan(sane.g);
  });

  it("negro y blanco salen donde corresponde", () => {
    expect(oklchToRgb(0, 0, 0)).toEqual({ r: 0, g: 0, b: 0 });
    const white = oklchToRgb(1, 0, 0);
    expect(white.r).toBeGreaterThan(250);
    expect(white.g).toBeGreaterThan(250);
    expect(white.b).toBeGreaterThan(250);
  });

  it("rgbToHex formatea con 6 dígitos", () => {
    expect(rgbToHex({ r: 0, g: 0, b: 0 })).toBe("#000000");
    expect(rgbToHex({ r: 255, g: 255, b: 255 })).toBe("#ffffff");
    expect(rgbToHex({ r: 155, g: 188, b: 15 })).toBe("#9bbc0f");
  });
});

describe("rampas de criatura", () => {
  it("siempre tiene 5 colores", () => {
    for (const seed of sample(200)) {
      expect(buildRamp(decodeGenome(seed), detectTraits(seed))).toHaveLength(5);
    }
  });

  it("respeta el orden de luminosidad contorno < sombra < base < luz", () => {
    for (const seed of sample(400)) {
      const ramp = buildRamp(decodeGenome(seed), detectTraits(seed));
      const outline = luminance(ramp[RAMP_OUTLINE]);
      const shadow = luminance(ramp[RAMP_SHADOW]);
      const base = luminance(ramp[RAMP_BASE]);
      const light = luminance(ramp[RAMP_LIGHT]);

      expect(outline, `seed ${seed.toString(16)}`).toBeLessThan(shadow);
      expect(shadow, `seed ${seed.toString(16)}`).toBeLessThan(base);
      expect(base, `seed ${seed.toString(16)}`).toBeLessThan(light);
    }
  });

  /**
   * Este es el test que justifica haber elegido OKLCH sobre HSL.
   *
   * En HSL la misma "lightness" se ve muy distinta según el tono, así que al
   * generar cientos de paletas una parte queda sin contraste utilizable entre
   * el cuerpo y su contorno, y la criatura se vuelve ilegible. En OKLCH la
   * luminosidad es perceptualmente uniforme y el piso se sostiene para TODA
   * combinación de tono y modo.
   */
  it("el contorno contrasta con la base en toda combinación de tono y modo", () => {
    let worst = Number.POSITIVE_INFINITY;
    let worstCase = "";

    for (let mode = 0; mode < PALETTE_MODES.length; mode++) {
      for (let hue = 0; hue < 256; hue += 4) {
        const seed = (BigInt(mode) << 32n) | (BigInt(hue) << 24n);
        const ramp = buildRamp(decodeGenome(seed), []);
        const ratio = contrastRatio(ramp[RAMP_OUTLINE], ramp[RAMP_BASE]);
        if (ratio < worst) {
          worst = ratio;
          worstCase = `${PALETTE_MODES[mode]?.name} hue ${hue} → ${ratio.toFixed(2)}:1`;
        }
      }
    }

    console.log(`peor contraste contorno/base: ${worstCase}`);
    expect(worst, `peor caso: ${worstCase}`).toBeGreaterThan(3);
  });

  it("las rarezas de pigmentación se notan", () => {
    // "Vacío" despigmenta y "Saturado" intensifica: no son solo etiquetas.
    const genes = decodeGenome(0x00000000aa000000n);
    const plain = buildRamp(genes, []);
    const empty = buildRamp(
      genes,
      detectTraits(0n).concat({
        id: "vacio",
        name: "Vacío",
        rule: "test",
        tier: "raro",
        approxRate: 0.03,
      }),
    );

    const chromaOf = (rgb: Rgb): number =>
      Math.max(rgb.r, rgb.g, rgb.b) - Math.min(rgb.r, rgb.g, rgb.b);
    expect(chromaOf(empty[RAMP_BASE])).toBeLessThan(chromaOf(plain[RAMP_BASE]));
  });
});
