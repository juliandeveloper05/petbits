#pragma once
/**
 * simulation.h — Port de src/core/simulation.ts
 *
 * El tiempo es real. El estado guarda la frontera del último tick procesado y
 * al abrir se recalcula todo lo que pasó mientras no estabas.
 *
 * ---
 *
 * INVARIANTE CENTRAL: simular un intervalo de una sola vez tiene que dar
 * exactamente lo mismo que simularlo en pedazos.
 *
 * Se sostiene con tres reglas, y las tres sobreviven al port:
 *
 * 1. El tiempo avanza SOLO en ticks enteros de un minuto. Lo que sobra queda
 *    pendiente para la próxima llamada.
 * 2. Cada tick lee la hora de su propia frontera, nunca del reloj del sistema.
 * 3. El azar se siembra por índice de tick, no por un flujo que se arrastra.
 *
 * ---
 *
 * POR QUÉ LOS TIEMPOS SON int64_t Y NO uint64_t.
 *
 * El header anterior los tenía sin signo y no podía funcionar. `simulate`
 * empieza preguntando si `nowMs < state.lastTickMs` para detectar el reloj
 * corrido hacia atrás: con enteros sin signo esa resta da la vuelta y un
 * retroceso de un minuto se lee como dieciocho trillones de milisegundos hacia
 * adelante. El número de JS tiene signo, y esto también.
 */

#include "evolution.h"
#include "genome.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace petbits {

// ---------------------------------------------------------------------------
// Constantes de tiempo — mismos valores que simulation.ts
// ---------------------------------------------------------------------------

inline constexpr int64_t TICK_MS = 60'000;
inline constexpr int64_t HOUR_MS = 3'600'000;
inline constexpr int64_t DAY_MS = 86'400'000;

/** Sin atención, a las 48 horas entra en letargo. */
inline constexpr int64_t LETHARGY_TICKS = (48 * HOUR_MS) / TICK_MS;

inline constexpr int NIGHT_START_HOUR = 23;
inline constexpr int NIGHT_END_HOUR = 7;

/** Techo de eventos devueltos. Lo recortado se informa, no se oculta. */
inline constexpr size_t MAX_EVENTS = 60;

// Desgaste por tick. Aletargado agota 100 puntos de energía en ~48 h;
// Frenético, en ~12 h.
inline constexpr double ENERGY_DRAIN_MIN = 100.0 / ((48.0 * HOUR_MS) / TICK_MS);
inline constexpr double ENERGY_DRAIN_MAX = 100.0 / ((12.0 * HOUR_MS) / TICK_MS);
inline constexpr double MOOD_DRAIN = 100.0 / ((30.0 * HOUR_MS) / TICK_MS);
inline constexpr double HEALTH_DECAY = 0.012;
inline constexpr double HEALTH_RECOVER = 0.004;

inline constexpr double LOW_ENERGY = 25.0;
inline constexpr double LOW_MOOD = 25.0;
inline constexpr double LOW_HEALTH = 30.0;

// ---------------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------------

struct Stats {
    double energia;
    double animo;
    double salud;
    double vinculo;
};

/** Una salida en curso. Va en el estado porque forma parte de él. */
struct Expedicion {
    std::string destinoId;
    int64_t salidaMs;
    int64_t regresoMs;
};

struct CreatureState {
    /**
     * Identificador único dentro de la partida.
     *
     * La semilla no alcanza: dos criaturas pueden compartir genoma —al cruzar
     * puede repetirse, o podés adoptar la misma semilla dos veces— y con una
     * colección hay que poder distinguirlas.
     */
    std::string id;

    /**
     * El genoma.
     *
     * En el TS se guarda como cadena decimal porque JSON no sabe representar un
     * bigint. Acá se guarda como número y la conversión vive en el borde: ver
     * `seedADecimal`, que es la que arma el id y la que va a usar el guardado.
     */
    Seed seed;

    int64_t nacimientoMs;
    /** Frontera del último tick procesado. */
    int64_t lastTickMs;
    /** Ticks vividos. Nunca baja, ni con el reloj hacia atrás. */
    int64_t ticksVividos;

    /**
     * Minutos de desfasaje horario, guardados en el estado.
     *
     * La hora local NO se lee del sistema al simular: si lo hiciera, la misma
     * partida daría resultados distintos en otra zona horaria y el invariante
     * de composición se rompería.
     */
    int tzOffsetMin;

    Stats stats;
    /** Vínculo ganado en el día en curso, para poder toparlo. */
    double vinculoHoy;
    /** Índice de día local, para saber cuándo reiniciar `vinculoHoy`. */
    int64_t diaIndice;
    int64_t ticksSinCuidado;
    bool letargico;
    bool durmiendo;
    /** Ticks activos, sin contar el letargo. Es el reloj que rige la evolución. */
    int64_t ticksActivos;
    Stage etapa;
    Form forma;
    Crianza crianza;
    /** Cuándo cruzó por última vez. Vacío si nunca lo hizo. */
    std::optional<int64_t> ultimaCruzaMs;
    std::optional<Expedicion> expedicion;
};

enum class SimEventKind : uint8_t {
    Hambre,
    Animo,
    Salud,
    Durmio,
    Desperto,
    Letargo,
    Hallazgo,
    Evolucion,
    Reloj,
};

/** El identificador textual del evento, igual que el del TS ("hambre", …). */
std::string_view simEventKindId(SimEventKind kind);

struct SimEvent {
    SimEventKind kind;
    int64_t atMs;
    std::string text;
};

struct SimResult {
    CreatureState state;
    /** Eventos del intervalo, recortados a MAX_EVENTS (los más recientes). */
    std::vector<SimEvent> events;
    /** Cuántos hubo de cada tipo, sin recortar. Clave = id textual. */
    std::map<std::string, int64_t> summary;
    /** Eventos descartados por el techo. */
    int64_t omitted;
    int64_t ticks;
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/** El genoma como cadena decimal, que es la forma en que lo guarda el TS. */
std::string seedADecimal(Seed seed);

/** Arma el identificador de una criatura. Estable para los mismos argumentos. */
std::string creatureId(const std::string& seedDecimal, int64_t nacimientoMs);

/** Hora local (0-23) de una marca de tiempo. */
int localHour(int64_t ms, int tzOffsetMin);

/** Índice de día local. Puede ser negativo: es división con piso, no truncada. */
int64_t localDayIndex(int64_t ms, int tzOffsetMin);

/** Estado inicial de una criatura recién nacida. */
CreatureState createCreature(Seed seed, int64_t nowMs, int tzOffsetMin);

/** Avanza la simulación hasta `nowMs`. Devuelve un estado nuevo; no muta el que recibe. */
SimResult simulate(const CreatureState& state, int64_t nowMs);

} // namespace petbits
