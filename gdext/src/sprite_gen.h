#pragma once
/**
 * sprite_gen.h — Port de src/render/spriteGen.ts
 *
 * Genoma → pixel art de 32×32. Dos reglas de composición sostienen todo:
 *
 * 1. **Silueta simétrica, luz asimétrica.** El cuerpo se construye en media
 *    grilla y se espeja, así que siempre lee como criatura y nunca como mancha.
 *    Pero el sombreado se pinta después, sobre el buffer YA espejado y con una
 *    dirección de luz fija (arriba-izquierda). Esa asimetría es lo que hace que
 *    parezca dibujado a mano en vez de un test de Rorschach.
 *
 * 2. **Los ojos mandan.** Es el rasgo que decide si algo lee como ser vivo. Se
 *    dibujan al final, siempre por encima del patrón, y con el brillo del mismo
 *    lado en los dos.
 *
 * ---
 *
 * POR QUÉ SE PORTA Y NO SE GENERAN LOS SPRITES APARTE.
 *
 * Había tres caminos: dejar `tools/sprite_gen.py`, dibujar los sprites a mano,
 * o portar el algoritmo. Los dos primeros rompen la promesa del proyecto —que
 * el mismo seed da la misma criatura en las dos plataformas— porque no hay
 * forma de verificar que un dibujo hecho aparte coincida con lo que calcula la
 * web.
 *
 * Portándolo, el sprite se somete a la misma disciplina que el resto: los
 * vectores de paridad salen de ejecutar el TypeScript y comparan el buffer
 * entero, píxel por píxel.
 */

#include "evolution.h"
#include "genome.h"
#include "pixel_buffer.h"

#include <vector>

namespace petbits {

inline constexpr int SPRITE_SIZE = 32;

/** Gestos de la criatura. Solo cambian la cara, nunca la silueta. */
enum class Expression : uint8_t { Normal, Parpadeo };

struct Sprite {
    int width;
    int height;
    /** RGBA, 4 bytes por píxel — mismo layout que ImageData. */
    std::vector<uint8_t> data;
};

Sprite generateSprite(Seed seed, Stage stage = Stage::Adulto, Form form = Form::Indefinida,
                      Expression expression = Expression::Normal);

} // namespace petbits
