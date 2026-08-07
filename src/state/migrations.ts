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
 * Cadena de migraciones. `MIGRATIONS[i]` lleva de la versión `i + 1` a la
 * `i + 2`.
 *
 * Vacía a propósito: la versión actual es la 1 y no hay nada anterior. Cuando
 * cambie el formato, se agrega acá una función y se sube SAVE_VERSION.
 */
export const MIGRATIONS: readonly Migration[] = [];

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
