/**
 * El juego.
 *
 * Capa de presentación: lee el reloj, dibuja y guarda. Toda la lógica vive en
 * `src/core`, que es puro y no sabe que existe un navegador.
 *
 * Una aclaración sobre el `setInterval` de abajo, porque en la versión anterior
 * había uno y era el bug central: acá NO es la fuente de verdad del tiempo. La
 * criatura envejece por marca de tiempo, y este intervalo solo pregunta cada
 * tanto "¿pasó algún minuto nuevo?". Si el navegador lo pausa una hora, el
 * siguiente disparo pone al día esa hora completa de una.
 */

import { FOODS, acariciar, alimentar, jugar } from "../core/actions.ts";
import { STAGE_NAMES, formDescription, formName } from "../core/evolution.ts";
import {
  decodeGenome,
  formatSeed,
  lineageName,
  parseSeed,
  randomSeed,
  temperamentName,
} from "../core/genome.ts";
import {
  type CreatureState,
  buildAbsenceDigest,
  createCreature,
  localHour,
  simulate,
} from "../core/simulation.ts";
import { drawSprite } from "../render/canvas.ts";
import { generateSprite } from "../render/spriteGen.ts";
import { loadGame, saveGame } from "../state/persistence.ts";
import { initAudio, isMuted, playSfx, setMuted } from "./audio.ts";

const SPRITE_SCALE = 6;
/** Cada cuánto se le pregunta al reloj si pasó un minuto nuevo. */
const POLL_MS = 5000;
const BLINK_EVERY_MS = 4200;
const BLINK_DURATION_MS = 150;

function el<T extends HTMLElement>(id: string): T {
  const node = document.getElementById(id);
  if (!node) throw new Error(`Falta #${id} en index.html`);
  return node as T;
}

const screens = {
  nacer: el<HTMLElement>("screen-nacer"),
  cuidado: el<HTMLElement>("screen-cuidado"),
};
const petCanvas = el<HTMLCanvasElement>("pet-canvas");
const petSprite = el<HTMLElement>("pet-sprite");
const petReaction = el<HTMLElement>("pet-reaction");
const petMood = el<HTMLElement>("pet-mood");
const petLineage = el<HTMLElement>("pet-lineage");
const petForm = el<HTMLElement>("pet-form");
const petStage = el<HTMLElement>("pet-stage");
const statsEl = el<HTMLElement>("stats");
const seedLabel = el<HTMLElement>("seed-label");
const toastEl = el<HTMLElement>("toast");
const muteBtn = el<HTMLButtonElement>("mute-btn");
const controls = document.querySelector<HTMLElement>(".controls");

const hatchCanvas = el<HTMLCanvasElement>("hatch-canvas");
const hatchInfo = el<HTMLElement>("hatch-info");
const hatchForm = el<HTMLFormElement>("hatch-form");
const hatchSeed = el<HTMLInputElement>("hatch-seed");
const hatchRandom = el<HTMLButtonElement>("hatch-random");

const foodModal = el<HTMLElement>("food-modal");
const foodGrid = el<HTMLElement>("food-grid");
const digestModal = el<HTMLElement>("digest-modal");
const digestHeadline = el<HTMLElement>("digest-headline");
const digestList = el<HTMLElement>("digest-list");

let state: CreatureState | null = null;
let hatchCandidate = randomSeed();
let blinking = false;
// `ReturnType` y no `number`: con los tipos de Node en el proyecto (los trae
// Vitest), setTimeout devuelve Timeout y no un entero.
let toastTimer: ReturnType<typeof setTimeout> | undefined;
let reactionTimer: ReturnType<typeof setTimeout> | undefined;

// ---------------------------------------------------------------------------
// Presentación
// ---------------------------------------------------------------------------

function showScreen(name: keyof typeof screens): void {
  for (const [key, node] of Object.entries(screens)) {
    node.hidden = key !== name;
  }
  if (controls) controls.hidden = name !== "cuidado";
}

function toast(message: string, kind: "info" | "error" = "info"): void {
  toastEl.textContent = message;
  toastEl.classList.toggle("is-error", kind === "error");
  toastEl.classList.add("show");
  globalThis.clearTimeout(toastTimer);
  toastTimer = globalThis.setTimeout(() => toastEl.classList.remove("show"), 2800);
}

function reaction(symbol: string): void {
  petReaction.textContent = symbol;
  petReaction.classList.remove("show");
  // Reiniciar la animación requiere forzar un reflow; sin esto, dos reacciones
  // seguidas no vuelven a disparar el keyframe.
  void petReaction.offsetWidth;
  petReaction.classList.add("show");
  globalThis.clearTimeout(reactionTimer);
  reactionTimer = globalThis.setTimeout(() => petReaction.classList.remove("show"), 1400);
}

const STAT_DEFS = [
  { key: "energia", label: "Energía", color: "var(--energia)" },
  { key: "animo", label: "Ánimo", color: "var(--animo)" },
  { key: "salud", label: "Salud", color: "var(--salud)" },
  { key: "vinculo", label: "Vínculo", color: "var(--vinculo)" },
] as const;

