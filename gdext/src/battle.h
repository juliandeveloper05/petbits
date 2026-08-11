#pragma once
/**
 * battle.h — Sistema de combate por turnos (nuevo en la versión Godot)
 *
 * Basado en los stats de la criatura (energia, animo, salud, vinculo)
 * y su Form (Coloso, Guardián, Errante, Oráculo).
 *
 * La Afinidad elemental (Brasa, Marea, Raíz, etc.) define ventajas/desventajas
 * en una tabla 8×8, igual que los tipos en Pokémon.
 *
 * No tiene equivalente en el TS — es una feature nativa de Godot.
 */

#include "evolution.h"
#include "genome.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace petbits {

// ---------------------------------------------------------------------------
// Stats de combate (derivados de los Stats de la simulación)
// ---------------------------------------------------------------------------

struct BattleStats {
    int hp;         ///< Vida en combate (derivado de salud * 2)
    int maxHp;
    int attack;     ///< Daño base (derivado de vinculo)
    int defense;    ///< Reducción de daño (derivado de salud)
    int speed;      ///< Quién actúa primero (derivado de animo)
    int level;      ///< Nivel visual (derivado de ticksActivos / 1000)

    uint8_t affinity; ///< Copia del gen affinity para tabla de tipos
};

// ---------------------------------------------------------------------------
// Movimientos
// ---------------------------------------------------------------------------

enum class MoveCategory : uint8_t { Fisico, Especial, Estado };

struct Move {
    std::string_view id;
    std::string_view name;
    std::string_view description;
    MoveCategory category;
    int          basePower;   ///< 0 si es estado
    uint8_t      affinity;    ///< Tipo del movimiento (0-7)
    int          ppMax;       ///< Usos máximos
    /// Efecto secundario opcional (expresado como string para GDScript)
    std::string_view effectId;
};

/** Los 4 movimientos de cada Form (en el mismo orden que se muestran en UI). */
std::array<Move, 4> movesForForm(Form form);

// ---------------------------------------------------------------------------
// Tabla de afinidades
// ---------------------------------------------------------------------------

/**
 * Multiplicador de daño entre un movimiento de tipo [ataque] vs un defensor
 * de tipo [defensa]. Valores: 2.0 (super efectivo), 1.0 (normal), 0.5 (poco).
 *
 * La tabla es 8×8 (8 afinidades: Brasa, Marea, Raíz, Chispa, Escarcha, Polvo, Eco, Vacío).
 */
double affinityMultiplier(uint8_t attackAffinity, uint8_t defenseAffinity);

// ---------------------------------------------------------------------------
// Estado de combate
// ---------------------------------------------------------------------------

struct Fighter {
    BattleStats stats;
    BattleStats current; ///< Stats actuales durante el combate
    Form        form;
    std::string nombre;
    Seed        seed;
    std::array<int, 4> ppLeft; ///< PP restantes por movimiento
};

enum class BattleStatus : uint8_t {
    Ongoing,
    PlayerWon,
    PlayerLost,
    Fled,
};

struct BattleAction {
    enum class Type : uint8_t { UseMove, Flee };
    Type type;
    int  moveIndex; ///< Solo si type == UseMove
};

struct BattleEvent {
    enum class Kind : uint8_t {
        MoveUsed,
        DamageDealt,
        SuperEffective,
        NotVeryEffective,
        StatusApplied,
        Fainted,
        Victory,
        Defeat,
        Fled,
    };
    Kind        kind;
    std::string message;
    int         damageAmount;
};

struct BattleState {
    Fighter     player;
    Fighter     opponent;
    uint32_t    turn;
    BattleStatus status;
    std::vector<BattleEvent> log;
};

// ---------------------------------------------------------------------------
// API de combate
// ---------------------------------------------------------------------------

/** Crea el estado inicial del combate. */
BattleState startBattle(const Fighter& player, const Fighter& opponent);

/**
 * Procesa un turno completo.
 * Devuelve el nuevo estado (inmutable, sin mutar el input).
 */
BattleState processTurn(const BattleState& state, BattleAction playerAction);

/** Construye un Fighter a partir del estado de simulación de una criatura. */
Fighter fighterFromCreature(const CreatureState& creature, uint64_t nowMs);

/** Genera un oponente salvaje usando un seed aleatorio. Nivel escalado al jugador. */
Fighter wildFighter(Seed seed, int playerLevel);

// ---------------------------------------------------------------------------
// Recompensas de victoria
// ---------------------------------------------------------------------------

struct BattleRewards {
    int    bondXp;        ///< XP de vínculo para el jugador
    std::string itemId;   ///< Comida ganada (puede ser "")
    int    itemQty;
    std::optional<Seed> wildSeed; ///< Seed de la criatura salvaje (para incubar)
};

/** Calcula las recompensas al ganar. No puede romper el balance económico. */
BattleRewards calcRewards(const BattleState& finalState, bool playerWon);

} // namespace petbits
