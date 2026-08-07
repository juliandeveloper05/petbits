/**
 * Laboratorio genético.
 *
 * Existe para responder de un vistazo la única pregunta que importa en esta
 * fase: ¿el generador produce criaturas o produce manchas? Sesenta a la vez,
 * regenerables, con el seed a la vista para poder reproducir cualquiera.
 */

import {
  GENOME_LAYOUT,
  type Genes,
  affinityName,
  decodeGenome,
  formatSeed,
  lineageName,
  metabolismName,
  parseSeed,
  randomSeed,
  temperamentName,
} from "../core/genome.ts";
import { buildRamp, paletteModeName, rgbToHex } from "../core/palette.ts";
import { RARITY_LABELS, type Trait, detectTraits, rarityTier } from "../core/traits.ts";
import { drawSprite } from "../render/canvas.ts";
import { type Stage, generateSprite } from "../render/spriteGen.ts";

const GRID_SIZE = 60;
const STAGES: readonly { id: Stage; label: string }[] = [
  { id: "bebe", label: "Bebé" },
  { id: "juvenil", label: "Juvenil" },
  { id: "adulto", label: "Adulto" },
];

/** Un color por gen, para pintar el mapa de bits. */
const GENE_COLORS: Record<string, string> = {
  lineage: "#ff6b6b",
  bodyShape: "#ffa94d",
  eyes: "#ffd43b",
  mouth: "#a9e34b",
  appendages: "#51cf66",
  pattern: "#38d9a9",
  hue: "#3bc9db",
  paletteMode: "#4dabf7",
  temperament: "#748ffc",
  metabolism: "#9775fa",
  affinity: "#da77f2",
  proportion: "#f783ac",
  statBias: "#e8590c",
  mutation: "#868e96",
};

function requireElement<T extends HTMLElement>(id: string): T {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Falta el elemento #${id} en lab.html`);
  return element as T;
}

const detailEl = requireElement<HTMLElement>("detail");
const gridEl = requireElement<HTMLElement>("grid");
const seedInput = requireElement<HTMLInputElement>("seed-input");
const seedForm = requireElement<HTMLFormElement>("seed-form");
const randomBtn = requireElement<HTMLButtonElement>("random-btn");
const rerollBtn = requireElement<HTMLButtonElement>("reroll-btn");

let currentSeed = randomSeed();

// ---------------------------------------------------------------------------
// Detalle
// ---------------------------------------------------------------------------

function geneRow(key: string, value: string): string {
  return `<div class="gene-row"><span class="gene-key">${key}</span><span class="gene-value">${value}</span></div>`;
}

/**
 * Dibuja los 64 bits del genoma coloreados por gen.
 *
 * Los bits van del 0 (menos significativo) al 63, que es el mismo orden en el
 * que están declarados en GENOME_LAYOUT. Es la manera más directa de mostrar
 * que el número no representa a la criatura: la ES.
 */
function renderBitmap(seed: bigint): string {
  const cells: string[] = [];
  for (let i = 0; i < 64; i++) {
    const field = GENOME_LAYOUT.find((entry) => i >= entry.offset && i < entry.offset + entry.bits);
    const color = field ? (GENE_COLORS[field.key] ?? "#868e96") : "#868e96";
    const on = ((seed >> BigInt(i)) & 1n) === 1n;
    const title = field ? `bit ${i} · ${field.key}` : `bit ${i}`;
    cells.push(
      `<span class="bit${on ? " on" : ""}" style="background:${color}" title="${title}"></span>`,
    );
  }
  return `<div class="bitmap">${cells.join("")}</div>`;
}

function renderTraits(traits: readonly Trait[]): string {
  if (traits.length === 0) {
    return `<p class="no-traits">Sin rarezas. Genoma corriente.</p>`;
  }
  const items = traits
    .map(
      (trait) =>
        `<div class="trait tier-${trait.tier}"><span class="trait-name">${trait.name}</span><span class="trait-rule">${trait.rule}</span></div>`,
    )
    .join("");
  return `<div class="traits">${items}</div>`;
}

