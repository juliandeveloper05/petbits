/**
 * inventory.cpp — ver inventory.h.
 */

#include "inventory.h"

namespace petbits {

int64_t Inventario::cuanto(std::string_view id) const {
    for (const auto& [k, v] : items_) {
        if (k == id) return v;
    }
    return 0;
}

bool Inventario::hay(std::string_view id) const {
    return cuanto(id) > 0;
}

int64_t Inventario::total() const {
    int64_t suma = 0;
    for (const auto& [k, v] : items_) suma += v;
    return suma;
}

void Inventario::poner(std::string_view id, int64_t cantidad) {
    for (auto& [k, v] : items_) {
        if (k == id) {
            v = cantidad;
            return;
        }
    }
    items_.emplace_back(std::string(id), cantidad);
}

bool Inventario::consumir(std::string_view id) {
    if (!hay(id)) return false;
    poner(id, cuanto(id) - 1);
    return true;
}

void Inventario::agregar(std::string_view id, int64_t cantidad) {
    if (cantidad <= 0) return;
    poner(id, cuanto(id) + cantidad);
}

Inventario inventarioInicial() {
    Inventario i;
    // El mismo orden que el objeto literal del TS, para que el JSON salga igual.
    i.poner("baya", 3);
    i.poner("raiz", 2);
    i.poner("larva", 2);
    i.poner("cristal", 0);
    return i;
}

bool tipoDeAlimento(std::string_view id, FoodKind& salida) {
    if (id == "baya") { salida = FoodKind::Dulce; return true; }
    if (id == "raiz") { salida = FoodKind::Mineral; return true; }
    if (id == "larva") { salida = FoodKind::Proteina; return true; }
    if (id == "cristal") { salida = FoodKind::Raro; return true; }
    return false;
}

} // namespace petbits
