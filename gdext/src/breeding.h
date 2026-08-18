#pragma once
/**
 * breeding — dos genomas dan uno nuevo.
 *
 * Port de `src/core/breeding.ts`. Paridad verificada contra vectores generados
 * ejecutando el TypeScript.
 *
 * ---
 *
 * LA DECISIÓN CENTRAL: se cruza por GEN, no por bit.
 *
 * Lo intuitivo sería mezclar bit a bit, y está mal. El genoma es un número
 * empaquetado: `hue` son los bits 24 a 31. Si de esos ocho se toman cuatro de
 * cada padre, el resultado no es "un color intermedio" — es un color al azar que
 * no se parece a ninguno de los dos. Lo mismo con cada gen: mezclar bits adentro
 * de un campo destruye su significado.
 *
 * Cruzando por gen, cada campo viene entero de uno de los padres. El hijo tiene
 * los ojos de uno y el color del otro, y eso se VE. Es la diferencia entre que
 * la herencia sea un mecanismo o sea ruido.
 *
 * La mutación sí actúa bit a bit, y ahí es lo correcto: un bit dado vuelta en
 * `hue` corre el tono un poco, uno en `lineage` cambia el linaje entero.
 *
 * ---
 *
 * TRES COSAS QUE PARECEN ERRORES Y NO LO SON.
 *
 * Están acá arriba porque son exactamente las que alguien va a querer
 * "corregir", y corregirlas rompe la paridad con la web sin que falle nada más.
 *
 * 1. `herencia` se compara contra `seedA`, no contra el menor de los dos. El TS
 *    escribe `desde === seedA ? "A" : "B"`, y `desde` vale el menor o el mayor.
 *    Si `seedA > seedB`, la etiqueta "A" sale cuando `desde` es el MAYOR. Y si
 *    los dos padres tienen el mismo genoma, todos los campos salen "A".
 *
 * 2. El bucle de mutación recorre los 64 bits SIEMPRE, sin salir antes. No es
 *    por prolijidad: cada vuelta consume un número del PRNG, así que cortar
 *    temprano correría la secuencia y cambiaría todos los hijos siguientes.
 *
 * 3. La etiqueta del PRNG lleva el seed en DECIMAL. En JavaScript `${bigint}`
 *    rinde base diez; escribirla en hexadecimal acá daría otro hijo y ningún
 *    test que no sea de paridad se enteraría.
 */

#include "genome.h"
#include "simulation.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace petbits {

/** Ocho horas de descanso entre cruzas. */
inline constexpr int64_t CRUZA_COOLDOWN_MS = 8LL * 60 * 60 * 1000;

/**
 * Vínculo mínimo para poder cruzar.
 *
 * Con el tope diario de 12, llegar a 20 lleva por lo menos dos días de atención.
 * Es deliberado: ata la cruza al bucle de cuidado en vez de convertirla en un
 * atajo para saltárselo.
 */
inline constexpr double CRUZA_MIN_VINCULO = 20.0;
inline constexpr double CRUZA_MIN_SALUD = 50.0;

/** Cuántos bits se espera que muten, en promedio, sobre los 64. */
inline constexpr double MUTACIONES_ESPERADAS = 2.5;

/** De dónde salió un gen del hijo. */
enum class Origen : uint8_t { A = 0, B = 1, Mutado = 2 };

/** "A" | "B" | "mutado" — los mismos textos que usa el TS. */
std::string_view nombreOrigen(Origen o);

/**
 * Un campo del genoma: cómo se llama, dónde empieza y cuánto mide.
 *
 * Es la contraparte de `GENOME_LAYOUT` en `genome.ts`, y **el orden es parte del
 * algoritmo**: se consume un valor del PRNG por campo, así que reordenar la
 * tabla cambia todos los hijos. Vive acá y no en `genome.h` porque la cruza es
 * el único código que necesita el genoma como lista de campos en vez de como
 * struct decodificado.
 */
struct CampoGenoma {
    std::string_view clave;
    int offset;
    int bits;
};

/** Los catorce campos, en el orden de `GENOME_LAYOUT`. */
const std::array<CampoGenoma, 14>& camposGenoma();

struct Cruza {
    Seed seed = 0;
    /** De dónde salió cada gen, indexado igual que `camposGenoma()`. */
    std::array<Origen, 14> herencia{};
    /** Cuántos bits mutaron. */
    int mutaciones = 0;
};

/**
 * Combina dos genomas.
 *
 * Es simétrica: cruzar A con B da lo mismo que cruzar B con A. El par determina
 * el hijo, no el orden en que los elegiste.
 *
 * `nonce` lo elige quien llama y es por donde entra el azar, así que la función
 * se puede testear sin trucos. Va como entero: el TS lo interpola con
 * `${nonce}`, que para un número entero rinde igual que `std::to_string`.
 */
Cruza cruzar(Seed seedA, Seed seedB, int64_t nonce);

struct Elegibilidad {
    bool puede = false;
    /** Vacío si puede. Si no, POR QUÉ no, redactado para mostrar tal cual. */
    std::string motivo;
};

/**
 * ¿Esta criatura está en condiciones de cruzar?
 *
 * Separada de `puedenCruzar` para poder decirle al jugador cuál de las dos es la
 * que no puede, y por qué. El orden de los chequeos importa: solo se informa el
 * primero que falla.
 */
Elegibilidad elegibilidad(const CreatureState& criatura, int64_t nowMs);

/** ¿Se puede cruzar este par? */
Elegibilidad puedenCruzar(const CreatureState& a, const CreatureState& b, int64_t nowMs);

/** Marca a una criatura como recién cruzada, sin tocar la original. */
CreatureState marcarCruzada(const CreatureState& criatura, int64_t nowMs);

/**
 * A quién salió el hijo, en texto.
 *
 * Cuenta genes, no bits: lo que el jugador percibe es "tiene la cara del padre",
 * y eso se corresponde con campos enteros. Los mutados no cuentan para ninguno
 * de los dos lados.
 */
std::string describirHerencia(const Cruza& cruza);

} // namespace petbits
