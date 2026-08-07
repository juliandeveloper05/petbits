/**
 * Migraciones del formato de guardado.
 *
 * El save se versiona desde el día uno, aunque hoy solo exista la versión 1.
 * Agregar el versionado después, cuando ya hay partidas guardadas afuera, es
 * imposible sin perder datos: no hay forma de saber qué formato tiene un blob
 * que no dice cuál es.
 */

export type RawSave = Record<string, unknown>;
export type Migration = (save: RawSave) => RawSave;

/**
 * Umbrales de evolución CONGELADOS al momento de escribir la migración v1→v2.
 *
 * No se importan de `evolution.ts` a propósito. Una migración tiene que
 * interpretar los saves viejos siempre igual: si mañana se rebalancean los
 * umbrales del juego, un save v1 migrado el año que viene debe dar el mismo
 * resultado que uno migrado hoy. Importar la constante viva ataría el pasado a
 * decisiones futuras.
 */
const V2_JUVENIL_TICKS = 24 * 60;
const V2_ADULTO_TICKS = 4 * 24 * 60;

/**
 * v1 → v2: incorpora la crianza y la etapa evolutiva.
 *
 * La versión 1 no registraba nada de cómo se criaba a la criatura, así que ese
 * historial se pierde: arranca en cero. La etapa sí se puede reconstruir a
 * partir de los ticks vividos.
 *
 * Se usa `ticksVividos` como aproximación de `ticksActivos` porque no hay forma
 * de saber cuánto tiempo pasó en letargo. Es la estimación más favorable para
 * el jugador, que es el sesgo correcto cuando se adivina.
 */
function v1ToV2(save: RawSave): RawSave {
  const criatura = (save.criatura ?? {}) as RawSave;
  const vividos = typeof criatura.ticksVividos === "number" ? criatura.ticksVividos : 0;

  const etapa =
    vividos >= V2_ADULTO_TICKS ? "adulto" : vividos >= V2_JUVENIL_TICKS ? "juvenil" : "bebe";

  return {
    ...save,
    criatura: {
      ...criatura,
      ticksActivos: vividos,
      etapa,
      // Sin historial de crianza no hay forma honesta de elegir una rama, así
      // que queda indefinida. Es información que la v1 nunca tuvo.
      forma: "indefinida",
      crianza: {
        dieta: { proteina: 0, dulce: 0, mineral: 0, raro: 0 },
        juego: 0,
        calma: 0,
        sumaAnimo: 0,
        sumaSalud: 0,
        ticksMedidos: 0,
      },
    },
  };
}

/**
 * Cadena de migraciones. `MIGRATIONS[i]` lleva de la versión `i + 1` a la
 * `i + 2`.
 */
export const MIGRATIONS: readonly Migration[] = [v1ToV2];

export class SaveVersionError extends Error {}

/**
 * Lleva un save de `fromVersion` a `toVersion` aplicando la cadena en orden.
 *
 * `chain` es un parámetro y no una constante importada para que los tests
 * puedan verificar el mecanismo con una cadena propia, sin depender de que
 * existan migraciones reales.
 */
export function applyMigrations(
  raw: RawSave,
  fromVersion: number,
  toVersion: number,
  chain: readonly Migration[] = MIGRATIONS,
): RawSave {
  if (!Number.isInteger(fromVersion) || fromVersion < 1) {
    throw new SaveVersionError(`Versión de guardado inválida: ${fromVersion}`);
  }
  if (fromVersion > toVersion) {
    // Pasa si el usuario abrió una versión más nueva del juego y volvió a una
    // vieja. Degradar a ciegas rompería datos, así que se rechaza.
    throw new SaveVersionError(
      `El guardado es de una versión más nueva (${fromVersion}) que la soportada (${toVersion})`,
    );
  }

  let current = raw;
  for (let version = fromVersion; version < toVersion; version++) {
    const migration = chain[version - 1];
    if (!migration) {
      throw new SaveVersionError(`Falta la migración de la versión ${version} a la ${version + 1}`);
    }
    current = migration(current);
  }

  return { ...current, version: toVersion };
}
