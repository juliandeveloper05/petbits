/**
 * expeditions.cpp — ver expeditions.h.
 */

#include "expeditions.h"
#include "actions.h"
#include "rng.h"

#include <algorithm>

namespace petbits {

static constexpr int64_t MINUTO = 60'000;
static constexpr int64_t HORA = 60 * MINUTO;

const std::vector<Destino>& destinos() {
    static const std::vector<Destino> lista = {
        {
            "patio", "El patio", "del patio",
            "Da una vuelta por acá nomás. Siempre trae algo.",
            15 * MINUTO,
            // Sin costo y sin etapa mínima: es la salida que evita quedar trabado.
            0.0, Stage::Bebe,
            1, 2,
            {{"baya", 3}, {"raiz", 2}},
            0.0,
        },
        {
            "bosque", "El bosque", "del bosque",
            "Un rato largo entre los árboles. A veces encuentra semillas.",
            90 * MINUTO,
            15.0, Stage::Juvenil,
            2, 3,
            {{"larva", 3}, {"raiz", 3}, {"baya", 2}},
            0.18,
        },
        {
            "ruinas", "Las ruinas", "de las ruinas",
            "Lejos y pesado. Vuelve con cosas que no se ven en otro lado.",
            4 * HORA,
            30.0, Stage::Adulto,
            3, 5,
            {{"cristal", 2}, {"larva", 3}, {"raiz", 2}, {"baya", 1}},
            0.55,
        },
    };
    return lista;
}

const Destino* destinoPorId(std::string_view id) {
    for (const Destino& d : destinos()) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

bool estaFuera(const CreatureState& criatura) {
    return criatura.expedicion.has_value();
}

PuedeSalir puedeSalir(const CreatureState& criatura, const Destino& destino) {
    if (estaFuera(criatura)) return {false, "Ya está afuera."};
    if (criatura.letargico) return {false, "Está en letargo."};

    if (static_cast<int>(criatura.etapa) < static_cast<int>(destino.etapaMinima)) {
        return {false, "Todavía está muy chica para ir tan lejos."};
    }
    if (criatura.stats.energia < destino.costoEnergia) {
        return {false, "No le da la energía. Primero que coma algo."};
    }
    return {true, ""};
}

CreatureState enviar(const CreatureState& criatura, const Destino& destino, int64_t nowMs) {
    CreatureState next = criatura;
    next.stats.energia = std::max(0.0, next.stats.energia - destino.costoEnergia);
    next.expedicion = Expedicion{std::string(destino.id), nowMs, nowMs + destino.duracionMs};
    return next;
}

bool yaVolvio(const CreatureState& criatura, int64_t nowMs) {
    return criatura.expedicion.has_value() && nowMs >= criatura.expedicion->regresoMs;
}

int64_t faltaParaVolver(const CreatureState& criatura, int64_t nowMs) {
    if (!criatura.expedicion.has_value()) return 0;
    return std::max<int64_t>(0, criatura.expedicion->regresoMs - nowMs);
}

Botin resolverBotin(Seed seed, const Destino& destino, int64_t salidaMs) {
    // La etiqueta lleva el destino y el momento de salida, así que dos salidas
    // al mismo lugar en momentos distintos traen cosas distintas, y la misma
    // salida siempre trae lo mismo.
    const std::string etiqueta =
        "expedicion:" + std::string(destino.id) + ":" + std::to_string(salidaMs);
    Rng rng = rngFor(seed, etiqueta);

    int pesoTotal = 0;
    for (const PesoAlimento& p : destino.pesos) pesoTotal += p.peso;

    const int64_t cantidad = rng.rango(destino.itemsMin, destino.itemsMax);

    Botin botin;
    for (int64_t i = 0; i < cantidad; ++i) {
        double tirada = rng.next() * pesoTotal;
        for (const PesoAlimento& p : destino.pesos) {
            tirada -= p.peso;
            if (tirada <= 0.0) {
                botin.alimentos.agregar(p.id, 1);
                break;
            }
        }
    }

    // Semilla de 64 bits armada con dos tiradas de 32.
    //
    // El orden de las dos llamadas importa y no es intercambiable: en JS el
    // operando izquierdo de `<<` se evalúa primero, así que la tirada alta sale
    // antes que la baja. Escribirlo en una sola expresión en C++ sería un error
    // —el orden de evaluación de los operandos no está especificado— así que van
    // en dos líneas.
    if (rng.next() < destino.chanceSemilla) {
        const uint64_t alta = static_cast<uint64_t>(rng.intMenorQue(0x100000000LL));
        const uint64_t baja = static_cast<uint64_t>(rng.intMenorQue(0x100000000LL));
        botin.semilla = (alta << 32) | baja;
    }

    return botin;
}

Regreso recibir(const CreatureState& criatura, int64_t nowMs) {
    Regreso r{};
    r.volvio = false;
    r.destino = nullptr;

    if (!criatura.expedicion.has_value() || nowMs < criatura.expedicion->regresoMs) {
        return r;
    }

    const std::string destinoId = criatura.expedicion->destinoId;

    r.volvio = true;
    r.criatura = criatura;
    r.criatura.expedicion = std::nullopt;
    r.criatura.ticksSinCuidado = 0;

    const Destino* destino = destinoPorId(destinoId);
    if (destino == nullptr) {
        // Un destino que ya no existe, por ejemplo tras una actualización del
        // juego. Se la trae de vuelta con las manos vacías en vez de dejarla
        // atrapada afuera para siempre.
        r.destino = &destinos()[0];
        return r;
    }

    r.destino = destino;
    r.botin = resolverBotin(criatura.seed, *destino, criatura.expedicion->salidaMs);
    return r;
}

std::string describirBotin(const Botin& botin) {
    std::string partes;
    for (const auto& [id, cantidad] : botin.alimentos.items()) {
        if (cantidad <= 0) continue;
        if (!partes.empty()) partes += ", ";
        partes += std::to_string(cantidad) + " ";
        // El nombre en minúscula sale de la tabla y no se baja de caso acá: la
        // "í" de "Raíz" ocupa dos bytes en UTF-8 y tolower sobre bytes la rompe.
        const Food* f = buscarAlimento(id);
        partes += f != nullptr ? std::string(f->nombreMinuscula) : id;
    }

    if (partes.empty() && !botin.semilla.has_value()) return "Volvió con las manos vacías.";

    const std::string comida = partes.empty() ? "No trajo comida." : "Trajo " + partes + ".";
    return botin.semilla.has_value() ? comida + " Y una semilla desconocida." : comida;
}

} // namespace petbits
