#pragma once
/**
 * rng.h — Port de src/core/rng.ts
 *
 * Generadores deterministas. Regla del proyecto: nada de azar del sistema en la
 * generación de criaturas. El mismo genoma tiene que dar la misma criatura en
 * cualquier máquina y en cualquier momento.
 *
 * ---
 *
 * ESTE ES EL MÓDULO DONDE UN PORT SE ROMPE MÁS FÁCIL, y conviene decir por qué.
 *
 * JavaScript no tiene enteros: tiene doubles y un puñado de operadores que
 * fingen aritmética de 32 bits. `>>>` convierte a uint32, `^` y `|` convierten
 * a int32, `Math.imul` multiplica como int32 descartando lo que sobra. Cada
 * una de esas conversiones es un truncamiento con reglas propias.
 *
 * En C++ eso se reproduce usando uint32_t en todos lados: el desborde de
 * enteros sin signo está definido como aritmética módulo 2^32, que es
 * exactamente lo que hacen `>>> 0` y `Math.imul`. La traducción sale limpia
 * justo porque no se intenta copiar el tipo, sino el patrón de bits.
 *
 * Traducirlo con int32_t sería un error: ahí el desborde es comportamiento
 * indefinido y el compilador puede optimizar asumiendo que no pasa.
 */

#include <cstdint>
#include <string_view>

namespace petbits {

/** Mezclador de 64 bits. Dado un entero cualquiera devuelve otro bien repartido. */
uint64_t splitmix64(uint64_t seed);

/**
 * Deriva una sub-semilla de 32 bits a partir de un genoma y una etiqueta.
 *
 * Cada subsistema pide la suya ("eventos", "manchas", …) para tener su propio
 * flujo. Así agregar un consumo de azar en un subsistema no corre la secuencia
 * de los demás, que es el bug clásico que rompe el determinismo entre
 * versiones.
 *
 * La etiqueta se recorre por unidades UTF-16, igual que `charCodeAt` en el TS.
 * Hoy todas las etiquetas son ASCII y daría lo mismo; el día que una lleve una
 * tilde, no.
 */
uint32_t deriveSeed(uint64_t seed, std::string_view label);

/**
 * mulberry32: PRNG de 32 bits con estado mínimo.
 *
 * Se expone como struct con estado explícito, en vez del objeto con cierres que
 * devuelve el TS, porque acá el estado se ve y se puede copiar — que es
 * justamente lo que la simulación necesita para sembrar un generador nuevo por
 * cada tick.
 */
struct Rng {
    uint32_t estado;

    explicit Rng(uint32_t semilla) : estado(semilla) {}

    /** Flotante en [0, 1). */
    double next();

    /** Entero en [0, maxExclusive). */
    int64_t intMenorQue(int64_t maxExclusive);

    /** Entero en [min, max], los dos incluidos. */
    int64_t rango(int64_t min, int64_t max);

    /** true con probabilidad p. */
    bool boolCon(double p = 0.5);
};

/** Atajo: un Rng derivado de un genoma y una etiqueta. */
inline Rng rngFor(uint64_t seed, std::string_view label) {
    return Rng(deriveSeed(seed, label));
}

} // namespace petbits
