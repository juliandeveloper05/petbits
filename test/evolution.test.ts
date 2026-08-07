import { describe, expect, it } from "vitest";
import { acariciar, alimentar, jugar } from "../src/core/actions.ts";
import {
  ADULTO_TICKS,
  ADULT_FORMS,
  type Form,
  JUVENIL_TICKS,
  crianzaInicial,
  resolverAdulto,
  resolverJuvenil,
} from "../src/core/evolution.ts";
import { decodeGenome } from "../src/core/genome.ts";
import { RAMP_BASE, RAMP_OUTLINE, buildRamp } from "../src/core/palette.ts";
import { type CreatureState, createCreature, simulate } from "../src/core/simulation.ts";
import { hashPixels } from "../src/render/pixelBuffer.ts";
import { generateSprite } from "../src/render/spriteGen.ts";

const SEED = 0xa3f091c477be2d08n;
const T0 = Date.UTC(2026, 2, 2, 9, 0, 0);
const MINUTE = 60_000;
const HOUR = 60 * MINUTE;

type Estrategia = "cuerpo" | "etereo";

/**
 * Cría una criatura durante varios días con una estrategia consistente.
 *
 * Atiende cada 4 horas, muy por debajo de las 48 del letargo, así que la
 * criatura crece sin interrupciones y lo único que cambia entre estrategias es
 * QUÉ se le da y QUÉ se hace con ella.
 */
function criar(seed: bigint, dias: number, estrategia: Estrategia): CreatureState {
  const comidas = estrategia === "cuerpo" ? ["larva", "raiz"] : ["baya", "cristal"];
  let state = createCreature(seed, T0, 0);
  let visita = 0;

  for (let minuto = 60; minuto <= dias * 24 * 60; minuto += 60) {
    const ahora = T0 + minuto * MINUTE;
    state = simulate(state, ahora).state;

    if (minuto % 240 === 0) {
      const comida = comidas[visita % comidas.length] ?? "larva";
      const fed = alimentar(state, comida, ahora);
      if (fed.ok) state = fed.state;

      const accion = estrategia === "cuerpo" ? jugar(state, ahora) : acariciar(state, ahora);
      if (accion.ok) state = accion.state;
      visita++;
    }
  }

  return state;
}

describe("criterio de la Fase 3", () => {
  /**
   * El test que define la fase.
   *
   * Antes había tres etapas lineales y el jugador no decidía nada: toda
   * criatura terminaba igual. Si esto falla, la crianza volvió a ser decorativa.
   */
  it("el mismo genoma criado al revés termina en adultos distintos", () => {
    const cuerpo = criar(SEED, 6, "cuerpo");
    const etereo = criar(SEED, 6, "etereo");

    expect(cuerpo.etapa).toBe("adulto");
    expect(etereo.etapa).toBe("adulto");
    expect(cuerpo.forma).not.toBe(etereo.forma);
  });

  it("y además se ven distintos", () => {
    // Que la forma sea otra etiqueta no alcanza: tiene que notarse en el sprite.
    const cuerpo = criar(SEED, 6, "cuerpo");
    const etereo = criar(SEED, 6, "etereo");

    const spriteCuerpo = generateSprite(BigInt(cuerpo.seed), "adulto", cuerpo.forma);
    const spriteEtereo = generateSprite(BigInt(etereo.seed), "adulto", etereo.forma);

    expect(hashPixels(spriteCuerpo.data)).not.toBe(hashPixels(spriteEtereo.data));
  });

  it("las cuatro formas adultas son alcanzables", () => {
    // Si alguna quedara inalcanzable, sería contenido muerto en el árbol.
    const genes = decodeGenome(SEED);
    const alcanzadas = new Set<Form>();

    for (const dieta of ["cuerpo", "etereo"] as const) {
      for (const actividad of ["activo", "calmo"] as const) {
        const crianza = crianzaInicial();
        if (dieta === "cuerpo") crianza.dieta.proteina = 20;
        else crianza.dieta.dulce = 20;
        if (actividad === "activo") crianza.juego = 20;
        else crianza.calma = 20;

        const juvenil = resolverJuvenil(crianza, genes);
        alcanzadas.add(resolverAdulto(crianza, genes, juvenil));
      }
    }

    expect([...alcanzadas].sort()).toEqual([...ADULT_FORMS].sort());
  });
});

