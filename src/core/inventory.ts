/**
 * La despensa.
 *
 * Hasta acá la comida era infinita: `alimentar()` no consultaba nada y podías
 * dar de comer cien veces seguidas. El principio de "toda acción cuesta algo"
 * estaba implementado a nivel de estadísticas —comer de más hace mal— pero no
 * a nivel de economía, que es donde falta la otra mitad.
 *
 * El inventario es del jugador, no de la criatura: es una despensa, no un
 * estómago. Por eso vive en el estado de la partida y no en cada criatura.
 */

import type { FoodKind } from "./evolution.ts";

export type Inventario = Record<string, number>;

/**
 * Con qué se arranca.
 *
 * Alcanza para los primeros días sin tener que salir de expedición enseguida.
 * El cristal empieza en cero a propósito: es lo raro, y tiene que sentirse así.
 */
export function inventarioInicial(): Inventario {
  return { baya: 3, raiz: 2, larva: 2, cristal: 0 };
}

export function cuanto(inventario: Inventario, id: string): number {
  return inventario[id] ?? 0;
}

export function hay(inventario: Inventario, id: string): boolean {
  return cuanto(inventario, id) > 0;
}

export function total(inventario: Inventario): number {
  return Object.values(inventario).reduce((suma, n) => suma + n, 0);
}

/**
 * Descuenta una unidad. Devuelve `null` si no había.
 *
 * No muta el inventario que recibe: devolver uno nuevo permite que quien llama
 * decida si el cambio se aplica, y evita descontar comida cuando la acción que
 * venía después terminó fallando.
 */
export function consumir(inventario: Inventario, id: string): Inventario | null {
  if (!hay(inventario, id)) return null;
  return { ...inventario, [id]: cuanto(inventario, id) - 1 };
}

export function agregar(inventario: Inventario, id: string, cantidad = 1): Inventario {
  if (cantidad <= 0) return { ...inventario };
  return { ...inventario, [id]: cuanto(inventario, id) + cantidad };
}

/** Suma un botín entero de una vez. */
export function agregarVarios(inventario: Inventario, botin: Inventario): Inventario {
  let resultado = { ...inventario };
  for (const [id, cantidad] of Object.entries(botin)) {
    resultado = agregar(resultado, id, cantidad);
  }
  return resultado;
}

/** Qué tipo de comida es cada id. Lo usa el botín de las expediciones. */
export const TIPO_POR_ALIMENTO: Record<string, FoodKind> = {
  baya: "dulce",
  raiz: "mineral",
  larva: "proteina",
  cristal: "raro",
};
