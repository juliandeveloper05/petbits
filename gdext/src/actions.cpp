/**
 * actions.cpp — ver actions.h.
 */

#include "actions.h"

#include <algorithm>

namespace petbits {

const std::array<Food, 4> FOODS = {{
    {"baya", "Baya", "la", "baya", 18, 5, 0, FoodKind::Dulce},
    {"raiz", "Raíz", "la", "raíz", 26, 0, 1, FoodKind::Mineral},
    {"larva", "Larva", "la", "larva", 34, -2, 2, FoodKind::Proteina},
    {"cristal", "Cristal", "el", "cristal", 12, 12, 3, FoodKind::Raro},
}};

// El nombre en minúscula va precalculado en la tabla y no se baja de caso en
// tiempo de ejecución. Pasar "Raíz" a minúscula requiere saber que la Í con
// tilde ocupa dos bytes en UTF-8: std::tolower sobre bytes la rompe, y hacerlo
// bien pide una tabla Unicode para resolver un problema que no existe si el
// texto ya está escrito de las dos formas.

const Food* buscarAlimento(std::string_view id) {
    for (const Food& f : FOODS) {
        if (f.id == id) return &f;
    }
    return nullptr;
}

/** A partir de acá, seguir comiendo rinde poco y empieza a hacer mal. */
static constexpr double FULL_THRESHOLD = 85.0;
static constexpr double OVERFEED_EFFICIENCY = 0.25;
static constexpr double OVERFEED_HEALTH_COST = 2.5;

static constexpr double PLAY_ENERGY_COST = 12.0;
static constexpr double PLAY_MIN_ENERGY = 15.0;
static constexpr double PLAY_MOOD_GAIN = 16.0;

static constexpr double PET_MOOD_GAIN = 4.0;

static constexpr double BOND_PER_ACTION = 2.0;

/** Salir del letargo cuesta vínculo. Se recupera la criatura, no el progreso. */
static constexpr double LETHARGY_BOND_PENALTY = 0.25;

static double clamp01to100(double value) {
    return value < 0.0 ? 0.0 : (value > 100.0 ? 100.0 : value);
}

/**
 * ¿Está de expedición?
 *
 * De expedición no se la puede alimentar ni jugar con ella: no está. Se chequea
 * en cada acción y no dentro de `touch`, para poder devolver el motivo.
 */
static bool ausente(const CreatureState& state) {
    return state.expedicion.has_value();
}

static ActionResult rechazo(std::string motivo) {
    ActionResult r;
    r.ok = false;
    r.message = std::move(motivo);
    return r;
}

/** Copia el estado y aplica lo común a toda interacción. */
static void touch(CreatureState& next, int64_t nowMs, bool& woke) {
    // El tope de vínculo se reinicia por día local, igual que en el tick.
    const int64_t dia = localDayIndex(nowMs, next.tzOffsetMin);
    if (dia != next.diaIndice) {
        next.diaIndice = dia;
        next.vinculoHoy = 0.0;
    }

    woke = next.letargico;
    if (woke) {
        next.letargico = false;
        next.stats.vinculo = std::max(0.0, next.stats.vinculo * (1.0 - LETHARGY_BOND_PENALTY));
    }
    next.ticksSinCuidado = 0;
}

/** Suma vínculo respetando el tope diario. Devuelve cuánto entró de verdad. */
static double grantBond(CreatureState& state, double amount) {
    const double lugar = std::max(0.0, BOND_DAILY_CAP - state.vinculoHoy);
    const double dado = std::min(amount, lugar);
    state.vinculoHoy += dado;
    state.stats.vinculo += dado;
    return dado;
}

static std::string conNotaDeDespertar(std::string mensaje, bool woke) {
    if (!woke) return mensaje;
    return mensaje + " Salió del letargo, pero el vínculo quedó golpeado.";
}

/** Suma uno al contador de dieta que corresponda. */
static void anotarDieta(Crianza& c, FoodKind tipo) {
    switch (tipo) {
        case FoodKind::Proteina: c.dieta_proteina++; break;
        case FoodKind::Dulce:    c.dieta_dulce++;    break;
        case FoodKind::Mineral:  c.dieta_mineral++;  break;
        case FoodKind::Raro:     c.dieta_raro++;     break;
    }
}

// ---------------------------------------------------------------------------

ActionResult alimentar(const CreatureState& state, std::string_view foodId, int64_t nowMs) {
    if (ausente(state)) return rechazo("Está de expedición. Volvé cuando regrese.");

    const Food* food = buscarAlimento(foodId);
    if (food == nullptr) {
        return rechazo("No existe el alimento \"" + std::string(foodId) + "\"");
    }

    CreatureState next = state;
    bool woke = false;
    touch(next, nowMs, woke);

    // Comer de más: rinde una cuarta parte y encima hace mal.
    const bool llena = next.stats.energia >= FULL_THRESHOLD;
    const double ganado = llena ? food->energia * OVERFEED_EFFICIENCY : food->energia;

    next.stats.energia = clamp01to100(next.stats.energia + ganado);
    next.stats.animo = clamp01to100(next.stats.animo + food->animo);
    next.stats.salud =
        clamp01to100(next.stats.salud + food->salud - (llena ? OVERFEED_HEALTH_COST : 0.0));
    grantBond(next, BOND_PER_ACTION);
    // La dieta se registra siempre, aun cuando comió sin ganas: lo que le diste
    // moldea en qué se convierte, más allá de cuánto le rindió esta vez.
    anotarDieta(next.crianza, food->tipo);

    const std::string comida = std::string(food->articulo) + " " + std::string(food->nombreMinuscula);
    const std::string mensaje = llena ? "Picoteó " + comida + " sin ganas. Ya estaba llena."
                                      : "Se morfó " + comida + " sin respirar.";

    ActionResult r;
    r.ok = true;
    r.state = std::move(next);
    r.message = conNotaDeDespertar(mensaje, woke);
    return r;
}

ActionResult jugar(const CreatureState& state, int64_t nowMs) {
    if (ausente(state)) return rechazo("Está de expedición. Volvé cuando regrese.");

    if (state.stats.energia < PLAY_MIN_ENERGY) {
        return rechazo("No le da la energía para jugar. Primero tiene que comer algo.");
    }

    CreatureState next = state;
    bool woke = false;
    touch(next, nowMs, woke);

    // Jugar decaído rinde la mitad: la salud baja se nota en todo.
    const double eficiencia = next.stats.salud < 30.0 ? 0.5 : 1.0;
    next.stats.animo = clamp01to100(next.stats.animo + PLAY_MOOD_GAIN * eficiencia);
    next.stats.energia = clamp01to100(next.stats.energia - PLAY_ENERGY_COST);
    grantBond(next, BOND_PER_ACTION);
    next.crianza.juego++;

    const std::string mensaje = eficiencia < 1.0
                                    ? "Jugó un rato pero se cansó enseguida, pobre."
                                    : "Jugó hasta quedar rendida de contenta.";

    ActionResult r;
    r.ok = true;
    r.state = std::move(next);
    r.message = conNotaDeDespertar(mensaje, woke);
    return r;
}

ActionResult acariciar(const CreatureState& state, int64_t nowMs) {
    if (ausente(state)) return rechazo("Está de expedición. Volvé cuando regrese.");

    CreatureState next = state;
    bool woke = false;
    touch(next, nowMs, woke);

    next.stats.animo = clamp01to100(next.stats.animo + PET_MOOD_GAIN);
    const double dado = grantBond(next, BOND_PER_ACTION);
    // Cuenta como crianza aunque el vínculo ya esté topeado: el tope limita el
    // vínculo, no el hecho de haber estado ahí.
    next.crianza.calma++;

    // Cuando el tope ya está alcanzado se dice, en vez de fingir que sumó.
    const std::string mensaje = dado > 0.0 ? "Se dejó hacer mimos un buen rato."
                                           : "Está a gusto, pero por hoy ya tuvo lo suyo.";

    ActionResult r;
    r.ok = true;
    r.state = std::move(next);
    r.message = conNotaDeDespertar(mensaje, woke);
    return r;
}

} // namespace petbits