describe("ritmo de crecimiento", () => {
  it("bebé hasta el primer día activo, después juvenil", () => {
    const antes = criar(SEED, 0.9, "cuerpo");
    expect(antes.etapa).toBe("bebe");
    expect(antes.forma).toBe("indefinida");

    const despues = criar(SEED, 1.2, "cuerpo");
    expect(despues.etapa).toBe("juvenil");
    expect(["petreo", "vaporoso"]).toContain(despues.forma);
  });

  it("adulto recién a los cuatro días activos", () => {
    expect(criar(SEED, 3.5, "cuerpo").etapa).toBe("juvenil");
    expect(criar(SEED, 4.5, "cuerpo").etapa).toBe("adulto");
  });

  it("evolucionar queda registrado como evento", () => {
    let state = createCreature(SEED, T0, 0);
    const result = simulate(state, T0 + 25 * HOUR);
    state = result.state;

    expect(result.summary.evolucion).toBe(1);
    const evento = result.events.find((e) => e.kind === "evolucion");
    expect(evento?.text).toContain("estirón");
  });
});

describe("el abandono no hace crecer", () => {
  /**
   * Los ticks de evolución se cuentan ACTIVOS, no vividos.
   *
   * Si contaran el tiempo total, dejar la criatura tirada una semana la haría
   * llegar a adulta sola, que es el mensaje exactamente contrario al del juego.
   */
  it("en letargo no acumula crecimiento", () => {
    const abandonada = simulate(createCreature(SEED, T0, 0), T0 + 10 * 24 * HOUR).state;

    expect(abandonada.letargico).toBe(true);
    // Alcanzó a ser juvenil en las primeras 24 h, antes del letargo de las 48.
    expect(abandonada.etapa).toBe("juvenil");
    expect(abandonada.ticksActivos).toBeLessThan(ADULTO_TICKS);
    // Diez días de reloj, pero solo dos de vida.
    expect(abandonada.ticksVividos).toBeGreaterThan(abandonada.ticksActivos * 4);
  });

  it("con la salud por el piso no evoluciona", () => {
    let state = createCreature(SEED, T0, 0);
    state = { ...state, stats: { ...state.stats, salud: 20 } };
    const result = simulate(state, T0 + 30 * HOUR);

    expect(result.state.ticksActivos).toBeGreaterThan(JUVENIL_TICKS);
    expect(result.state.etapa).toBe("bebe");
  });
});

describe("qué decide la rama", () => {
  it("la dieta manda sobre el eje somático", () => {
    const genes = decodeGenome(SEED);

    const cuerpo = crianzaInicial();
    cuerpo.dieta.proteina = 10;
    cuerpo.dieta.mineral = 10;
    expect(resolverJuvenil(cuerpo, genes)).toBe("petreo");

    const etereo = crianzaInicial();
    etereo.dieta.dulce = 10;
    etereo.dieta.raro = 10;
    expect(resolverJuvenil(etereo, genes)).toBe("vaporoso");
  });

  it("el genoma sigue pesando: sin crianza, decide la afinidad", () => {
    // Es lo que impide que dos criaturas criadas igual sean siempre iguales.
    const vacia = crianzaInicial();
    const formas = new Set<Form>();
    for (let affinity = 0; affinity < 8; affinity++) {
      const seed = BigInt(affinity) << 41n;
      formas.add(resolverJuvenil(vacia, decodeGenome(seed)));
    }
    expect(formas.size).toBe(2);
  });

  it("jugar tira a activo y acariciar a calmo", () => {
    const genes = decodeGenome(SEED);
    const base = crianzaInicial();
    base.dieta.proteina = 10;

    const activa = { ...base, juego: 15, calma: 0 };
    const calma = { ...base, juego: 0, calma: 15 };

    expect(resolverAdulto(activa, genes, "petreo")).toBe("coloso");
    expect(resolverAdulto(calma, genes, "petreo")).toBe("guardian");
  });

  it("la crianza se registra al alimentar y al interactuar", () => {
    let state = createCreature(SEED, T0, 0);

    const fed = alimentar(state, "larva", T0);
    expect(fed.ok).toBe(true);
    if (!fed.ok) return;
    state = fed.state;
    expect(state.crianza.dieta.proteina).toBe(1);

    const played = jugar(state, T0);
    expect(played.ok).toBe(true);
    if (!played.ok) return;
    expect(played.state.crianza.juego).toBe(1);
  });

  it("las acciones no mutan el estado que reciben", () => {
    // La crianza tiene objetos anidados; una copia superficial los compartiría
    // por referencia y la acción ensuciaría el estado anterior.
    const original = createCreature(SEED, T0, 0);
    const snapshot = JSON.stringify(original);

    alimentar(original, "larva", T0);
    jugar(original, T0);
    acariciar(original, T0);

    expect(JSON.stringify(original)).toBe(snapshot);
  });
});

