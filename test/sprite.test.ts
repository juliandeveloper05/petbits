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
    for (const seed of sample(400)) {
      for (const stage of STAGES) {
        const sprite = generateSprite(seed, stage);
        expect(sprite.width).toBe(SPRITE_SIZE);
        expect(sprite.height).toBe(SPRITE_SIZE);
        expect(sprite.data.length).toBe(SPRITE_SIZE * SPRITE_SIZE * 4);

        const opaque = opaqueCount(sprite.data);
        const total = SPRITE_SIZE * SPRITE_SIZE;
        // Un cuerpo tiene que ocupar algo, pero no puede tapar todo el lienzo.
        expect(opaque, `seed ${seed.toString(16)} ${stage} quedó casi vacía`).toBeGreaterThan(
          total * 0.06,
        );
        expect(opaque, `seed ${seed.toString(16)} ${stage} desbordó`).toBeLessThan(total * 0.92);
      }
    }
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

  it("la criatura se apoya en la línea de piso y no toca los bordes", () => {
    for (const seed of sample(200)) {
      const { data } = generateSprite(seed, "adulto");
      for (let x = 0; x < SPRITE_SIZE; x++) {
        expect(alphaAt(data, x, 0), `seed ${seed.toString(16)} toca el borde superior`).toBe(0);
        expect(
          alphaAt(data, x, SPRITE_SIZE - 1),
          `seed ${seed.toString(16)} toca el borde inferior`,
        ).toBe(0);
      }
    }
  });
});
