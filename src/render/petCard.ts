/**
 * Pet Card: la criatura como imagen compartible.
 *
 * Es la pieza que hace que el proyecto se vea sin tener que abrirlo. Y como el
 * seed va impreso en la tarjeta, quien la reciba puede incubar exactamente la
 * misma criatura — que es el sentido de que el genoma quepa en dieciséis
 * dígitos.
 *
 * Se dibuja en un canvas y no se arma con HTML porque tiene que salir del
 * navegador como PNG. Todo lo que se ve acá está pintado a mano.
 */

import { formName } from "../core/evolution.ts";
import { STAGE_NAMES } from "../core/evolution.ts";
import { decodeGenome, formatSeed, lineageName, temperamentName } from "../core/genome.ts";
import { RAMP_BASE, buildRamp, rgbToHex } from "../core/palette.ts";
import type { CreatureState } from "../core/simulation.ts";
import { RARITY_LABELS, detectTraits, rarityTier } from "../core/traits.ts";
import { generateSprite } from "./spriteGen.ts";

const ANCHO = 640;
const ALTO = 400;
const MARGEN = 28;
const ESCALA_SPRITE = 9;

const PIXEL = '"Press Start 2P", monospace';
const CUERPO = '"VT323", monospace';

const COLOR = {
  fondo: "#0a0e0a",
  panel: "#0c140d",
  borde: "#3d5c46",
  fosforo: "#9bbc0f",
  fosforoClaro: "#c6ec3a",
  texto: "#d6e6d0",
  tenue: "#7e937a",
} as const;

const COLOR_RAREZA: Record<string, string> = {
  comun: "#7e937a",
  raro: "#4aa3ff",
  epico: "#c07cff",
  legendario: "#ffc23d",
};

/**
 * Espera a que estén las tipografías.
 *
 * Sin esto, la tarjeta se dibuja con la fuente por defecto: el canvas no
 * reintenta cuando la fuente termina de cargar, así que el texto queda mal
 * para siempre en el PNG.
 */
async function esperarTipografias(): Promise<void> {
  try {
    await document.fonts.ready;
  } catch {
    // Si el navegador no expone `document.fonts`, se dibuja igual con la
    // fuente que haya.
  }
}

/**
 * Achica la tipografía hasta que el texto entre en el ancho disponible.
 *
 * Hace falta medir y no confiar en un tamaño fijo: el seed son 19 caracteres y
 * a 16px se salía de la tarjeta. Además cubre el caso de que Press Start 2P no
 * haya cargado y el navegador caiga a otra fuente con otro ancho.
 */
function fijarFuenteQueEntre(
  ctx: CanvasRenderingContext2D,
  texto: string,
  familia: string,
  tamanoIdeal: number,
  anchoMaximo: number,
): void {
  let tamano = tamanoIdeal;
  ctx.font = `${tamano}px ${familia}`;
  while (ctx.measureText(texto).width > anchoMaximo && tamano > 8) {
    tamano -= 1;
    ctx.font = `${tamano}px ${familia}`;
  }
}

function dibujarSprite(ctx: CanvasRenderingContext2D, criatura: CreatureState): void {
  const sprite = generateSprite(BigInt(criatura.seed), criatura.etapa, criatura.forma);

  const intermedio = document.createElement("canvas");
  intermedio.width = sprite.width;
  intermedio.height = sprite.height;
  const intermedioCtx = intermedio.getContext("2d");
  if (!intermedioCtx) return;

  const imagen = intermedioCtx.createImageData(sprite.width, sprite.height);
  imagen.data.set(sprite.data);
  intermedioCtx.putImageData(imagen, 0, 0);

  const lado = sprite.width * ESCALA_SPRITE;
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(intermedio, MARGEN + 12, (ALTO - lado) / 2, lado, lado);
}

