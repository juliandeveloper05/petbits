#pragma once
/**
 * codex — el registro de lo que fuiste encontrando.
 *
 * Port de `src/core/codex.ts`. Linajes vistos, formas alcanzadas y rarezas
 * encontradas, más el total de criaturas registradas.
 *
 * ---
 *
 * POR QUÉ SE PORTÓ, Y POR QUÉ NO ALCANZABA CON GUARDARLO.
 *
 * El nativo leía el codex y lo volvía a escribir tal cual, adentro de
 * `Partida::otros`. Con tests que lo probaban: el campo viajaba intacto, un save
 * de la web pasaba por el nativo y volvía sin perder nada.
 *
 * Y estaba mal, por la misma razón por la que estuvo mal el inventario. Nadie lo
 * ESCRIBÍA. Podías evolucionar una criatura a una forma adulta nueva en el
 * nativo y el codex no se enteraba, porque enterarse era trabajo del código que
 * no existía. La web anota, el nativo no anotaba, y las dos partidas dejaban de
 * ser la misma.
 *
 * Vale la pena tenerlo escrito porque el error es tentador: "se guarda y se
 * devuelve intacto" suena a que está resuelto. *"El formato viaja bien" y "las
 * dos plataformas juegan el mismo juego" son dos afirmaciones distintas.*
 *
 * ---
 *
 * LOS TRES ORDENAMIENTOS CAMBIAN BYTES DEL ARCHIVO.
 *
 * El TS ordena las tres listas antes de guardar, y lo dice: si no, dos partidas
 * equivalentes producen JSON distinto y cualquier comparación miente. Copiar eso
 * exige cuidado porque los tres órdenes son distintos entre sí:
 *
 *   - `linajes` se ordena por NÚMERO;
 *   - `formas` se ordena por el ID en texto —"coloso" < "errante" < "guardian"—
 *     y NO por el valor del enum, que va en otro orden completamente
 *     (Indefinida, Petreo, Vaporoso, Coloso…). Ordenar por enum da otro archivo;
 *   - `rarezas` se ordena por id. Los ocho ids son ASCII, así que el orden de
 *     `std::sort` coincide con el de JavaScript, que compara unidades UTF-16.
 */

#include "evolution.h"
#include "genome.h"
#include "traits.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace petbits {

struct Codex {
    /** Índices de linaje descubiertos, ordenados por número. */
    std::vector<int> linajes;
    /** Formas alcanzadas alguna vez, ordenadas por su id en texto. */
    std::vector<Form> formas;
    /** Ids de rarezas encontradas, ordenados. */
    std::vector<std::string> rarezas;
    /** Cuántas criaturas se registraron en total, incluidas las repetidas. */
    int64_t totalRegistradas = 0;
};

enum class TipoDescubrimiento : uint8_t { Linaje, Forma, Rareza };

std::string_view nombreTipo(TipoDescubrimiento t);

struct Descubrimiento {
    TipoDescubrimiento tipo;
    /** Identificador interno, para la interfaz. */
    std::string id;
    /** Nombre legible, ya redactado. */
    std::string nombre;
};

struct Registro {
    Codex codex;
    /** Lo que fue novedad en esta pasada. Vacío si ya se conocía todo. */
    std::vector<Descubrimiento> nuevos;
};

/**
 * Registra una criatura en el codex.
 *
 * Devuelve además qué fue novedad, y eso no es un lujo: descubrir algo por
 * primera vez tiene que sentirse distinto a volver a verlo, y sin esta lista
 * habría que comparar el codex viejo contra el nuevo desde afuera.
 */
Registro registrar(const Codex& codex, Seed seed, Form forma);

struct Avance {
    int vistos = 0;
    int total = 0;
};

struct ProgresoCodex {
    Avance linajes;
    Avance formas;
    Avance rarezas;
    /** Completitud global, 0-100. */
    int porcentaje = 0;
};

/**
 * Cuánto llevás descubierto.
 *
 * Las dos rarezas legendarias entran en el total aunque sean prácticamente
 * inalcanzables —Pangrama es 1 en 880.000—. Es a propósito: un codex que se
 * completa del todo deja de dar motivo para seguir mirando.
 */
ProgresoCodex progresoCodex(const Codex& codex);

bool conoceLinaje(const Codex& codex, int lineage);
bool conoceForma(const Codex& codex, Form forma);
bool conoceRareza(const Codex& codex, std::string_view id);

} // namespace petbits