function moodText(creature: CreatureState): string {
  if (creature.letargico) return "Está en letargo. Con que la atiendas alcanza.";
  if (creature.durmiendo) return "Está durmiendo. Son las horas.";
  if (creature.stats.salud < 30) return "No se la ve nada bien.";
  if (creature.stats.energia < 25) return "Le suena la panza.";
  if (creature.stats.animo < 25) return "Se está aburriendo mal.";
  if (creature.stats.animo > 72 && creature.stats.energia > 60) return "Anda pipí cucú.";
  return "Anda tranquila.";
}

function renderStats(creature: CreatureState): void {
  statsEl.replaceChildren(
    ...STAT_DEFS.flatMap((def) => {
      const raw = creature.stats[def.key];
      // El vínculo no tiene techo, así que la barra se satura en 100 pero el
      // número sigue subiendo. Mostrar una barra llena y un número que crece es
      // más honesto que inventarle un máximo.
      const percent = Math.min(100, raw);

      const row = document.createElement("div");
      row.className = `stat${percent < 25 ? " is-low" : ""}`;

      const name = document.createElement("dt");
      name.className = "stat-name";
      name.textContent = def.label;

      const track = document.createElement("dd");
      track.className = "stat-track";
      const fill = document.createElement("div");
      fill.className = "stat-fill";
      fill.style.width = `${percent}%`;
      fill.style.background = def.color;
      track.append(fill);

      const value = document.createElement("dd");
      value.className = "stat-value";
      value.textContent = String(Math.round(raw));

      row.append(name, track, value);
      return [row];
    }),
  );
}

function renderPet(): void {
  if (!state) return;
  const seed = BigInt(state.seed);
  const genes = decodeGenome(seed);

  drawSprite(
    petCanvas,
    generateSprite(seed, state.etapa, state.forma, blinking ? "parpadeo" : "normal"),
    SPRITE_SCALE,
  );

  petLineage.textContent = `${lineageName(genes)} · ${temperamentName(genes)}`;
  petForm.textContent =
    state.forma === "indefinida" ? formDescription("indefinida") : formName(state.forma);
  petStage.textContent = STAGE_NAMES[state.etapa];
  petMood.textContent = moodText(state);
  petSprite.classList.toggle("is-lethargic", state.letargico);
  seedLabel.textContent = formatSeed(seed);

  renderStats(state);
}

// ---------------------------------------------------------------------------
// Modales
// ---------------------------------------------------------------------------

function closeModals(): void {
  foodModal.hidden = true;
  digestModal.hidden = true;
}

function openFoodModal(): void {
  foodGrid.replaceChildren(
    ...FOODS.map((food) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "food-item";

      const name = document.createElement("span");
      name.className = "food-name";
      name.textContent = food.name;

      const stats = document.createElement("span");
      stats.className = "food-stats";
      const parts = [`+${food.energia} energía`];
      if (food.animo !== 0) parts.push(`${food.animo > 0 ? "+" : ""}${food.animo} ánimo`);
      if (food.salud !== 0) parts.push(`+${food.salud} salud`);
      stats.textContent = parts.join(" · ");

      button.append(name, stats);
      button.addEventListener("click", () => {
        closeModals();
        void act("alimentar", food.id);
      });
      return button;
    }),
  );
  foodModal.hidden = false;
  foodGrid.querySelector("button")?.focus();
}

function showDigest(
  creature: CreatureState,
  digest: NonNullable<ReturnType<typeof buildAbsenceDigest>>,
): void {
  digestHeadline.textContent = digest.headline;
  digestList.replaceChildren(
    ...digest.highlights.map((event) => {
      const item = document.createElement("li");
      if (event.kind === "evolucion" || event.kind === "letargo") item.classList.add("is-key");

      const when = document.createElement("span");
      when.className = "digest-when";
      when.textContent = `${String(localHour(event.atMs, creature.tzOffsetMin)).padStart(2, "0")}:00`;

      const text = document.createElement("span");
      text.textContent = event.text;

      item.append(when, text);
      return item;
    }),
  );
  digestModal.hidden = false;
  digestModal.querySelector<HTMLButtonElement>("[data-close-modal]")?.focus();
}

// ---------------------------------------------------------------------------
// Bucle
// ---------------------------------------------------------------------------

async function persist(): Promise<void> {
  if (!state) return;
  try {
    await saveGame(state, Date.now());
  } catch (error) {
    toast("No se pudo guardar la partida.", "error");
    console.error("Fallo al guardar", error);
  }
}

/** Pone la simulación al día. Devuelve los eventos nuevos, si hubo. */
function catchUp(): ReturnType<typeof simulate> | null {
  if (!state) return null;
  const result = simulate(state, Date.now());
  state = result.state;
  return result;
}

function announce(result: ReturnType<typeof simulate>): void {
  for (const event of result.events) {
    if (event.kind === "evolucion") {
      playSfx("evolucion");
      reaction("✨");
      toast(event.text);
    }
  }
  if ((result.summary.letargo ?? 0) > 0) playSfx("letargo");
}

function tick(): void {
  const result = catchUp();
  if (!result || result.ticks === 0) return;
  renderPet();
  announce(result);
  void persist();
}

