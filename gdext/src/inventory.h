#pragma once
/**
 * inventory.h — Port de src/core/inventory.ts
 *
 * La despensa. Es del jugador, no de la criatura: una despensa, no un estómago.
 * Por eso vive en el estado de la partida y no en cada bicho.
 *
 * ---
 *
 * QUÉ NO HACE ESTE MÓDULO, Y POR QUÉ IMPORTA.
 *
 * No descuenta nada solo, y `alimentar()` tampoco lo consulta. En el TypeScript
 * el cobro lo hace la capa de juego, en este orden:
 *
 *     1. `consumir` aparta la unidad y devuelve una despensa NUEVA
 *     2. se llama a `alimentar`
 *     3. si la acción falló, la despensa nueva se descarta
 *     4. si salió bien, recién ahí se reemplaza la despensa
 *
 * Devolver una copia en vez de mutar es lo que hace posible el paso 3. Si
 * `consumir` restara sobre el original, una acción rechazada —la criatura está
 * de expedición, por ejemplo— te habría comido la baya igual.
 *
 * El port respeta esa separación. Meter el descuento adentro de `alimentar`
 * sería más corto y haría que las dos plataformas jugaran juegos distintos.
 */

#include "evolution.h"

#include <string>
#include <string_view>
#include <vector>

namespace petbits {

/**
 * La despensa: cuánto hay de cada alimento.
 *
 * Vector de pares y no un map, por lo mismo que en el JSON: el orden de las
 * claves se conserva, así el archivo guardado no baila entre sesiones.
 */
class Inventario {
public:
    int64_t cuanto(std::string_view id) const;
    bool hay(std::string_view id) const;
    int64_t total() const;

    /** Pone una cantidad exacta. Usada al cargar el guardado. */
    void poner(std::string_view id, int64_t cantidad);

    /**
     * Descuenta una unidad. Devuelve false y no toca nada si no había.
     *
     * Se aplica sobre una COPIA de la despensa, no sobre la original: quien
     * llama se queda con la copia solo si la acción que venía después salió
     * bien. Ver el comentario del encabezado.
     */
    bool consumir(std::string_view id);

    void agregar(std::string_view id, int64_t cantidad = 1);

    const std::vector<std::pair<std::string, int64_t>>& items() const { return items_; }

private:
    std::vector<std::pair<std::string, int64_t>> items_;
};

/** Con qué se arranca. El cristal en cero a propósito: es lo raro. */
Inventario inventarioInicial();

/** Qué tipo de comida es cada id. Lo usa el botín de las expediciones. */
bool tipoDeAlimento(std::string_view id, FoodKind& salida);

} // namespace petbits
