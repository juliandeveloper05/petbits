/**
 * Generador de sprites: genoma → pixel art de 32×32.
 *
 * Dos reglas de composición sostienen todo esto:
 *
 * 1. **Silueta simétrica, luz asimétrica.** El cuerpo se construye en media
 *    grilla y se espeja, así que siempre lee como criatura y nunca como mancha.
 *    Pero el sombreado se pinta después, sobre el buffer ya espejado y con una
 *    dirección de luz fija (arriba-izquierda). Esa asimetría es lo que hace que
 *    parezca dibujado a mano en vez de un test de Rorschach.
 *
 * 2. **Los ojos mandan.** Es el rasgo que decide si algo lee como ser vivo. Se
 *    dibujan al final, siempre por encima del patrón, y con el brillo del mismo
 *    lado en los dos ojos.
 *
 * Nada de esto toca el DOM: devuelve un buffer RGBA plano.
 */

import { type Genes, decodeGenome } from "../core/genome.ts";
import {
  RAMP_ACCENT,
  RAMP_BASE,
  RAMP_LIGHT,
  RAMP_OUTLINE,
  RAMP_SHADOW,
  type Ramp,
  type Rgb,
  buildRamp,
} from "../core/palette.ts";
import { rngFor } from "../core/rng.ts";
import { detectTraits } from "../core/traits.ts";
import { Mask, PixelBuffer } from "./pixelBuffer.ts";

export const SPRITE_SIZE = 32;

/** Eje de simetría: cae entre las columnas 15 y 16. */
const AXIS = (SPRITE_SIZE - 1) / 2;

/** Línea de piso. Todas las etapas se apoyan acá, así no "flotan" distinto. */
const GROUND = 28;

const SCLERA: Rgb = { r: 248, g: 248, b: 252 };

export type Stage = "bebe" | "juvenil" | "adulto";

export interface Sprite {
  width: number;
  height: number;
  /** RGBA, 4 bytes por píxel — mismo layout que `ImageData`. */
  data: Uint8ClampedArray;
}

interface Shape {
  cy: number;
  rx: number;
  ry: number;
  /** Exponente de la superelipse: 2 = elipse, 4 = casi rectángulo. */
  n: number;
}

interface Archetype {
  name: string;
  head: Shape;
  body: Shape;
  legs: boolean;
}

const ARCHETYPES: readonly Archetype[] = [
  {
    name: "blob",
    head: { cy: 14, rx: 10, ry: 9, n: 2.4 },
    body: { cy: 21, rx: 9, ry: 7, n: 2.6 },
    legs: false,
  },
  {
    name: "chibi",
    head: { cy: 11, rx: 9, ry: 8, n: 2.2 },
    body: { cy: 22, rx: 7, ry: 6, n: 2.4 },
    legs: true,
  },
  {
    name: "alto",
    head: { cy: 9, rx: 6, ry: 6, n: 2.2 },
    body: { cy: 20, rx: 7, ry: 8, n: 2.6 },
    legs: true,
  },
  {
    name: "redondo",
    head: { cy: 14, rx: 11, ry: 10, n: 3 },
    body: { cy: 20, rx: 10, ry: 8, n: 3 },
    legs: false,
  },
  {
    name: "esbelto",
    head: { cy: 10, rx: 6, ry: 7, n: 2 },
    body: { cy: 20, rx: 5, ry: 8, n: 2.2 },
    legs: true,
  },
  {
    name: "cuadrado",
    head: { cy: 12, rx: 8, ry: 7, n: 4 },
    body: { cy: 21, rx: 8, ry: 7, n: 4 },
    legs: true,
  },
  {
    name: "gota",
    head: { cy: 12, rx: 7, ry: 8, n: 2 },
    body: { cy: 21, rx: 9, ry: 7, n: 2.8 },
    legs: false,
  },
  {
    name: "ancho",
    head: { cy: 11, rx: 9, ry: 7, n: 2.6 },
    body: { cy: 20, rx: 11, ry: 8, n: 2.4 },
    legs: true,
  },
];

const STAGE_SCALE: Record<Stage, { overall: number; headBoost: number }> = {
  bebe: { overall: 0.66, headBoost: 1.2 },
  juvenil: { overall: 0.84, headBoost: 1.07 },
  adulto: { overall: 1, headBoost: 1 },
};

// ---------------------------------------------------------------------------
// Geometría
// ---------------------------------------------------------------------------

function inSuperellipse(x: number, y: number, shape: Shape): boolean {
  const dx = Math.abs((x - AXIS) / shape.rx);
  const dy = Math.abs((y - shape.cy) / shape.ry);
  return dx ** shape.n + dy ** shape.n <= 1;
}

