import { describe, expect, it } from "vitest";
import { splitmix64 } from "../src/core/rng.ts";
import { hashPixels } from "../src/render/pixelBuffer.ts";
import { SPRITE_SIZE, type Stage, generateSprite } from "../src/render/spriteGen.ts";

const STAGES: readonly Stage[] = ["bebe", "juvenil", "adulto"];

function sample(count: number): bigint[] {
  const seeds: bigint[] = [];
  let cursor = 0x5591_7e00_0001n;
  for (let i = 0; i < count; i++) {
    cursor = splitmix64(cursor);
    seeds.push(cursor);
  }
  return seeds;
}

function alphaAt(data: Uint8ClampedArray, x: number, y: number): number {
  return data[(y * SPRITE_SIZE + x) * 4 + 3] ?? 0;
}

function opaqueCount(data: Uint8ClampedArray): number {
  let total = 0;
  for (let i = 3; i < data.length; i += 4) {
    if ((data[i] ?? 0) > 0) total++;
  }
  return total;
}

describe("determinismo", () => {
  it("el mismo genoma da exactamente los mismos píxeles", () => {
    for (const seed of sample(300)) {
      const first = generateSprite(seed, "adulto");
      const second = generateSprite(seed, "adulto");
      expect(hashPixels(second.data), `seed ${seed.toString(16)}`).toBe(hashPixels(first.data));
    }
  });

  it("generar otras criaturas en el medio no altera el resultado", () => {
    // Cazaría el bug clásico: un PRNG global compartido entre criaturas, donde
    // el resultado depende de en qué orden se generaron.
    const target = 0xfeed_face_dead_beefn;
    const before = hashPixels(generateSprite(target, "adulto").data);
    for (const other of sample(50)) generateSprite(other, "adulto");
    expect(hashPixels(generateSprite(target, "adulto").data)).toBe(before);
  });

  it("las tres etapas son distintas entre sí", () => {
    for (const seed of sample(50)) {
      const hashes = STAGES.map((stage) => hashPixels(generateSprite(seed, stage).data));
      expect(new Set(hashes).size, `seed ${seed.toString(16)}`).toBe(3);
    }
  });
});

describe("variedad", () => {
  it("genomas distintos dan criaturas distintas", () => {
    const seeds = sample(400);
    const hashes = new Set(seeds.map((seed) => hashPixels(generateSprite(seed, "adulto").data)));
    // No se exige unicidad perfecta: dos genomas pueden coincidir en todos los
    // genes visuales y diferir solo en bits de mutación o de sesgo de stats.
    expect(hashes.size).toBeGreaterThan(seeds.length * 0.95);
  });
});

describe("integridad del sprite", () => {
  it("ninguna criatura sale vacía ni desbordada", () => {
    // Se acumulan los casos malos y se afirma una vez, igual que en los demás
    // tests de este archivo: miles de expect() hacen que solo este tarde
    // varios segundos.
    const total = SPRITE_SIZE * SPRITE_SIZE;
    const malos: string[] = [];

    for (const seed of sample(400)) {
      for (const stage of STAGES) {
        const sprite = generateSprite(seed, stage);
        const donde = `${seed.toString(16)} ${stage}`;

        if (
          sprite.width !== SPRITE_SIZE ||
          sprite.height !== SPRITE_SIZE ||
          sprite.data.length !== total * 4
        ) {
          malos.push(`${donde}: dimensiones incorrectas`);
          continue;
        }

        // Un cuerpo tiene que ocupar algo, pero no puede tapar todo el lienzo.
        const opaque = opaqueCount(sprite.data);
        if (opaque <= total * 0.06) malos.push(`${donde}: casi vacía (${opaque}px)`);
        if (opaque >= total * 0.92) malos.push(`${donde}: desbordada (${opaque}px)`);
      }
    }

    expect(malos.slice(0, 5), `${malos.length} sprites fuera de rango`).toEqual([]);
  });

  /**
   * La silueta se construye en media grilla y se espeja, así que el canal alfa
   * tiene que ser simétrico píxel a píxel.
   *
   * Solo el COLOR rompe la simetría, y a propósito: el sombreado se pinta con la
   * luz fija arriba-izquierda. Si esta prueba falla, algo se está dibujando
   * fuera de la máscara sin su espejo — típicamente un ojo mal centrado.
   */
  it("la silueta es perfectamente simétrica", () => {
    // Se compara a mano y se afirma una sola vez por sprite: un expect() por
    // píxel son 600.000 llamadas y hacen que este test solo tarde 17 segundos.
    const asymmetric: string[] = [];

    for (const seed of sample(400)) {
      for (const stage of STAGES) {
        const { data } = generateSprite(seed, stage);
        for (let y = 0; y < SPRITE_SIZE; y++) {
          for (let x = 0; x < SPRITE_SIZE / 2; x++) {
            if (alphaAt(data, x, y) !== alphaAt(data, SPRITE_SIZE - 1 - x, y)) {
              asymmetric.push(`${seed.toString(16)} ${stage} en (${x}, ${y})`);
              break;
            }
          }
        }
      }
    }

    expect(asymmetric.slice(0, 5), `${asymmetric.length} siluetas asimétricas`).toEqual([]);
  });

  /**
   * Ninguna criatura puede tocar el marco.
   *
   * La primera versión de este test solo miraba las filas 0 y 31, y por eso se
   * le escapó un desborde lateral: alas y aletas se dibujan por fuera del
   * cuerpo, y en un cuerpo ancho caían en la columna 0 y quedaban recortadas.
   * Los cuatro bordes o ninguno.
   */
  it("no toca ninguno de los cuatro bordes", () => {
    // Se acumulan los desbordes y se afirma una sola vez, igual que en el test
    // de simetría: un expect() por píxel son decenas de miles de llamadas y
    // hacen que este test solo tarde varios segundos.
    const desbordes: string[] = [];

    for (const seed of sample(200)) {
      for (const stage of STAGES) {
        const { data } = generateSprite(seed, stage);
        const donde = `${seed.toString(16)} ${stage}`;

        for (let i = 0; i < SPRITE_SIZE; i++) {
          if (alphaAt(data, i, 0) !== 0) desbordes.push(`${donde} arriba`);
          if (alphaAt(data, i, SPRITE_SIZE - 1) !== 0) desbordes.push(`${donde} abajo`);
          if (alphaAt(data, 0, i) !== 0) desbordes.push(`${donde} izquierda`);
          if (alphaAt(data, SPRITE_SIZE - 1, i) !== 0) desbordes.push(`${donde} derecha`);
        }
      }
    }

    expect(
      [...new Set(desbordes)].slice(0, 5),
      `${desbordes.length} píxeles fuera del lienzo`,
    ).toEqual([]);
  });
});
