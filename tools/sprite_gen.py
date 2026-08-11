#!/usr/bin/env python3
"""
tools/sprite_gen.py
===================
Port de src/render/spriteGen.ts para uso en pipeline de build.

Genera sprite sheets PNG a partir de un seed de genoma de 64 bits.
Usa la misma lógica de paleta y forma que spriteGen.ts para garantizar
que los sprites Godot se vean idénticos a los de la web.

Uso:
    python tools/sprite_gen.py --seed A3F0-91C4-77BE-2D08 --out godot/assets/sprites/
    python tools/sprite_gen.py --seed-file seeds.txt --out godot/assets/sprites/

Requiere:
    pip install pillow numpy
"""

import argparse
import hashlib
import struct
import sys
from pathlib import Path
from typing import NamedTuple

# Pillow para generar PNG
try:
    from PIL import Image
except ImportError:
    print("ERROR: instalar Pillow → pip install pillow", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# Tipos
# ---------------------------------------------------------------------------

class Genes(NamedTuple):
    lineage:     int  # 0-15
    body_shape:  int  # 0-15
    eyes:        int  # 0-15
    mouth:       int  # 0-15
    appendages:  int  # 0-15  (bitfield: 1=orejas 2=cuernos 4=alas 8=cola)
    pattern:     int  # 0-15
    hue:         int  # 0-255
    palette_mode: int # 0-7
    temperament: int  # 0-7
    metabolism:  int  # 0-7
    affinity:    int  # 0-7
    proportion:  int  # 0-15
    mutation:    int  # 0-255


# ---------------------------------------------------------------------------
# decode_genome — mirror de decodeGenome() en genome.ts
# ---------------------------------------------------------------------------

def _bits(seed: int, offset: int, length: int) -> int:
    mask = (1 << length) - 1
    return (seed >> offset) & mask

def decode_genome(seed: int) -> Genes:
    seed &= (1 << 64) - 1
    return Genes(
        lineage      = _bits(seed,  0, 4),
        body_shape   = _bits(seed,  4, 4),
        eyes         = _bits(seed,  8, 4),
        mouth        = _bits(seed, 12, 4),
        appendages   = _bits(seed, 16, 4),
        pattern      = _bits(seed, 20, 4),
        hue          = _bits(seed, 24, 8),
        palette_mode = _bits(seed, 32, 3),
        temperament  = _bits(seed, 35, 3),
        metabolism   = _bits(seed, 38, 3),
        affinity     = _bits(seed, 41, 3),
        proportion   = _bits(seed, 44, 4),
        mutation     = _bits(seed, 56, 8),
    )


# ---------------------------------------------------------------------------
# Paleta de colores (port de palette.ts)
# ---------------------------------------------------------------------------

def hsl_to_rgb(h: float, s: float, l: float) -> tuple[int, int, int]:
    """Convierte HSL (0-1, 0-1, 0-1) a RGB (0-255)."""
    if s == 0:
        v = int(l * 255)
        return (v, v, v)
    def hue2rgb(p: float, q: float, t: float) -> float:
        t %= 1.0
        if t < 1/6: return p + (q - p) * 6 * t
        if t < 1/2: return q
        if t < 2/3: return p + (q - p) * (2/3 - t) * 6
        return p
    q = l * (1 + s) if l < 0.5 else l + s - l * s
    p = 2 * l - q
    r = hue2rgb(p, q, h + 1/3)
    g = hue2rgb(p, q, h)
    b = hue2rgb(p, q, h - 1/3)
    return (int(r * 255), int(g * 255), int(b * 255))

def genome_palette(genes: Genes) -> list[tuple[int, int, int]]:
    """Genera 4 colores de paleta a partir del genoma."""
    hue = genes.hue / 255.0  # 0-1
    mode = genes.palette_mode

    # Modo 0-7 ajusta saturación y luminosidad (mirror de palette.ts)
    saturation_values = [0.9, 0.7, 0.6, 0.8, 0.5, 0.9, 0.4, 0.75]
    lightness_offsets = [0.0, 0.1, -0.1, 0.05, 0.2, -0.05, 0.15, -0.15]

    s = saturation_values[mode % 8]
    l_base = 0.5 + lightness_offsets[mode % 8]

    return [
        hsl_to_rgb(hue, s, max(0.05, l_base - 0.3)),  # sombra
        hsl_to_rgb(hue, s, max(0.1,  l_base - 0.1)),  # base oscura
        hsl_to_rgb(hue, s, min(0.95, l_base + 0.1)),  # base clara
        hsl_to_rgb(hue, s, min(0.98, l_base + 0.3)),  # brillo
    ]


# ---------------------------------------------------------------------------
# Generador de sprite (simplificado — se expande en Fase 2)
# ---------------------------------------------------------------------------

SPRITE_SIZE = 32  # 32x32 para adulto; bebé usa 16x16 escalado

def generate_sprite_frame(genes: Genes, state: str = "idle") -> Image.Image:
    """
    Genera un frame de sprite de 32×32 píxeles a partir del genoma.
    Versión inicial (placeholder) — se reemplaza con la lógica completa en Fase 2.
    """
    palette = genome_palette(genes)
    img = Image.new("RGBA", (SPRITE_SIZE, SPRITE_SIZE), (0, 0, 0, 0))
    pixels = img.load()

    # Cuerpo base: elipse proporcional
    cx, cy = SPRITE_SIZE // 2, SPRITE_SIZE // 2
    w = 8 + (genes.body_shape % 8)
    h = 8 + (genes.proportion % 8)

    for y in range(SPRITE_SIZE):
        for x in range(SPRITE_SIZE):
            dx = (x - cx) / w
            dy = (y - cy) / h
            dist = dx * dx + dy * dy
            if dist <= 1.0:
                # Gradiente radial: brillo en el centro
                shade = int(dist * 3)
                shade = min(shade, 3)
                r, g, b = palette[3 - shade]
                # Animación de estado
                alpha = 255
                if state == "sad" and (x + y) % 4 == 0:
                    alpha = 180
                pixels[x, y] = (r, g, b, alpha)

    # Ojos (simplificado)
    eye_y = cy - h // 3
    eye_style = genes.eyes % 4
    if eye_style == 0:   # ojos redondos
        for ex in [cx - 3, cx + 3]:
            if 0 <= ex < SPRITE_SIZE and 0 <= eye_y < SPRITE_SIZE:
                pixels[ex, eye_y] = (10, 10, 10, 255)
    elif eye_style == 1: # ojos brillantes
        for ex in [cx - 3, cx + 3]:
            if 0 <= ex < SPRITE_SIZE and 0 <= eye_y < SPRITE_SIZE:
                pixels[ex, eye_y] = (20, 20, 80, 255)
                if ex + 1 < SPRITE_SIZE:
                    pixels[ex + 1, eye_y] = (240, 240, 255, 200)

    return img

def generate_sprite_sheet(genes: Genes) -> Image.Image:
    """Genera un sprite sheet 4×1 con los frames: idle, happy, sad, battle."""
    states = ["idle", "happy", "sad", "battle"]
    sheet = Image.new("RGBA", (SPRITE_SIZE * 4, SPRITE_SIZE), (0, 0, 0, 0))
    for i, state in enumerate(states):
        frame = generate_sprite_frame(genes, state)
        sheet.paste(frame, (i * SPRITE_SIZE, 0))
    return sheet


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_seed(s: str) -> int:
    """Parsea seed en formato 'A3F0-91C4-77BE-2D08' o hex o decimal."""
    compact = s.replace("-", "").replace(" ", "")
    try:
        return int(compact, 16) & ((1 << 64) - 1)
    except ValueError:
        # Hash FNV-1a (mismo que hashString en genome.ts)
        h = 0xcbf29ce484222325
        prime = 0x100000001b3
        for c in s.encode("utf-8"):
            h = ((h ^ c) * prime) & ((1 << 64) - 1)
        return h

def main() -> None:
    parser = argparse.ArgumentParser(description="Generador de sprites PetBits")
    parser.add_argument("--seed", help="Seed individual (hex o texto)")
    parser.add_argument("--seed-file", help="Archivo con un seed por línea")
    parser.add_argument("--out", required=True, help="Directorio de salida")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    seeds: list[str] = []
    if args.seed:
        seeds.append(args.seed)
    elif args.seed_file:
        seeds = Path(args.seed_file).read_text().splitlines()
    else:
        parser.error("Requiere --seed o --seed-file")

    for seed_str in seeds:
        seed_str = seed_str.strip()
        if not seed_str or seed_str.startswith("#"):
            continue
        seed_int = parse_seed(seed_str)
        genes = decode_genome(seed_int)
        sheet = generate_sprite_sheet(genes)

        # Nombre de archivo: seed normalizado sin guiones
        fname = format(seed_int, "016X")
        fpath = out_dir / f"{fname}.png"
        sheet.save(fpath)
        print(f"  → {fpath} (hue={genes.hue} palette={genes.palette_mode})")

    print(f"✓ {len(seeds)} sprites generados en {out_dir}/")

if __name__ == "__main__":
    main()
