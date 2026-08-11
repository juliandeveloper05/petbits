/**
 * evolution.cpp — Port de src/core/evolution.ts
 */

#include "evolution.h"
#include <cassert>

namespace petbits {

// ---------------------------------------------------------------------------
// Sesgo por afinidad (mismo array que evolution.ts)
// Índice = gen affinity (0-7): Brasa, Marea, Raíz, Chispa, Escarcha, Polvo, Eco, Vacío
// ---------------------------------------------------------------------------

static constexpr std::array<int, 8> AFFINITY_BIAS = {2, -1, 3, 0, 1, 2, -3, -2};

static int affinityBias(const Genes& g) {
    return AFFINITY_BIAS[g.affinity % AFFINITY_BIAS.size()];
}

// ---------------------------------------------------------------------------
// Ejes
// ---------------------------------------------------------------------------

double animoPromedio(const Crianza& c) {
    return c.ticksMedidos == 0 ? 50.0 : c.sumaAnimo / static_cast<double>(c.ticksMedidos);
}

double saludPromedio(const Crianza& c) {
    return c.ticksMedidos == 0 ? 100.0 : c.sumaSalud / static_cast<double>(c.ticksMedidos);
}

double ejeSomatico(const Crianza& c, const Genes& g) {
    const double cuerpo = static_cast<double>(c.dieta_proteina + c.dieta_mineral);
    const double etereo = static_cast<double>(c.dieta_dulce   + c.dieta_raro);
    return cuerpo - etereo + static_cast<double>(affinityBias(g));
}

double ejeActividad(const Crianza& c) {
    return static_cast<double>(c.juego - c.calma) + (animoPromedio(c) - 50.0) / 12.0;
}

// ---------------------------------------------------------------------------
// Resolución de forma
// ---------------------------------------------------------------------------

Form resolverJuvenil(const Crianza& c, const Genes& g) {
    return ejeSomatico(c, g) >= 0.0 ? Form::Petreo : Form::Vaporoso;
}

Form resolverAdulto(const Crianza& c, const Genes& g, Form juvenil) {
    // Si no hay juvenil válido, recalcular
    const Form rama = (juvenil == Form::Petreo || juvenil == Form::Vaporoso)
                    ? juvenil
                    : resolverJuvenil(c, g);
    const bool activo = ejeActividad(c) >= 0.0;

    if (rama == Form::Petreo)  return activo ? Form::Coloso  : Form::Guardian;
    return                             activo ? Form::Errante : Form::Oraculo;
}

// ---------------------------------------------------------------------------
// Presentación
// ---------------------------------------------------------------------------

std::string_view formName(Form f) {
    switch (f) {
        case Form::Indefinida: return "Sin definir";
        case Form::Petreo:     return "Pétreo";
        case Form::Vaporoso:   return "Vaporoso";
        case Form::Coloso:     return "Coloso";
        case Form::Guardian:   return "Guardián";
        case Form::Errante:    return "Errante";
        case Form::Oraculo:    return "Oráculo";
    }
    return "?";
}

std::string_view formDescription(Form f) {
    switch (f) {
        case Form::Indefinida: return "Todavía no muestra por dónde va a crecer.";
        case Form::Petreo:     return "Se le puso el cuerpo denso. Comió para durar.";
        case Form::Vaporoso:   return "Se le afinó el cuerpo. Comió para otra cosa.";
        case Form::Coloso:     return "Cuerpo de sobra y ganas de usarlo.";
        case Form::Guardian:   return "Se plantó y no se mueve de ahí. Le gusta el lugar.";
        case Form::Errante:    return "Liviana y sin quedarse quieta un segundo.";
        case Form::Oraculo:    return "Callada, mirando cosas que nadie más mira.";
    }
    return "";
}

std::string_view stageName(Stage s) {
    switch (s) {
        case Stage::Bebe:    return "Bebé";
        case Stage::Juvenil: return "Juvenil";
        case Stage::Adulto:  return "Adulto";
    }
    return "?";
}

} // namespace petbits