/** Dibuja la criatura como tarjeta y devuelve el canvas. */
export async function renderPetCard(criatura: CreatureState): Promise<HTMLCanvasElement> {
  await esperarTipografias();

  const canvas = document.createElement("canvas");
  canvas.width = ANCHO;
  canvas.height = ALTO;
  const ctx = canvas.getContext("2d");
  if (!ctx) throw new Error("No se pudo obtener el contexto 2D de la tarjeta");

  const seed = BigInt(criatura.seed);
  const genes = decodeGenome(seed);
  const traits = detectTraits(seed);
  const ramp = buildRamp(genes, traits, criatura.forma);
  const tier = rarityTier(traits);

  // Fondo, con un resplandor del color de la criatura para que cada tarjeta
  // tenga el tono de su dueña.
  ctx.fillStyle = COLOR.fondo;
  ctx.fillRect(0, 0, ANCHO, ALTO);

  const brillo = ctx.createRadialGradient(200, ALTO / 2, 10, 200, ALTO / 2, 320);
  brillo.addColorStop(0, `${rgbToHex(ramp[RAMP_BASE])}33`);
  brillo.addColorStop(1, "#00000000");
  ctx.fillStyle = brillo;
  ctx.fillRect(0, 0, ANCHO, ALTO);

  ctx.strokeStyle = COLOR.borde;
  ctx.lineWidth = 4;
  ctx.strokeRect(2, 2, ANCHO - 4, ALTO - 4);

  dibujarSprite(ctx, criatura);

  // --------------------------------------------------------------- columna
  const x = 350;
  const anchoColumna = ANCHO - MARGEN - x;
  let y = MARGEN + 34;

  ctx.textBaseline = "alphabetic";
  ctx.fillStyle = COLOR.fosforoClaro;
  const textoSeed = formatSeed(seed);
  fijarFuenteQueEntre(ctx, textoSeed, PIXEL, 16, anchoColumna);
  ctx.fillText(textoSeed, x, y);

  y += 34;
  ctx.fillStyle = COLOR.texto;
  const linaje = `${lineageName(genes)} · ${temperamentName(genes)}`;
  fijarFuenteQueEntre(ctx, linaje, CUERPO, 24, anchoColumna);
  ctx.fillText(linaje, x, y);

  y += 26;
  ctx.fillStyle = COLOR.tenue;
  const forma = criatura.forma === "indefinida" ? "" : ` · ${formName(criatura.forma)}`;
  const etapa = `${STAGE_NAMES[criatura.etapa]}${forma}`;
  fijarFuenteQueEntre(ctx, etapa, CUERPO, 22, anchoColumna);
  ctx.fillText(etapa, x, y);

  y += 30;
  ctx.fillStyle = COLOR_RAREZA[tier] ?? COLOR.tenue;
  ctx.font = `10px ${PIXEL}`;
  ctx.fillText(RARITY_LABELS[tier].toUpperCase(), x, y);

  // Rarezas, una por línea. Se corta a cuatro: más no entra sin apretujar.
  y += 26;
  ctx.font = `20px ${CUERPO}`;
  for (const trait of traits.slice(0, 4)) {
    ctx.fillStyle = COLOR_RAREZA[trait.tier] ?? COLOR.tenue;
    ctx.fillText(`◆ ${trait.name}`, x, y);
    y += 22;
  }
  if (traits.length === 0) {
    ctx.fillStyle = COLOR.tenue;
    ctx.fillText("Sin rarezas.", x, y);
    y += 22;
  }

  // La rampa de color, abajo de la columna.
  const rampaY = ALTO - MARGEN - 58;
  const ancho = 40;
  ramp.forEach((color, i) => {
    ctx.fillStyle = rgbToHex(color);
    ctx.fillRect(x + i * ancho, rampaY, ancho, 20);
  });

  // Firma.
  ctx.fillStyle = COLOR.fosforo;
  ctx.font = `12px ${PIXEL}`;
  ctx.fillText("PetBits", x, ALTO - MARGEN);

  ctx.fillStyle = COLOR.tenue;
  ctx.font = `18px ${CUERPO}`;
  ctx.fillText("criaturas de 64 bits", x + 92, ALTO - MARGEN);

  return canvas;
}

/** Convierte la tarjeta en un archivo PNG. */
export async function petCardFile(criatura: CreatureState): Promise<File> {
  const canvas = await renderPetCard(criatura);

  const blob = await new Promise<Blob | null>((resolve) => {
    canvas.toBlob(resolve, "image/png");
  });
  if (!blob) throw new Error("No se pudo generar el PNG de la tarjeta");

  const nombre = `petbits-${formatSeed(BigInt(criatura.seed))}.png`;
  return new File([blob], nombre, { type: "image/png" });
}
