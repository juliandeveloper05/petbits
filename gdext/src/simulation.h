#pragma once
/**
 * simulation.h — Port de src/core/simulation.ts
 *
 * Simulación por marca de tiempo.
 *
 * El tiempo es real. El estado guarda el último tick procesado y al abrir
 * se recalcula todo lo que pasó mientras no estabas.
 *
 * INVARIANTE CENTRAL (mismo que el TS):
 *   simulate(state, t0, t2) == simulate(simulate(state, t0, t1), t1, t2)
 *   para cualquier t1 entre t0 y t2.
 */

#include "evolution.h"
#include "genome.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace petbits {

// ---------------------------------------------------------------------------
// Constantes de tiempo (mirrors exactos de simulation.ts)
// ---------------------------------------------------------------------------

inline constexpr uint64_t TICK_MS         = 60'000ULL;         ///< 1 tick = 1 minuto
inline constexpr uint64_t HOUR_MS         = 3'600'000ULL;
inline constexpr uint64_t DAY_MS          = 86'400'000ULL;
inline constexpr double   LETHARGY_TICKS  = (48.0 * HOUR_MS) / TICK_MS; ///< 2880 ticks

// Tasas de desgaste (mirrors de simulation.ts)
inline constexpr double ENERGY_DRAIN_MIN  = 100.0 / ((48.0 * HOUR_MS) / TICK_MS);
inline constexpr double ENERGY_DRAIN_MAX  = 100.0 / ((12.0 * HOUR_MS) / TICK_MS);
inline constexpr double MOOD_DRAIN        = 100.0 / ((30.0 * HOUR_MS) / TICK_MS);
inline constexpr double HEALTH_DECAY      = 0.012;
inline constexpr double HEALTH_RECOVER    = 0.004;
inline constexpr double LOW_ENERGY        = 25.0;
inline constexpr double LOW_MOOD          = 25.0;
inline constexpr double LOW_HEALTH        = 30.0;

inline constexpr int NIGHT_START_HOUR = 23;
inline constexpr int NIGHT_END_HOUR   = 7;

// ---------------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------------

struct Stats {
    double energia;  ///< 0-100
    double animo;    ///< 0-100
    double salud;    ///< 0-100
    double vinculo;  ///< 0-100+
};

struct Expedicion {
    std::string destinoId;
    uint64_t    salidaMs;
    uint64_t    regresoMs;
};

/** Estado completo de una criatura. Serializable a JSON. */
struct CreatureState {
    std::string seed;          ///< Seed como hex string "A3F0-..."
    std::string nombre;
    Stats       stats;
    Stage       etapa;
    Form        forma;
    Crianza     crianza;
    uint64_t    ticksActivos;  ///< Ticks en los que no estaba en letargo
    uint64_t    ticksSinCuidado;
    bool        letargico;
    std::optional<Expedicion> expedicion;

    uint64_t    lastTickMs;    ///< Timestamp del último tick procesado
};

/** Evento generado durante un tick. */
struct GameEvent {
    enum class Kind : uint8_t {
        LowEnergy, LowMood, LowHealth,
        Evolved, Lethargy, LethargyEnd,
        NightSleep,
    };
    Kind        kind;
    uint64_t    tickMs;
    std::string message;
};

/** Resultado de simulate(). */
struct SimulateResult {
    CreatureState            state;
    std::vector<GameEvent>   events;
    uint64_t                 ticksSkipped; ///< Eventos recortados por MAX_EVENTS
};

// ---------------------------------------------------------------------------
// API principal
// ---------------------------------------------------------------------------

/**
 * Avanza la simulación desde el último tick conocido hasta `nowMs`.
 *
 * Determinista y particionable:
 *   simulate(s, a, c) == simulate(simulate(s, a, b), b, c)
 *   para cualquier a ≤ b ≤ c.
 */
SimulateResult simulate(const CreatureState& state, uint64_t nowMs);

/** Crea el estado inicial de una criatura a partir de su seed. */
CreatureState newCreature(Seed seed, const std::string& nombre, uint64_t nowMs);

/** ¿Cuántos ticks completos hay entre fromMs y toMs? */
uint64_t tickCount(uint64_t fromMs, uint64_t toMs);

/** Índice de día local (para el bond daily cap). */
int localDayIndex(uint64_t timestampMs);

} // namespace petbits