/** Escala una forma respecto de la línea de piso, para que siga apoyada. */
function scaleShape(shape: Shape, scale: number): Shape {
  return {
    cy: GROUND - (GROUND - shape.cy) * scale,
    rx: shape.rx * scale,
    ry: shape.ry * scale,
    n: shape.n,
  };
}

function archetypeFor(genes: Genes): Archetype {
  const base = ARCHETYPES[genes.bodyShape % ARCHETYPES.length] ?? ARCHETYPES[0];
  if (!base) throw new Error("No hay arquetipos de cuerpo definidos");
  // El bit alto de bodyShape invierte si tiene patas: duplica la variedad
  // sin necesidad de duplicar la tabla.
  return genes.bodyShape >= 8 ? { ...base, legs: !base.legs } : base;
}

// ---------------------------------------------------------------------------
// Apéndices — se agregan a la máscara para que compartan color y contorno
// ---------------------------------------------------------------------------

function addTriangle(mask: Mask, tipX: number, baseY: number, width: number, height: number): void {
  for (let i = 0; i < height; i++) {
    const halfWidth = ((width * (height - i)) / height) * 0.5;
    for (let dx = -Math.round(halfWidth); dx <= Math.round(halfWidth); dx++) {
      mask.setMirrored(Math.round(tipX + dx), Math.round(baseY - i));
    }
  }
}

