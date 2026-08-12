#pragma once
/**
 * pixel_buffer.h — Port de src/render/pixelBuffer.ts
 *
 * Máscara booleana y lienzo RGBA, sin nada del motor. Igual que del lado web,
 * donde el criterio era no tocar el DOM: acá el generador de sprites no incluye
 * un solo header de Godot, y por eso se puede verificar contra el TypeScript
 * con un compilador y nada más.
 */

#include <cstdint>
#include <vector>

namespace petbits {

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/** Define qué píxeles son "cuerpo" antes de pintarlos. */
class Mask {
public:
    Mask(int ancho, int alto) : ancho_(ancho), alto_(alto), celdas_(static_cast<size_t>(ancho * alto), 0) {}

    bool has(int x, int y) const {
        if (x < 0 || y < 0 || x >= ancho_ || y >= alto_) return false;
        return celdas_[static_cast<size_t>(y * ancho_ + x)] == 1;
    }

    void set(int x, int y) {
        if (x < 0 || y < 0 || x >= ancho_ || y >= alto_) return;
        celdas_[static_cast<size_t>(y * ancho_ + x)] = 1;
    }

    /** Marca el píxel y su espejo respecto del eje vertical. */
    void setMirrored(int x, int y) {
        set(x, y);
        set(ancho_ - 1 - x, y);
    }

    /** ¿Tiene al menos un vecino de 4 direcciones dentro de la máscara? */
    bool touchesBody(int x, int y) const {
        return has(x - 1, y) || has(x + 1, y) || has(x, y - 1) || has(x, y + 1);
    }

private:
    int ancho_;
    int alto_;
    std::vector<uint8_t> celdas_;
};

/** Lienzo RGBA. 4 bytes por píxel, igual que ImageData. */
class PixelBuffer {
public:
    PixelBuffer(int ancho, int alto)
        : ancho_(ancho), alto_(alto), datos_(static_cast<size_t>(ancho * alto * 4), 0) {}

    void set(int x, int y, const Rgb& color, uint8_t alpha = 255) {
        if (x < 0 || y < 0 || x >= ancho_ || y >= alto_) return;
        const size_t i = static_cast<size_t>((y * ancho_ + x) * 4);
        datos_[i] = color.r;
        datos_[i + 1] = color.g;
        datos_[i + 2] = color.b;
        datos_[i + 3] = alpha;
    }

    const std::vector<uint8_t>& datos() const { return datos_; }
    int ancho() const { return ancho_; }
    int alto() const { return alto_; }

private:
    int ancho_;
    int alto_;
    std::vector<uint8_t> datos_;
};

/**
 * Hash FNV-1a del contenido de un buffer.
 *
 * Es la forma compacta de comparar un sprite entero contra el TypeScript: 4096
 * bytes se resumen en ocho dígitos, y cualquier píxel distinto lo cambia.
 */
inline uint32_t hashPixels(const std::vector<uint8_t>& datos) {
    uint32_t hash = 0x811C9DC5u;
    for (const uint8_t b : datos) {
        hash ^= b;
        hash *= 0x01000193u;
    }
    return hash;
}

} // namespace petbits