describe("sprites por forma", () => {
  /**
   * La geometría sola no alcanzaba.
   *
   * Con solo estirar y achatar el cuerpo, las cuatro formas adultas se veían
   * casi idénticas a 32×32: dos píxeles de diferencia no se leen. El color sí,
   * y es lo que hace que la crianza se note de un vistazo.
   */
  it("cada forma tiñe distinto", () => {
    const genes = decodeGenome(SEED);
    const claridad = (rgb: { r: number; g: number; b: number }) => rgb.r + rgb.g + rgb.b;
    const intensidad = (rgb: { r: number; g: number; b: number }) =>
      Math.max(rgb.r, rgb.g, rgb.b) - Math.min(rgb.r, rgb.g, rgb.b);

    const coloso = buildRamp(genes, [], "coloso")[RAMP_BASE];
    const oraculo = buildRamp(genes, [], "oraculo")[RAMP_BASE];

    // Coloso oscurece e intensifica; Oráculo aclara y lava.
    expect(claridad(coloso)).toBeLessThan(claridad(oraculo));
    expect(intensidad(coloso)).toBeGreaterThan(intensidad(oraculo));
  });

  it("teñir por forma no rompe el contraste mínimo", () => {
    // El piso de luminosidad existe porque por debajo de cierto valor ningún
    // contorno alcanza 3:1 contra el cuerpo. Las formas oscuras no pueden
    // saltárselo.
    const luminancia = (rgb: { r: number; g: number; b: number }) => {
      const canal = (v: number) => {
        const c = v / 255;
        return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
      };
      return 0.2126 * canal(rgb.r) + 0.7152 * canal(rgb.g) + 0.0722 * canal(rgb.b);
    };

    for (const form of ["indefinida", ...ADULT_FORMS] as Form[]) {
      for (let i = 0; i < 60; i++) {
        const seed = BigInt(i) * 0x9e3779b97f4a7c15n;
        const ramp = buildRamp(decodeGenome(seed), [], form);
        const claro = luminancia(ramp[RAMP_BASE]);
        const oscuro = luminancia(ramp[RAMP_OUTLINE]);
        const ratio = (claro + 0.05) / (oscuro + 0.05);
        expect(ratio, `${form} con seed ${i} quedó en ${ratio.toFixed(2)}:1`).toBeGreaterThan(3);
      }
    }
  });

  it("cada forma da un sprite distinto", () => {
    const hashes = new Set(
      (["indefinida", ...ADULT_FORMS] as Form[]).map((form) =>
        hashPixels(generateSprite(SEED, "adulto", form).data),
      ),
    );
    expect(hashes.size).toBe(5);
  });

  it("las formas siguen respetando los límites del lienzo", () => {
    // Las ramas pétreas ensanchan el cuerpo: hay que confirmar que el tope de
    // ancho las contiene y no se salen por los costados.
    const desbordes: string[] = [];

    for (const form of ["indefinida", ...ADULT_FORMS] as Form[]) {
      for (let i = 0; i < 40; i++) {
        const seed = BigInt(i) * 0x9e3779b97f4a7c15n;
        const { data } = generateSprite(seed, "adulto", form);
        for (let y = 0; y < 32; y++) {
          for (const x of [0, 31]) {
            if (data[(y * 32 + x) * 4 + 3] !== 0) desbordes.push(`${form} en la columna ${x}`);
          }
        }
      }
    }

    expect([...new Set(desbordes)], `${desbordes.length} píxeles fuera`).toEqual([]);
  });
});