function addEllipse(mask: Mask, cx: number, cy: number, rx: number, ry: number): void {
  for (let y = Math.floor(cy - ry); y <= Math.ceil(cy + ry); y++) {
    for (let x = Math.floor(cx - rx); x <= Math.ceil(cx + rx); x++) {
      const dx = (x - cx) / rx;
      const dy = (y - cy) / ry;
      if (dx * dx + dy * dy <= 1) mask.setMirrored(x, y);
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
const MIN_BODY_Y = 2;

/** Baja la base de un apéndice lo justo para que su punta no se salga por arriba. */
function clampApexBase(baseY: number, height: number): number {
  return Math.max(baseY, MIN_BODY_Y + height - 1);
}

/**
 * Baja una forma si su borde superior se pasa del límite.
 *
 * Hace falta además del clamp de apéndices: con proporción máxima, una cabeza
 * alta ya se sale sola, sin necesidad de orejas.
 */
function clampShapeTop(shape: Shape): Shape {
  const overflow = MIN_BODY_Y - (shape.cy - shape.ry);
  return overflow > 0 ? { ...shape, cy: shape.cy + overflow } : shape;
}

function addAppendages(mask: Mask, genes: Genes, head: Shape, body: Shape, legs: boolean): void {
  // bit 0 — orejas
  if ((genes.appendages & 1) !== 0) {
    addTriangle(mask, AXIS - head.rx * 0.55, clampApexBase(head.cy - head.ry * 0.6, 5), 4, 5);
  }
  // bit 1 — cuernos
  if ((genes.appendages & 2) !== 0) {
    addTriangle(mask, AXIS - head.rx * 0.42, clampApexBase(head.cy - head.ry * 0.8, 5), 2, 5);
  }
  // bit 2 — alas
  if ((genes.appendages & 4) !== 0) {
    addEllipse(mask, AXIS - body.rx - 1.5, body.cy - 1, 3, 4.5);
  }
  // bit 3 — aletas laterales (una "cola" espejada serían dos, así que va aleta)
  if ((genes.appendages & 8) !== 0) {
    addTriangle(mask, AXIS - body.rx - 1, body.cy + 3, 3, 3);
  }

  if (legs) {
    const legX = Math.round(AXIS - body.rx * 0.5);
    const legTop = Math.round(body.cy + body.ry * 0.6);
    for (let y = legTop; y <= GROUND + 1; y++) {
      mask.setMirrored(legX, y);
      mask.setMirrored(legX - 1, y);
    }
  }
}

// ---------------------------------------------------------------------------
// Patrones
// ---------------------------------------------------------------------------

function applyPattern(
  buffer: PixelBuffer,
  mask: Mask,
  genes: Genes,
  seed: bigint,
  head: Shape,
  body: Shape,
  ramp: Ramp,
): void {
  const style = genes.pattern % 8;
  const color = genes.pattern >= 8 ? ramp[RAMP_ACCENT] : ramp[RAMP_SHADOW];
  const midY = (head.cy + body.cy) / 2;

  const paint = (x: number, y: number): void => {
    if (mask.has(x, y)) buffer.set(x, y, color);
  };

  switch (style) {
    case 1: {
      // Panza: mancha clara en la mitad baja del cuerpo.
      for (let y = 0; y < SPRITE_SIZE; y++) {
        for (let x = 0; x < SPRITE_SIZE; x++) {
          const dx = (x - AXIS) / (body.rx * 0.62);
          const dy = (y - (body.cy + 1)) / (body.ry * 0.72);
          if (dx * dx + dy * dy <= 1 && mask.has(x, y)) buffer.set(x, y, ramp[RAMP_LIGHT]);
        }
      }
      break;
    }
    case 2: {
      // Rayas horizontales.
      for (let y = 0; y < SPRITE_SIZE; y++) {
        if (y % 5 >= 2) continue;
        for (let x = 0; x < SPRITE_SIZE; x++) paint(x, y);
      }
      break;
    }
    case 3: {
      // Franja vertical por el eje.
      for (let y = 0; y < SPRITE_SIZE; y++) {
        for (let x = Math.round(AXIS - 1); x <= Math.round(AXIS + 2); x++) paint(x, y);
      }
      break;
    }
    case 4: {
      // Manchas en posiciones deterministas derivadas del genoma.
      const rng = rngFor(seed, "manchas");
      const spots = rng.range(3, 5);
      for (let i = 0; i < spots; i++) {
        const cx = rng.range(4, SPRITE_SIZE - 5);
        const cy = rng.range(Math.round(head.cy), GROUND - 2);
        const radius = rng.range(1, 2);
        for (let y = cy - radius; y <= cy + radius; y++) {
          for (let x = cx - radius; x <= cx + radius; x++) {
            if ((x - cx) ** 2 + (y - cy) ** 2 <= radius * radius) paint(x, y);
          }
        }
      }
      break;
    }
    case 5: {
      // Bicolor: mitad de abajo en otro color.
      for (let y = Math.round(midY); y < SPRITE_SIZE; y++) {
        for (let x = 0; x < SPRITE_SIZE; x++) paint(x, y);
      }
      break;
    }
    case 6: {
      // Cachetes.
      const cheekY = Math.round(head.cy + head.ry * 0.2);
      const cheekX = Math.round(AXIS - head.rx * 0.62);
      for (let dy = -1; dy <= 1; dy++) {
        for (let dx = -1; dx <= 1; dx++) {
          if (Math.abs(dx) + Math.abs(dy) > 2) continue;
          if (mask.has(cheekX + dx, cheekY + dy)) {
            buffer.set(cheekX + dx, cheekY + dy, ramp[RAMP_ACCENT]);
          }
          const mirrored = SPRITE_SIZE - 1 - (cheekX + dx);
          if (mask.has(mirrored, cheekY + dy)) {
            buffer.set(mirrored, cheekY + dy, ramp[RAMP_ACCENT]);
          }
        }
      }
      break;
    }
    case 7: {
      // Degradado con tramado (dithering) en la mitad baja.
      for (let y = Math.round(midY); y < SPRITE_SIZE; y++) {
        for (let x = 0; x < SPRITE_SIZE; x++) {
          if ((x + y) % 2 === 0) paint(x, y);
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
const EYE_WIDTHS: readonly number[] = [1, 2, 2, 3, 3, 3, 2, 2];

function drawEye(buffer: PixelBuffer, cx: number, cy: number, style: number, ramp: Ramp): void {
  const dark = ramp[RAMP_OUTLINE];
  const x = Math.round(cx);
  const y = Math.round(cy);

  switch (style) {
    case 0:
      buffer.set(x, y, dark);
      buffer.set(x, y + 1, dark);
      break;
    case 1:
      for (let dy = 0; dy < 2; dy++)
        for (let dx = 0; dx < 2; dx++) buffer.set(x + dx, y + dy, dark);
      break;
    case 2:
      for (let dy = 0; dy < 3; dy++)
        for (let dx = 0; dx < 2; dx++) buffer.set(x + dx, y + dy, dark);
      buffer.set(x, y, SCLERA);
      break;
    case 3:
      for (let dy = 0; dy < 3; dy++) {
        for (let dx = 0; dx < 3; dx++) buffer.set(x + dx, y + dy, SCLERA);
      }
      for (let dy = 1; dy < 3; dy++) {
        for (let dx = 1; dx < 3; dx++) buffer.set(x + dx, y + dy, dark);
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
      for (let dy = 0; dy < 2; dy++)
        for (let dx = 0; dx < 2; dx++) buffer.set(x + dx, y + dy, dark);
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

function drawFace(buffer: PixelBuffer, genes: Genes, head: Shape, ramp: Ramp): void {
  const dark = ramp[RAMP_OUTLINE];
  const style = genes.eyes % 8;
  const eyeY = head.cy - head.ry * 0.15;

  if (style === 6 && genes.eyes >= 8) {
    // Cíclope: un ojo grande centrado en el eje.
    //
    // Va oscuro por fuera y con un brillo blanco adentro, no al revés. Un bloque
    // blanco con pupila chica sobre un cuerpo claro no lee como ojo: lee como un
    // agujero en el sprite.
    //
    // `Math.round(AXIS)` da 16, así que el bloque de 4 arranca en 14 y queda
    // repartido 14-17 alrededor del eje en 15.5. Redondear AXIS - 1 lo correría
    // un píxel a la derecha.
    const left = Math.round(AXIS) - 2;
    const top = Math.round(eyeY);
    for (let dy = 0; dy < 5; dy++) {
      for (let dx = 0; dx < 4; dx++) {
        // Esquinas recortadas para que la silueta lea redonda.
        if ((dy === 0 || dy === 4) && (dx === 0 || dx === 3)) continue;
        buffer.set(left + dx, top + dy, dark);
      }
    }
    buffer.set(left + 1, top + 1, SCLERA);
    buffer.set(left + 2, top + 1, SCLERA);
    buffer.set(left + 1, top + 2, SCLERA);
  } else {
    const spread = Math.max(2.2, head.rx * 0.44);
    const width = EYE_WIDTHS[style] ?? 2;
    const leftX = Math.round(AXIS - spread - 1);
    // El ojo derecho va en la posición espejada del izquierdo, pero se DIBUJA
    // igual, sin espejar. Así el par queda centrado sobre el eje y el brillo cae
    // del mismo lado en los dos, coherente con la dirección de luz del sprite.
    //
    // Trasladarlo (leftX + 2·spread) parece equivalente pero no lo es: con ojos
    // de ancho par deja el par corrido un píxel a la izquierda.
    const rightX = SPRITE_SIZE - 1 - leftX - (width - 1);
    drawEye(buffer, leftX, eyeY, style, ramp);
    drawEye(buffer, rightX, eyeY, style, ramp);
  }

  // Boca
  const mouthY = Math.round(eyeY + Math.max(3, head.ry * 0.45));
  const mx = Math.round(AXIS);
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
      for (let dx = -1; dx <= 1; dx++) buffer.set(mx + dx, mouthY, dark);
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

export function generateSprite(seed: bigint, stage: Stage = "adulto"): Sprite {
  const genes = decodeGenome(seed);
  const traits = detectTraits(seed);
  const ramp = buildRamp(genes, traits);

  const archetype = archetypeFor(genes);
  const { overall, headBoost } = STAGE_SCALE[stage];

  const headScale = (0.88 + (genes.proportion & 0b11) * 0.06) * overall * headBoost;
  const bodyScale = (0.88 + ((genes.proportion >> 2) & 0b11) * 0.06) * overall;
  const head = clampShapeTop(scaleShape(archetype.head, headScale));
  const body = clampShapeTop(scaleShape(archetype.body, bodyScale));

  // 1. Silueta, en media grilla y espejada.
  const mask = new Mask(SPRITE_SIZE, SPRITE_SIZE);
  for (let y = 0; y < SPRITE_SIZE; y++) {
    for (let x = 0; x <= Math.floor(AXIS); x++) {
      if (inSuperellipse(x, y, head) || inSuperellipse(x, y, body)) mask.setMirrored(x, y);
    }
  }
  addAppendages(mask, genes, head, body, archetype.legs);

  // 2. Relleno base.
  const buffer = new PixelBuffer(SPRITE_SIZE, SPRITE_SIZE);
  for (let y = 0; y < SPRITE_SIZE; y++) {
    for (let x = 0; x < SPRITE_SIZE; x++) {
      if (mask.has(x, y)) buffer.set(x, y, ramp[RAMP_BASE]);
    }
  }

  // 3. Patrón.
  applyPattern(buffer, mask, genes, seed, head, body, ramp);

  // 4. Sombreado direccional. Se calcula sobre el buffer YA espejado, así que
  //    la luz cae siempre desde arriba-izquierda y rompe la simetría a propósito.
  for (let y = 0; y < SPRITE_SIZE; y++) {
    for (let x = 0; x < SPRITE_SIZE; x++) {
      if (!mask.has(x, y)) continue;
      if (!mask.has(x, y + 2)) buffer.set(x, y, ramp[RAMP_SHADOW]);
    }
  }
  for (let y = 0; y < SPRITE_SIZE; y++) {
    for (let x = 0; x < SPRITE_SIZE; x++) {
      if (!mask.has(x, y)) continue;
      if (!mask.has(x - 1, y - 2)) buffer.set(x, y, ramp[RAMP_LIGHT]);
    }
  }

  // 5. Contorno por fuera de la silueta, para no comerle tamaño al cuerpo.
  for (let y = 0; y < SPRITE_SIZE; y++) {
    for (let x = 0; x < SPRITE_SIZE; x++) {
      if (mask.has(x, y)) continue;
      if (mask.touchesBody(x, y)) buffer.set(x, y, ramp[RAMP_OUTLINE]);
    }
  }

  // 6. La cara siempre arriba de todo.
  drawFace(buffer, genes, head, ramp);

  return { width: SPRITE_SIZE, height: SPRITE_SIZE, data: buffer.data };
}
