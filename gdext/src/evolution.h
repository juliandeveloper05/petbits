#pragma once
/**
 * evolution.h — Port de src/core/evolution.ts
 *
 * Evolución ramificada: bebé → 2 juveniles → 4 adultos.
 * La rama la decide la crianza, no el azar.
 *
 * INVARIANTE DE PARIDAD: resolverAdulto() debe dar el mismo Form
 * que su equivalente TypeScript para los mismos parámetros.
 */

#include "genome.h"
#include <array>
#include <cstdint>
#include <string_view>

namespace petbits {

// ---------------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------------

enum class Stage : uint8_t { Bebe = 0, Juvenil = 1, Adulto = 2 };

enum class Form : uint8_t {
    Indefinida = 0,
    Petreo     = 1,
    Vaporoso   = 2,
    Coloso     = 3,
    Guardian   = 4,
    Errante    = 5,
    Oraculo    = 6,
};

enum class FoodKind : uint8_t { Proteina, Dulce, Mineral, Raro };

/** Acumulado de crianza — cómo fue tratada la criatura. */
struct Crianza {
    // Dieta (conteos por tipo)
    uint32_t dieta_proteina = 0;
    uint32_t dieta_dulce    = 0;
    uint32_t dieta_mineral  = 0;
    uint32_t dieta_raro     = 0;

    uint32_t juego  = 0;  ///< Veces que se jugó con ella
    uint32_t calma  = 0;  ///< Veces que se la acarició

    double sumaAnimo  = 0.0;  ///< Suma de ánimo por tick activo
    double sumaSalud  = 0.0;  ///< Suma de salud por tick activo
    uint64_t ticksMedidos = 0; ///< Ticks contados (letargo no cuenta)
};

// ---------------------------------------------------------------------------
// Constantes — deben coincidir con evolution.ts
// ---------------------------------------------------------------------------

/** Un tick = 1 minuto. */
inline constexpr uint64_t JUVENIL_TICKS = 24 * 60;
inline constexpr uint64_t ADULTO_TICKS  = 4 * 24 * 60;

/** Con la salud por el piso no evoluciona. */
inline constexpr double MIN_SALUD_EVOLUCION = 40.0;

// ---------------------------------------------------------------------------
// Ejes de crianza
// ---------------------------------------------------------------------------

double animoPromedio(const Crianza& c);
double saludPromedio(const Crianza& c);
double ejeSomatico(const Crianza& c, const Genes& g);
double ejeActividad(const Crianza& c);

// ---------------------------------------------------------------------------
// Resolución de forma
// ---------------------------------------------------------------------------

Form resolverJuvenil(const Crianza& c, const Genes& g);
Form resolverAdulto(const Crianza& c, const Genes& g, Form juvenil);

// ---------------------------------------------------------------------------
// Presentación
// ---------------------------------------------------------------------------

std::string_view formName(Form f);

/**
 * El identificador con el que la forma viaja en el guardado: "petreo", "coloso".
 *
 * Es distinto de `formName`, que devuelve el nombre con acentos para mostrar
 * ("Pétreo"). Los dos existen a propósito: si el archivo guardara el nombre
 * legible, cambiar una tilde rompería todas las partidas.
 *
 * Vive acá y no en el guardado porque el codex también lo necesita, y dos copias
 * del mismo mapeo es una que se puede desincronizar.
 */
std::string_view formId(Form f);

/** El inverso. Devuelve false si el id no corresponde a ninguna forma. */
bool formFromId(std::string_view id, Form& salida);

/**
 * Las formas que cuentan para el codex.
 *
 * `Indefinida` queda afuera: no es un descubrimiento, es la ausencia de uno.
 */
const std::array<Form, 6>& formasColeccionables();
std::string_view formDescription(Form f);
std::string_view stageName(Stage s);

/** Las formas adultas que se pueden alcanzar. */
inline constexpr std::array<Form, 4> ADULT_FORMS = {
    Form::Coloso, Form::Guardian, Form::Errante, Form::Oraculo
};

/** Formas que cuentan para el codex (juveniles + adultas). */
inline constexpr std::array<Form, 6> COLLECTIBLE_FORMS = {
    Form::Petreo, Form::Vaporoso,
    Form::Coloso, Form::Guardian, Form::Errante, Form::Oraculo
};

} // namespace petbits