async function act(kind: "alimentar" | "jugar" | "mimo", foodId?: string): Promise<void> {
  if (!state) return;
  initAudio();

  // Ponerse al día ANTES de actuar: si no, la acción se aplicaría sobre un
  // estado viejo y el tiempo transcurrido se descontaría después, pisándola.
  catchUp();
  if (!state) return;

  const now = Date.now();
  const result =
    kind === "alimentar"
      ? alimentar(state, foodId ?? "", now)
      : kind === "jugar"
        ? jugar(state, now)
        : acariciar(state, now);

  if (!result.ok) {
    playSfx("error");
    toast(result.reason, "error");
    return;
  }

  state = result.state;
  playSfx(kind === "alimentar" ? "comer" : kind === "jugar" ? "jugar" : "mimo");
  reaction(kind === "alimentar" ? "🍖" : kind === "jugar" ? "🎾" : "💚");
  petSprite.classList.remove("is-acting");
  void petSprite.offsetWidth;
  petSprite.classList.add("is-acting");

  renderPet();
  toast(result.message);
  await persist();
}

function startBlinking(): void {
  globalThis.setInterval(() => {
    if (!state || state.durmiendo || state.letargico || screens.cuidado.hidden) return;
    blinking = true;
    renderPet();
    globalThis.setTimeout(() => {
      blinking = false;
      renderPet();
    }, BLINK_DURATION_MS);
  }, BLINK_EVERY_MS);
}

// ---------------------------------------------------------------------------
// Nacimiento
// ---------------------------------------------------------------------------

function renderHatchPreview(): void {
  const genes = decodeGenome(hatchCandidate);
  drawSprite(hatchCanvas, generateSprite(hatchCandidate, "bebe"), SPRITE_SCALE);
  hatchInfo.textContent = `${lineageName(genes)} · ${temperamentName(genes)} · ${formatSeed(hatchCandidate)}`;
}

function setHatchSeed(seed: bigint): void {
  hatchCandidate = seed;
  renderHatchPreview();
}

async function hatch(): Promise<void> {
  initAudio();
  state = createCreature(hatchCandidate, Date.now(), -new Date().getTimezoneOffset());
  await persist();
  playSfx("nacer");
  showScreen("cuidado");
  renderPet();
  toast("Nació. Ahora es tuya.");
}

// ---------------------------------------------------------------------------
// Arranque
// ---------------------------------------------------------------------------

function wireEvents(): void {
  for (const button of document.querySelectorAll<HTMLButtonElement>("[data-action]")) {
    button.addEventListener("click", () => {
      initAudio();
      const action = button.dataset.action;
      if (action === "alimentar") {
        playSfx("click");
        openFoodModal();
      } else if (action === "jugar" || action === "mimo") {
        void act(action);
      }
    });
  }

  for (const button of document.querySelectorAll<HTMLElement>("[data-close-modal]")) {
    button.addEventListener("click", closeModals);
  }
  for (const modal of [foodModal, digestModal]) {
    modal.addEventListener("click", (event) => {
      if (event.target === modal) closeModals();
    });
  }
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") closeModals();
  });

  muteBtn.setAttribute("aria-pressed", String(isMuted()));
  muteBtn.addEventListener("click", () => {
    initAudio();
    setMuted(!isMuted());
    muteBtn.setAttribute("aria-pressed", String(isMuted()));
    if (!isMuted()) playSfx("click");
  });

  hatchRandom.addEventListener("click", () => {
    initAudio();
    playSfx("click");
    hatchSeed.value = "";
    setHatchSeed(randomSeed());
  });

  hatchSeed.addEventListener("input", () => {
    const raw = hatchSeed.value.trim();
    if (raw === "") return;
    try {
      setHatchSeed(parseSeed(raw));
    } catch {
      // Mientras escribe puede quedar en un estado no interpretable; se ignora
      // y se conserva la última criatura válida.
    }
  });

  hatchForm.addEventListener("submit", (event) => {
    event.preventDefault();
    void hatch();
  });

  // Al volver a la pestaña se pone al día en el acto, sin esperar al intervalo.
  document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "visible") tick();
    else void persist();
  });
  globalThis.addEventListener("pagehide", () => void persist());
}

async function boot(): Promise<void> {
  wireEvents();
  startBlinking();
  globalThis.setInterval(tick, POLL_MS);

  const loaded = await loadGame();

  if (loaded.status === "corrupto") {
    // No se borra nada: el save quedó en cuarentena y se puede rescatar.
    toast("La partida guardada no se pudo leer. Empezá una nueva.", "error");
    console.error("Guardado corrupto:", loaded.reason);
  }

  if (loaded.status !== "ok") {
    setHatchSeed(randomSeed());
    showScreen("nacer");
    return;
  }

  state = loaded.save.criatura;
  const result = catchUp();
  showScreen("cuidado");
  renderPet();

  if (result) {
    const digest = buildAbsenceDigest(result);
    if (digest && state) showDigest(state, digest);
    announce(result);
    if (result.ticks > 0) void persist();
  }
}

void boot();
