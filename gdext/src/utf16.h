#pragma once
/**
 * Recorrer un texto como lo recorre JavaScript.
 *
 * Un std::string son bytes UTF-8; un String de JS son unidades UTF-16, que es
 * lo que devuelve `charCodeAt`. Para el ASCII da igual, y por eso la diferencia
 * pasa desapercibida hasta que aparece un acento: "Nébula" son siete bytes y
 * seis unidades.
 *
 * Dos funciones del núcleo recorren texto carácter por carácter —`hashString` y
 * `deriveSeed`— y las dos tienen que ver exactamente lo mismo que el TypeScript
 * o producen otro número. El decodificador vive acá para que sea uno solo: si
 * estuviera copiado en los dos lados, arreglar un borde en uno y no en el otro
 * daría dos hashes distintos para el mismo texto dentro del mismo programa.
 */

#include <cstdint>
#include <string_view>

namespace petbits {

/**
 * Llama a `emitir` una vez por cada unidad UTF-16 del texto.
 *
 * Es plantilla y no std::function para que el compilador la meta en línea: se
 * la llama por cada carácter de cada etiqueta, en el camino caliente de la
 * simulación.
 */
template <typename F>
void paraCadaUnidadUtf16(std::string_view texto, F emitir) {
    const size_t largoTexto = texto.size();
    size_t i = 0;

    while (i < largoTexto) {
        const unsigned char b0 = static_cast<unsigned char>(texto[i]);
        uint32_t punto = 0;
        size_t largo = 0;

        if (b0 < 0x80) {
            punto = b0;
            largo = 1;
        } else if ((b0 & 0xE0) == 0xC0) {
            punto = b0 & 0x1Fu;
            largo = 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            punto = b0 & 0x0Fu;
            largo = 3;
        } else if ((b0 & 0xF8) == 0xF0) {
            punto = b0 & 0x07u;
            largo = 4;
        } else {
            // Byte suelto que no puede empezar una secuencia. Un string de JS
            // nunca llega así; se emite tal cual y se sigue, que es preferible
            // a quedarse en el lugar.
            emitir(b0);
            ++i;
            continue;
        }

        bool completa = (i + largo <= largoTexto);
        for (size_t k = 1; completa && k < largo; ++k) {
            const unsigned char bk = static_cast<unsigned char>(texto[i + k]);
            if ((bk & 0xC0) != 0x80) {
                completa = false;
                break;
            }
            punto = (punto << 6) | (bk & 0x3Fu);
        }
        if (!completa) {
            emitir(b0);
            ++i;
            continue;
        }

        i += largo;

        if (punto <= 0xFFFF) {
            emitir(punto);
        } else {
            // Fuera del plano básico: JavaScript lo guarda como dos unidades, y
            // charCodeAt las devuelve por separado.
            const uint32_t resto = punto - 0x10000u;
            emitir(0xD800u + (resto >> 10));
            emitir(0xDC00u + (resto & 0x3FFu));
        }
    }
}

} // namespace petbits
