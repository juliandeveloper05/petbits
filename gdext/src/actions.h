#pragma once
/**
 * actions.h — Port de src/core/actions.ts
 *
 * Regla de diseño que sostiene todo el módulo: **toda acción da algo y cobra
 * algo**. En la versión vieja del juego, alimentar y jugar eran botones que
 * subían barras sin contrapartida, así que no había ninguna decisión que tomar,
 * solo clickear.
 *
 * Acá comer de más pasa factura, jugar gasta energía, y el vínculo tiene tope
 * diario para que la constancia valga más que el spam.
 */

#include "evolution.h"
#include "simulation.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace petbits {

struct Food {
    std::string_view id;
    std::string_view name;
    /** Artículo, para armar frases sin romper la concordancia. */
    std::string_view articulo;
    /** El nombre en minúscula, que es como entra en las frases. */
    std::string_view nombreMinuscula;
    double energia;
    double animo;
    double salud;
    /** Alimenta el vector de crianza que decide la rama evolutiva. */
    FoodKind tipo;
};

extern const std::array<Food, 4> FOODS;

/** Busca un alimento por id. Vacío si no existe. */
const Food* buscarAlimento(std::string_view id);

/**
 * Tope diario de vínculo.
 *
 * Es la regla que convierte "abrir la app" en un hábito en vez de en una sesión
 * de farmeo: alcanzado el tope, seguir interactuando no suma más.
 */
inline constexpr double BOND_DAILY_CAP = 12.0;

/**
 * El resultado de una acción.
 *
 * En el TS es una unión discriminada: o trae estado y mensaje, o trae motivo del
 * rechazo. Acá se representa con `ok` más los dos campos, porque una unión de
 * verdad no aporta nada y complica el puente a GDScript, que igual lo va a leer
 * como diccionario.
 */
struct ActionResult {
    bool ok;
    /** Solo si ok. El estado nuevo; el que se pasó no se toca. */
    CreatureState state;
    /** Si ok, qué pasó. Si no, por qué no se pudo. */
    std::string message;
};

/**
 * Le da de comer. `foodId` es uno de los ids de FOODS.
 *
 * Comer con la panza llena rinde una cuarta parte y encima resta salud. Sin eso,
 * alimentar sería siempre la jugada correcta y no habría decisión.
 */
ActionResult alimentar(const CreatureState& state, std::string_view foodId, int64_t nowMs);

/** Juega con ella. Necesita energía mínima, y con la salud baja rinde la mitad. */
ActionResult jugar(const CreatureState& state, int64_t nowMs);

/** Le hace mimos. Es la acción que nunca se puede rechazar por falta de nada. */
ActionResult acariciar(const CreatureState& state, int64_t nowMs);

} // namespace petbits
