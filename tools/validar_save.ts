/**
 * Valida un save escrito por el C++ con el cargador REAL de la web.
 *
 *   npm run validar-save -- ruta/al/save.json
 *
 * ---
 *
 * Es la única prueba de que el nativo escribe algo que la web puede leer.
 *
 * Los tests de C++ comprueban que el C++ lee sus propios archivos, y eso es
 * fácil de pasar estando equivocado: basta con equivocarse igual al leer y al
 * escribir. Acá el archivo lo revisa `parseSave`, que no sabe nada de ese
 * código — el mismo esquema de Zod, las mismas migraciones y la misma
 * validación cruzada de `activaId` que corren cuando alguien abre su partida en
 * el navegador.
 *
 * Si esto pasa, un save del nativo se abre en la web. Si no, dice exactamente
 * qué campo está mal.
 */

import { readFileSync } from "node:fs";
import { parseSave } from "../src/state/save.ts";

const ruta = process.argv[2];
if (!ruta) {
  console.error("Falta la ruta del archivo.\n  npm run validar-save -- ruta/al/save.json");
  process.exit(2);
}

let crudo: unknown;
try {
  crudo = JSON.parse(readFileSync(ruta, "utf8"));
} catch (error) {
  console.error(`No se pudo leer ${ruta} como JSON:`);
  console.error(`  ${error instanceof Error ? error.message : String(error)}`);
  process.exit(1);
}

const resultado = parseSave(crudo);

if (!resultado.ok) {
  console.error(`\nEl save del C++ NO pasa el validador de la web:\n  ${resultado.reason}\n`);
  process.exit(1);
}

const save = resultado.save;
const activa = save.criaturas.find((c) => c.id === save.activaId);

console.log(`\n${ruta} es un save válido para la web.`);
console.log(`  versión ${save.version}, ${save.criaturas.length} criatura(s)`);

if (activa) {
  console.log(`  activa: ${activa.id}`);
  console.log(`  ${activa.etapa} · ${activa.forma} · ${activa.ticksVividos} ticks vividos`);
  // Se imprimen con todos los dígitos a propósito: si el guardado hubiera
  // perdido precisión, acá se vería un número redondo donde tiene que haber uno
  // con quince decimales.
  console.log(`  energía ${activa.stats.energia}`);
  console.log(`  salud   ${activa.stats.salud}`);
  console.log(`  crianza.sumaSalud ${activa.crianza.sumaSalud}`);
}

console.log(`  codex: ${save.codex.totalRegistradas} registradas`);
console.log(`  semillas: ${save.semillas.join(", ") || "(ninguna)"}`);
console.log();