function statBiasLabel(genes: Genes): string {
  const { vigor, animo, ingenio, vinculo } = genes.statBias;
  return `Vigor ${vigor} · Ánimo ${animo} · Ingenio ${ingenio} · Vínculo ${vinculo}`;
}

function renderDetail(seed: bigint): void {
  const genes = decodeGenome(seed);
  const traits = detectTraits(seed);
  const ramp = buildRamp(genes, traits);
  const tier = rarityTier(traits);

  const swatches = ramp
    .map((color) => `<span class="swatch" style="background:${rgbToHex(color)}"></span>`)
    .join("");

  detailEl.innerHTML = `
    <div class="stages" id="detail-stages"></div>
    <div>
      <p class="detail-seed">${formatSeed(seed)}</p>
      <p class="detail-lineage">${lineageName(genes)} · ${RARITY_LABELS[tier]}</p>
      ${renderBitmap(seed)}
      <div class="gene-table">
        ${geneRow("Temperamento", temperamentName(genes))}
        ${geneRow("Afinidad", affinityName(genes))}
        ${geneRow("Metabolismo", metabolismName(genes))}
        ${geneRow("Paleta", paletteModeName(genes))}
        ${geneRow("Sesgo", statBiasLabel(genes))}
      </div>
      <div class="ramp">${swatches}</div>
      ${renderTraits(traits)}
    </div>
  `;

  const stagesEl = requireElement<HTMLElement>("detail-stages");
  for (const stage of STAGES) {
    const wrapper = document.createElement("div");
    wrapper.className = "stage";

    const canvas = document.createElement("canvas");
    drawSprite(canvas, generateSprite(seed, stage.id), 5);

    const label = document.createElement("span");
    label.className = "stage-name";
    label.textContent = stage.label;

    wrapper.append(canvas, label);
    stagesEl.append(wrapper);
  }
}

// ---------------------------------------------------------------------------
// Grilla
// ---------------------------------------------------------------------------

function buildCard(seed: bigint): HTMLButtonElement {
  const genes = decodeGenome(seed);
  const traits = detectTraits(seed);
  const tier = rarityTier(traits);

  const card = document.createElement("button");
  card.type = "button";
  card.className = `card tier-${tier}`;
  card.dataset.seed = seed.toString();
  card.setAttribute("aria-label", `${lineageName(genes)}, semilla ${formatSeed(seed)}`);

  const canvas = document.createElement("canvas");
  drawSprite(canvas, generateSprite(seed, "adulto"), 3);

  const lineage = document.createElement("span");
  lineage.className = "card-lineage";
  lineage.textContent = lineageName(genes);

  const seedLabel = document.createElement("span");
  seedLabel.className = "card-seed";
  seedLabel.textContent = formatSeed(seed).slice(0, 9);

  card.append(canvas, lineage, seedLabel);
  card.addEventListener("click", () => {
    select(seed);
  });

  return card;
}

function renderGrid(seeds: readonly bigint[]): void {
  const fragment = document.createDocumentFragment();
  for (const seed of seeds) fragment.append(buildCard(seed));
  gridEl.replaceChildren(fragment);
  markActive();
}

function markActive(): void {
  const target = currentSeed.toString();
  for (const card of gridEl.querySelectorAll<HTMLElement>(".card")) {
    card.classList.toggle("is-active", card.dataset.seed === target);
  }
}

function select(seed: bigint): void {
  currentSeed = seed;
  seedInput.value = formatSeed(seed);
  renderDetail(seed);
  markActive();
}

function reroll(): void {
  renderGrid(Array.from({ length: GRID_SIZE }, () => randomSeed()));
}

// ---------------------------------------------------------------------------
// Eventos
// ---------------------------------------------------------------------------

seedForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const raw = seedInput.value.trim();
  if (raw === "") return;
  try {
    select(parseSeed(raw));
  } catch {
    seedInput.setCustomValidity("No pude interpretar esa semilla");
    seedInput.reportValidity();
    seedInput.setCustomValidity("");
  }
});

randomBtn.addEventListener("click", () => {
  select(randomSeed());
});

rerollBtn.addEventListener("click", reroll);

select(currentSeed);
reroll();
