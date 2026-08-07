/**
 * Sonido 8-bit sintetizado en el navegador.
 *
 * Sin archivos de audio y sin dependencias: cada efecto es un oscilador con dos
 * envolventes, una de frecuencia y otra de volumen. Pesa nada, no hay que
 * esperar ninguna descarga, y suena a consola de cuatro botones — que es
 * exactamente la textura del proyecto.
 */

export type SfxName =
  | "click"
  | "comer"
  | "jugar"
  | "mimo"
  | "evolucion"
  | "error"
  | "nacer"
  | "letargo";

interface Tone {
  /** Frecuencia inicial en Hz. */
  freq: number;
  /** Frecuencia final, si el tono se desliza. */
  to?: number;
  /** Duración en segundos. */
  dur: number;
  type?: OscillatorType;
  gain?: number;
  /** Retraso respecto del inicio del efecto. */
  at?: number;
}

const SFX: Record<SfxName, Tone[]> = {
  click: [{ freq: 880, dur: 0.04, type: "square", gain: 0.12 }],
  comer: [
    { freq: 420, to: 620, dur: 0.07, type: "triangle" },
    { freq: 620, to: 880, dur: 0.07, type: "triangle", at: 0.08 },
  ],
  jugar: [
    { freq: 440, dur: 0.06, type: "square" },
    { freq: 554, dur: 0.06, type: "square", at: 0.07 },
    { freq: 659, dur: 0.1, type: "square", at: 0.14 },
  ],
  mimo: [{ freq: 620, to: 880, dur: 0.28, type: "sine", gain: 0.1 }],
  evolucion: [
    { freq: 523, dur: 0.09, type: "square" },
    { freq: 659, dur: 0.09, type: "square", at: 0.1 },
    { freq: 784, dur: 0.09, type: "square", at: 0.2 },
    { freq: 1047, dur: 0.26, type: "square", at: 0.3 },
    { freq: 1568, to: 2093, dur: 0.3, type: "triangle", gain: 0.07, at: 0.34 },
  ],
  error: [{ freq: 300, to: 130, dur: 0.16, type: "sawtooth", gain: 0.1 }],
  nacer: [
    { freq: 392, dur: 0.08, type: "triangle" },
    { freq: 523, dur: 0.08, type: "triangle", at: 0.09 },
    { freq: 784, dur: 0.2, type: "triangle", at: 0.18 },
  ],
  letargo: [{ freq: 330, to: 110, dur: 0.5, type: "sine", gain: 0.09 }],
};

const MUTE_KEY = "petbits:mute";

let ctx: AudioContext | null = null;
let muted = false;

try {
  muted = globalThis.localStorage?.getItem(MUTE_KEY) === "1";
} catch {
  // Modo incógnito o almacenamiento bloqueado: arranca con sonido.
}

/**
 * Prepara el audio. Hay que llamarlo desde un gesto del usuario.
 *
 * Los navegadores no dejan crear ni reanudar un AudioContext sin interacción
 * previa: hacerlo al cargar la página deja el contexto suspendido y ningún
 * sonido se escucha jamás, sin ningún error visible.
 */
export function initAudio(): void {
  if (!ctx) {
    const Ctor = globalThis.AudioContext;
    if (!Ctor) return;
    ctx = new Ctor();
  }
  if (ctx.state === "suspended") void ctx.resume();
}

export function isMuted(): boolean {
  return muted;
}

export function setMuted(value: boolean): void {
  muted = value;
  try {
    globalThis.localStorage?.setItem(MUTE_KEY, value ? "1" : "0");
  } catch {
    // Si no se puede persistir, al menos vale para esta sesión.
  }
}

export function playSfx(name: SfxName): void {
  if (muted || !ctx || ctx.state !== "running") return;

  const now = ctx.currentTime;
  for (const tone of SFX[name]) {
    const start = now + (tone.at ?? 0);
    const end = start + tone.dur;
    const peak = tone.gain ?? 0.14;

    const osc = ctx.createOscillator();
    osc.type = tone.type ?? "square";
    osc.frequency.setValueAtTime(tone.freq, start);
    if (tone.to !== undefined) {
      osc.frequency.exponentialRampToValueAtTime(tone.to, end);
    }

    const amp = ctx.createGain();
    // Ataque corto y caída exponencial. Sin la rampa de entrada se escucha un
    // chasquido al arrancar el oscilador de golpe.
    amp.gain.setValueAtTime(0.0001, start);
    amp.gain.exponentialRampToValueAtTime(peak, start + 0.008);
    amp.gain.exponentialRampToValueAtTime(0.0001, end);

    osc.connect(amp).connect(ctx.destination);
    osc.start(start);
    osc.stop(end + 0.02);
  }
}
