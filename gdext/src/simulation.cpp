/**
 * simulation.cpp — ver simulation.h.
 *
 * Traducción del bucle de simulation.ts. Se mantuvo el orden de las
 * operaciones tal cual está en el original, incluso donde reordenarlas sería
 * más prolijo: los stats son de punto flotante, y `a - b + c` no da siempre lo
 * mismo que `a + c - b`. Un reordenamiento inocente corre el último bit y, tras
 * diez mil ticks, la criatura de la web y la del nativo dejan de ser la misma.
 */

#include "simulation.h"
#include "rng.h"

#include <algorithm>
#include <cmath>

namespace petbits {

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

static double clamp01to100(double value) {
    return value < 0.0 ? 0.0 : (value > 100.0 ? 100.0 : value);
}

/** Módulo que siempre devuelve un resultado no negativo, igual que en el TS. */
static int64_t modPositivo(int64_t value, int64_t divisor) {
    return ((value % divisor) + divisor) % divisor;
}

/**
 * División con piso.
 *
 * `Math.floor(a / b)` en JS redondea hacia abajo también con negativos; la
 * división de enteros de C++ trunca hacia cero. Para -1 / 86400000 eso es la
 * diferencia entre -1 y 0, o sea entre el día de ayer y el de hoy.
 *
 * Con las fechas de hoy nunca da negativo, pero el desfasaje horario puede
 * restar y no hay razón para dejar la trampa armada.
 */
static int64_t divisionConPiso(int64_t numerador, int64_t divisor) {
    const int64_t cociente = numerador / divisor;
    const int64_t resto = numerador % divisor;
    return (resto != 0 && ((resto < 0) != (divisor < 0))) ? cociente - 1 : cociente;
}

int localHour(int64_t ms, int tzOffsetMin) {
    const int64_t local = ms + static_cast<int64_t>(tzOffsetMin) * 60'000;
    return static_cast<int>(modPositivo(local, DAY_MS) / HOUR_MS);
}

int64_t localDayIndex(int64_t ms, int tzOffsetMin) {
    return divisionConPiso(ms + static_cast<int64_t>(tzOffsetMin) * 60'000, DAY_MS);
}

static bool esDeNoche(int hour) {
    return hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR;
}

std::string seedADecimal(Seed seed) {
    return std::to_string(seed);
}

std::string creatureId(const std::string& seedDecimal, int64_t nacimientoMs) {
    return seedDecimal + "-" + std::to_string(nacimientoMs);
}

std::string_view simEventKindId(SimEventKind kind) {
    switch (kind) {
        case SimEventKind::Hambre:    return "hambre";
        case SimEventKind::Animo:     return "animo";
        case SimEventKind::Salud:     return "salud";
        case SimEventKind::Durmio:    return "durmio";
        case SimEventKind::Desperto:  return "desperto";
        case SimEventKind::Letargo:   return "letargo";
        case SimEventKind::Hallazgo:  return "hallazgo";
        case SimEventKind::Evolucion: return "evolucion";
        case SimEventKind::Reloj:     return "reloj";
    }
    return "?";
}

static double energyDrainFor(uint8_t metabolism) {
    return ENERGY_DRAIN_MIN + (static_cast<double>(metabolism) / 7.0) * (ENERGY_DRAIN_MAX - ENERGY_DRAIN_MIN);
}

/**
 * Texto de color de los hallazgos.
 *
 * Va en rioplatense, no en español neutro, y la criatura se trata en femenino
 * en todo el juego. Si se agrega texto nuevo, mantener la concordancia — y
 * agregarlo AL FINAL: el índice se sortea sobre la lista, así que meter una
 * frase en el medio le cambia el hallazgo a todas las partidas existentes.
 */
static const char* const FINDINGS[] = {
    "Encontró algo que brillaba y lo escondió al toque.",
    "Se pasó un rato largo correteando una sombra.",
    "Se quedó dura mirando la pared. Andá a saber qué vio.",
    "Chusmeó un rincón nuevo de arriba a abajo.",
    "Estornudó tres veces seguidas y quedó ofendida.",
    "Acomodó sus cosas. A su manera, digamos.",
    "Le agarró la fiaca y no hizo nada por un buen rato.",
    "Se puso a hablar sola. O eso pareció.",
    "Se peleó con su propia sombra y salió empatada.",
    "Se quedó escuchando algo que solo ella escuchaba.",
    "Movió todo de lugar y después lo dejó igual que antes.",
    "Se metió en un rincón y no quiso salir por un rato.",
};
static constexpr size_t FINDINGS_COUNT = sizeof(FINDINGS) / sizeof(FINDINGS[0]);

// ---------------------------------------------------------------------------
// createCreature
// ---------------------------------------------------------------------------

CreatureState createCreature(Seed seed, int64_t nowMs, int tzOffsetMin) {
    CreatureState s{};
    const std::string decimal = seedADecimal(seed);

    s.id = creatureId(decimal, nowMs);
    s.seed = seed;
    s.nacimientoMs = nowMs;
    s.lastTickMs = nowMs;
    s.ticksVividos = 0;
    s.tzOffsetMin = tzOffsetMin;
    s.stats = {70.0, 70.0, 100.0, 0.0};
    s.vinculoHoy = 0.0;
    s.diaIndice = localDayIndex(nowMs, tzOffsetMin);
    s.ticksSinCuidado = 0;
    s.letargico = false;
    s.durmiendo = esDeNoche(localHour(nowMs, tzOffsetMin));
    s.ticksActivos = 0;
    s.etapa = Stage::Bebe;
    s.forma = Form::Indefinida;
    s.crianza = Crianza{};
    s.ultimaCruzaMs = std::nullopt;
    s.expedicion = std::nullopt;

    return s;
}

// ---------------------------------------------------------------------------
// simulate
// ---------------------------------------------------------------------------

SimResult simulate(const CreatureState& state, int64_t nowMs) {
    SimResult resultado;
    resultado.state = state;
    resultado.omitted = 0;
    resultado.ticks = 0;

    std::vector<SimEvent> eventos;

    const auto emitir = [&](SimEventKind kind, int64_t atMs, std::string text) {
        resultado.summary[std::string(simEventKindId(kind))] += 1;
        eventos.push_back(SimEvent{kind, atMs, std::move(text)});
    };

    // Reloj hacia atrás. Es lo único detectable del lado del cliente: un salto
    // hacia ADELANTE es indistinguible de haber estado ausente de verdad, y no
    // se puede impedir sin un servidor. El letargo limita el beneficio de
    // intentarlo, porque estando ausente no se acumula nada bueno.
    if (nowMs < state.lastTickMs) {
        emitir(SimEventKind::Reloj, state.lastTickMs,
               "El reloj del sistema fue para atrás. No perdiste nada.");
        resultado.events = std::move(eventos);
        return resultado;
    }

    const int64_t ticks = (nowMs - state.lastTickMs) / TICK_MS;
    if (ticks == 0) {
        resultado.events = std::move(eventos);
        return resultado;
    }
    resultado.ticks = ticks;

    const Genes genes = decodeGenome(state.seed);
    const double energyDrain = energyDrainFor(genes.metabolism);
    const uint32_t eventBase = deriveSeed(state.seed, "eventos");

    CreatureState& next = resultado.state;

    for (int64_t i = 0; i < ticks; ++i) {
        // Cada tick lee la hora de SU frontera, no de `nowMs`. Es lo que
        // permite que simular por pedazos dé el mismo resultado.
        const int64_t tickStart = next.lastTickMs;
        const int hour = localHour(tickStart, next.tzOffsetMin);
        const bool noche = esDeNoche(hour);

        // Reinicio diario del tope de vínculo.
        const int64_t dia = localDayIndex(tickStart, next.tzOffsetMin);
        if (dia != next.diaIndice) {
            next.diaIndice = dia;
            next.vinculoHoy = 0.0;
        }

        // Ciclo de sueño.
        if (noche && !next.durmiendo) {
            next.durmiendo = true;
            emitir(SimEventKind::Durmio, tickStart, "Se hizo un rollito y se durmió.");
        } else if (!noche && next.durmiendo) {
            next.durmiendo = false;
            emitir(SimEventKind::Desperto, tickStart, "Se despertó, estiró las patas y bostezó.");
        }

        next.ticksSinCuidado++;

        if (!next.letargico && next.ticksSinCuidado >= LETHARGY_TICKS) {
            next.letargico = true;
            emitir(SimEventKind::Letargo, tickStart,
                   "Se apagó un poco y entró en letargo. Dejó de empeorar, pero el vínculo aflojó.");
        }

        // En letargo TODO se congela. Es deliberado: la criatura nunca muere, y
        // así los ticks de una ausencia larga son operaciones nulas — se pueden
        // recorrer sin cambiar nada y sin costo real.
        if (!next.letargico) {
            const Stats prev = next.stats;

            double drain = energyDrain;
            if (noche && next.durmiendo) drain *= 0.4;
            next.stats.energia = clamp01to100(next.stats.energia - drain);

            double moodDrain = MOOD_DRAIN;
            if (next.stats.energia < 20.0) moodDrain *= 2.0;
            if (noche && next.durmiendo) moodDrain = 0.0;
            next.stats.animo = clamp01to100(next.stats.animo - moodDrain);

            if (next.stats.energia <= 0.0 || next.stats.animo <= 0.0) {
                next.stats.salud = clamp01to100(next.stats.salud - HEALTH_DECAY);
            } else if (next.stats.energia > 30.0 && next.stats.animo > 30.0) {
                next.stats.salud = clamp01to100(next.stats.salud + HEALTH_RECOVER);
            }

            // Eventos por cruce de umbral: se avisa al cruzar, no en cada tick
            // por debajo, para no inundar el registro.
            if (prev.energia >= LOW_ENERGY && next.stats.energia < LOW_ENERGY) {
                emitir(SimEventKind::Hambre, tickStart, "Le empezó a sonar la panza.");
            }
            if (prev.animo >= LOW_MOOD && next.stats.animo < LOW_MOOD) {
                emitir(SimEventKind::Animo, tickStart, "Se está aburriendo mal.");
            }
            if (prev.salud >= LOW_HEALTH && next.stats.salud < LOW_HEALTH) {
                emitir(SimEventKind::Salud, tickStart, "No se la ve nada bien.");
            }

            // Crianza: se acumula solo con la criatura activa. Sumar durante el
            // letargo promediaría valores congelados y ensuciaría la rama
            // evolutiva con tiempo en el que no pasó nada.
            next.ticksActivos++;
            next.crianza.sumaAnimo += next.stats.animo;
            next.crianza.sumaSalud += next.stats.salud;
            next.crianza.ticksMedidos++;

            // Crecimiento. Requiere salud mínima: una criatura hecha pelota no
            // evoluciona, primero hay que levantarla.
            if (next.stats.salud >= MIN_SALUD_EVOLUCION) {
                if (next.etapa == Stage::Bebe &&
                    next.ticksActivos >= static_cast<int64_t>(JUVENIL_TICKS)) {
                    next.etapa = Stage::Juvenil;
                    next.forma = resolverJuvenil(next.crianza, genes);
                    emitir(SimEventKind::Evolucion, tickStart,
                           std::string("Pegó el estirón. Ahora es ") +
                               std::string(formName(next.forma)) + ".");
                } else if (next.etapa == Stage::Juvenil &&
                           next.ticksActivos >= static_cast<int64_t>(ADULTO_TICKS)) {
                    next.etapa = Stage::Adulto;
                    next.forma = resolverAdulto(next.crianza, genes, next.forma);
                    emitir(SimEventKind::Evolucion, tickStart,
                           std::string("Terminó de crecer. Quedó ") +
                               std::string(formName(next.forma)) + ".");
                }
            }

            // Hallazgos: azar sembrado por índice de tick, nunca por un flujo
            // arrastrado. Con un PRNG compartido, simular en dos pedazos daría
            // otra secuencia que simular de corrido.
            //
            // `Math.imul(ticksVividos, 0x9e3779b1)` es multiplicación de 32
            // bits: se trunca el contador a uint32 y se deja desbordar.
            const uint32_t mezcla =
                static_cast<uint32_t>(next.ticksVividos) * 0x9E3779B1u;
            Rng rng(eventBase ^ mezcla);

            // ~1 hallazgo cada 4 horas despierta.
            if (!next.durmiendo && rng.next() < 0.004) {
                // rng.pick del TS: Math.floor(next() * length).
                const int64_t indice = rng.intMenorQue(static_cast<int64_t>(FINDINGS_COUNT));
                emitir(SimEventKind::Hallazgo, tickStart, FINDINGS[indice]);
            }
        }

        next.lastTickMs += TICK_MS;
        next.ticksVividos++;
    }

    // Recorte: se conservan los más recientes, y cuántos se tiraron se informa.
    if (eventos.size() > MAX_EVENTS) {
        resultado.omitted = static_cast<int64_t>(eventos.size() - MAX_EVENTS);
        resultado.events.assign(eventos.end() - static_cast<long>(MAX_EVENTS), eventos.end());
    } else {
        resultado.events = std::move(eventos);
    }

    return resultado;
}

} // namespace petbits
