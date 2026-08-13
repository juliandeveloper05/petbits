#pragma once
/**
 * save_manager.h — Lee y escribe el guardado de la web.
 *
 * El mismo archivo sirve para las dos plataformas. Un save escrito en el
 * navegador se abre en el nativo y al revés.
 *
 * ---
 *
 * LO QUE NO SE ENTIENDE, SE CONSERVA.
 *
 * El save trae `codex`, `inventario` y `semillas`, y esos tres módulos todavía
 * no están portados a C++. La tentación es ignorarlos; hacerlo significaría que
 * abrir tu partida en el nativo te borra el codex entero.
 *
 * Así que se leen y se vuelven a escribir tal cual vinieron, sin interpretarlos.
 * El nativo toca solo lo que sabe tocar —las criaturas y cuál está activa— y
 * todo lo demás pasa de largo intacto. Es la única regla que hace seguro
 * compartir un formato entre dos programas que no están igual de completos.
 *
 * Cuando esos módulos se porten, dejan de ser opacos y nada más cambia.
 */

#include "json.h"
#include "simulation.h"

#include <string>
#include <vector>

namespace petbits {

/** La versión que escribe este código. Tiene que coincidir con SAVE_VERSION del TS. */
inline constexpr int64_t SAVE_VERSION = 5;

struct Partida {
    std::vector<CreatureState> criaturas;
    std::string activaId;

    /**
     * El resto del save, sin interpretar: codex, inventario, semillas y
     * cualquier campo que una versión futura agregue.
     *
     * Se guarda el objeto entero y no solo esas tres claves a propósito: si el
     * TS suma un campo nuevo, este código lo preserva sin enterarse.
     */
    Json otros;
};

/** La criatura activa, o nullptr si `activaId` no apunta a ninguna. */
const CreatureState* criaturaActiva(const Partida& p);

/** Reemplaza una criatura por su versión nueva. No hace nada si no está. */
void reemplazarCriatura(Partida& p, const CreatureState& criatura);

/** Arranca una partida con una sola criatura, con codex e inventario vacíos. */
Partida partidaInicial(const CreatureState& criatura);

/**
 * Interpreta el texto de un save.
 *
 * Nunca lanza. Devuelve false con el motivo en `error`: un guardado corrupto
 * tiene que degradar a "empezá de nuevo", no reventar el arranque.
 */
bool cargarPartida(const std::string& texto, Partida& salida, std::string& error);

/** Serializa la partida al formato de la web. */
std::string guardarPartida(const Partida& p, int64_t nowMs);

// Expuestas para poder testearlas por separado.
Json criaturaAJson(const CreatureState& c);
bool criaturaDesdeJson(const Json& j, CreatureState& salida, std::string& error);

} // namespace petbits
