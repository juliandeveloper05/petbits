<div align="center">

# 🧬 PetBits
### *Criaturas de datos — cada mascota nace de una semilla de 64 bits*

[![TypeScript](https://img.shields.io/badge/TypeScript-5.7-3178C6?style=for-the-badge&logo=typescript&logoColor=white)](https://www.typescriptlang.org/)
[![Godot Engine](https://img.shields.io/badge/Godot-4.3-478CBF?style=for-the-badge&logo=godotengine&logoColor=white)](https://godotengine.org/)
[![C++17](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![PWA](https://img.shields.io/badge/PWA-Instalable-5A0FC8?style=for-the-badge&logo=pwa&logoColor=white)](https://web.dev/progressive-web-apps/)
[![Android](https://img.shields.io/badge/Android-Export-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_android.html)
[![License](https://img.shields.io/badge/License-MIT-22c55e?style=for-the-badge)](LICENSE)

*Un Tamagotchi al estilo Pokémon — con genética real, evolución ramificada y rarezas matemáticas*

[🌐 Demo Web](https://petbits.vercel.app) · [📖 Documentación](#-documentación) · [🛠 Setup](#-instalación-local) · [🗺 Roadmap](#-roadmap)

</div>

---

## 🎮 ¿Qué es PetBits?

PetBits es un simulador de criaturas virtuales donde **cada mascota existe a partir de un número de 64 bits**. No hay tablas de sprites ni catálogos: el cuerpo, el color, el carácter y hasta la rareza se calculan matemáticamente a partir del seed.

El proyecto corre en **dos plataformas paralelas**:

| Plataforma | Stack | Estado |
|---|---|---|
| 🌐 **Web PWA** | TypeScript 5 + Vite + PocketBase | ✅ Estable (v2.0) |
| 🎮 **Nativo** | Godot 4 + C++17 (GDExtension) | 🚧 En desarrollo (v3.0) |

Ambas plataformas comparten la **misma lógica de juego** — los algoritmos en C++ son un port directo del TypeScript, con tests de paridad que garantizan resultados idénticos para cualquier seed.

---

## ✨ Sistemas del juego

### Heredados del TS (conservados y portados a C++)

| Sistema | Descripción |
|---|---|
| 🧬 **Genoma 64-bit** | Seed determinista → criatura única. Mismo número = misma criatura siempre |
| ⏱ **Simulación por timestamp** | El tiempo corre aunque no estés. Off-screen sin `setInterval` |
| 🌿 **Evolución ramificada** | Bebé → 2 Juveniles → 4 Adultos según **cómo la criaste** |
| ✨ **Rarezas emergentes** | Miller-Rabin, Hamming weight, bit-mirror — verificables matemáticamente |
| 🗺 **Expediciones** | Salidas con botín determinista. Evita el soft-lock económico |
| 🧪 **Breeding genético** | Cruza por gen (no por bit), con mutación controlada |
| 📚 **Codex + Multi-criatura** | Colección persistente, tarjeta compartible |
| ⚡ **Sistema de acciones** | Feed/play con tradeoffs. Bond daily cap anti-spam |

### Nuevos en Godot v3.0

| Sistema | Descripción |
|---|---|
| 🗺️ **Mundo navegable** | Mapa top-down estilo Game Boy Color con 6 zonas |
| ⚔️ **Combate por turnos** | 4 movimientos por Form, tabla de afinidades 8×8 |
| 🎨 **Sprites animados** | 4 frames por estado: idle, happy, sad, battle |
| 🎵 **Audio nativo** | BGM por zona, SFX, temas dinámicos |
| 📱 **Export Android** | APK nativo además de la PWA |

---

## 🏗 Arquitectura del proyecto

```
petbits/
│
├── src/                    # TypeScript — motor canónico de lógica
│   ├── core/               # genome, simulation, evolution, traits, breeding…
│   ├── game/               # main.ts, audio.ts
│   ├── render/             # spriteGen, canvas, petCard
│   └── state/              # save, persistence, migrations
│
├── legacy/                 # Versión JS original (conservada como referencia)
├── backend/                # PocketBase (Go)
│
├── godot/                  # Proyecto Godot 4 (nuevo)
│   ├── project.godot
│   ├── scenes/             # Pet, World, Battle, Codex, Breeding, UI
│   ├── assets/             # sprites, tilesets, audio, fonts
│   ├── scripts/            # GDScript (glue code)
│   └── bin/                # GDExtension compilada (.dll/.so)
│
├── gdext/                  # C++ GDExtension (nuevo)
│   ├── src/                # genome, simulation, evolution, traits, battle…
│   ├── tests/              # Tests Catch2 de paridad TS ↔ C++
│   └── SConstruct          # Build system
│
└── tools/                  # Scripts de pipeline
    ├── sprite_gen.py        # Genera sprites desde seeds (port de spriteGen.ts)
    └── verify_parity.ts     # Compara output TS vs C++ para N seeds
```

---

## 🛠 Instalación local

### Requisitos

| Herramienta | Versión | Para |
|---|---|---|
| Node.js | 20+ | Web PWA |
| Git | — | Ambos |
| Python | 3.10+ | Tools de pipeline |
| Godot Engine | 4.3+ | Versión nativa |
| C++ compiler | C++17 | GDExtension (MSVC / GCC / Clang) |
| SCons | 4+ | Build de GDExtension |

---

### 🌐 Web PWA (TypeScript)

```bash
# Clonar el repositorio
git clone https://github.com/juliandeveloper05/petbits.git
cd petbits

# Instalar dependencias
npm install

# Servidor de desarrollo
npm run dev
# → http://localhost:5173

# Tests
npm test

# Build de producción
npm run build
```

#### Backend (PocketBase)

```powershell
# Windows
cd backend
.\setup.ps1

# Linux / Mac
cd backend
wget https://github.com/pocketbase/pocketbase/releases/download/v0.22.0/pocketbase_0.22.0_linux_amd64.zip
unzip pocketbase_0.22.0_linux_amd64.zip
./pocketbase serve
# → http://127.0.0.1:8090
```

---

### 🎮 Versión Godot + C++

#### 1. Inicializar submodulos

```bash
git submodule update --init --recursive
# Descarga godot-cpp (bindings de C++ para Godot 4)
```

#### 2. Compilar la GDExtension

```bash
cd gdext

# Debug (para desarrollo)
scons

# Release
scons target=template_release

# Android ARM64
scons platform=android arch=arm64 target=template_release
```

La biblioteca compilada se coloca automáticamente en `godot/bin/`.

#### 3. Abrir el proyecto en Godot

1. Abrir Godot 4.3+
2. Importar proyecto desde la carpeta `godot/`
3. Godot detecta la GDExtension automáticamente
4. ▶ Run

#### 4. (Opcional) Generar sprites desde seeds

```bash
cd tools
pip install pillow

# Sprite de una criatura específica
python sprite_gen.py --seed "A3F0-91C4-77BE-2D08" --out ../godot/assets/sprites/

# Batch desde archivo
python sprite_gen.py --seed-file seeds.txt --out ../godot/assets/sprites/
```

---

## 🗺 Roadmap

```
v2.0 ✅  Web PWA estable
         └─ Genoma, simulación, evolución, rarezas, expediciones, breeding, codex

v3.0 🚧  Godot + C++ (en desarrollo)
     │
     ├── Fase 0  ✅  Infra: estructura del repo, scaffolding GDExt, README
     ├── Fase 1  🔲  Port del core TypeScript a C++ con tests de paridad
     ├── Fase 2  🔲  Godot base: escena de criatura, sprites animados, UI
     ├── Fase 3  🔲  Mundo navegable (6 zonas, NPCs, diálogos)
     ├── Fase 4  🔲  Sistema de combate por turnos (4 Forms × 4 movimientos)
     ├── Fase 5  🔲  Audio nativo, efectos, polish
     └── Fase 6  🔲  Export Windows/Linux/Android + GitHub Releases

v3.1 📋  Post-lanzamiento
         └─ Leaderboard global, i18n EN/PT, modo multijugador local
```

---

## 🧪 Testing

```bash
# Tests del TypeScript (simulación, genome, evolution…)
npm test

# Tests C++ (paridad TS ↔ C++)
cd gdext
scons
./tests/run_tests

# Verificación cruzada TS ↔ C++ (10.000 seeds)
npx vite-node tools/verify_parity.ts --seeds=10000
```

### Invariantes de paridad garantizados

Para cualquier seed de 64 bits, los siguientes resultados son **idénticos** en TypeScript y C++:

- `decodeGenome(seed)` → misma struct `Genes`
- `resolverAdulto(crianza, genes, juvenil)` → mismo `Form`
- `detectTraits(seed)` → mismas rarezas
- `simulate(state, 0, 7days)` == `simulate(simulate(state, 0, 3days), 3days, 7days)` *(invariante de partición)*

---

## 🚀 Deploy

### Web (Vercel)

```bash
# El deploy es automático con cada push a master
git push origin master
# → https://petbits.vercel.app
```

### Backend (Fly.io)

```bash
cd backend
flyctl launch
flyctl deploy
```

### Godot — GitHub Releases

```bash
# Los binarios se construyen automáticamente via GitHub Actions
# Ver .github/workflows/godot-export.yml (Fase 6)
```

---

## 📁 Scripts disponibles

```bash
npm run dev          # Servidor de desarrollo
npm run build        # Build de producción (typecheck + lint + test + vite)
npm run test         # Tests con Vitest
npm run typecheck    # TypeScript sin emitir
npm run lint         # Biome check
npm run format       # Biome format
npm run sheet        # Genera contact sheet de sprites (TS)
npm run simular      # Simula N días de una criatura (TS)
npm run formas       # Muestra todas las formas posibles (TS)
```

---

## 🤝 Contribuir

1. Fork del proyecto
2. Crea una rama para tu feature (`git checkout -b feat/mi-feature`)
3. Commit con mensaje descriptivo (`git commit -m '✨ feat: descripción'`)
4. Push a la rama (`git push origin feat/mi-feature`)
5. Abre un Pull Request

### Convenciones de commit

```
✨ feat:    nueva funcionalidad
🐛 fix:     corrección de bug
🔧 chore:   cambios de mantenimiento
🧪 test:    tests
📝 docs:    documentación
🎨 style:   formato / sin cambios de lógica
♻️ refactor: refactorización
⚡ perf:    mejora de rendimiento
```

---

## 📄 Licencia

Este proyecto está bajo la Licencia MIT — ver [LICENSE](LICENSE) para más detalles.

---

<div align="center">

## 👤 Developer

**Julián**
| | |
|---|---|
| 🐙 GitHub | [@juliandeveloper05](https://github.com/juliandeveloper05) |
| 📦 Repo | [github.com/juliandeveloper05/petbits](https://github.com/juliandeveloper05/petbits) |

---

*"La rareza no es una tabla de loot ni un tirón de dados oculto. Es una propiedad matemática del propio seed."*

**¿Te gustó el proyecto? ¡Dale una ⭐!**

Hecho con ❤️, mucho ☕ y algunos números primos

</div>
