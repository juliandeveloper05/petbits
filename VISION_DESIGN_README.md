# 🧬 PetBits — Vision & Game Design Document
## *Lo que es, lo que será y cómo lo vas a ver mientras lo construimos*

---

## 1. El juego hoy (versión web — lo que ya existe)

### ¿Qué es PetBits v2.0?

PetBits es un **Tamagotchi con genética matemática**. En vez de elegir una mascota de una lista, le das un número — el **seed** — y de ese número emerge una criatura única. El mismo número siempre da la misma criatura. No hay servidor que decida, no hay loot tables, no hay porcentajes escondidos: todo es matemática pura y determinista.

La web PWA **ya funciona y está desplegada**. Abrís el navegador, escribís un seed (o le pedís uno al azar), y nace tu criatura.

---

### ¿Cómo se ve hoy?

La interfaz web actual tiene **estética retro de consola portátil** — algo a mitad de camino entre un Game Boy y una terminal de los 90:

```
┌──────────────────────────────┐
│  🧬 PetBits                  │
│  ─────────────────────────── │
│                              │
│     [   SPRITE 32×32   ]     │  ← pixel art generado desde tu seed
│     Brasa · Voraz            │  ← linaje + temperamento del genoma
│     Anda pipí cucú           │  ← estado de ánimo en frase
│                              │
│  Energía  ████████░░  82     │
│  Ánimo    ███████░░░  71     │  ← cuatro barras de stats
│  Salud    ██████████  100    │
│  Vínculo  ████░░░░░░  38     │
│                              │
│  [ Alimentar ] [ Jugar ]     │
│  [ Acariciar ] [ Codex ]     │  ← acciones del jugador
│  [ Expedición ] [ Tarjeta ]  │
│                              │
│  Seed: A3F0-91C4-77BE-2D08   │
└──────────────────────────────┘
```

#### El sprite

El sprite de 32×32 **no está pre-dibujado**. Se genera en tiempo real en un `<canvas>` a partir del genoma. El algoritmo:

1. Lee el gen `bodyShape` → elige uno de 8 **arquetipos** (blob, chibi, alto, redondo, esbelto, cuadrado, gota, ancho)
2. Aplica la **Form evolutiva** como modificador de proporciones (Coloso = más ancho y cuadrado, Oráculo = más fino y alto)
3. Dibuja el cuerpo con **superelipses** (como las de los íconos de iPhone) en media grilla y espeja
4. Agrega **apéndices** (orejas, cuernos, alas, cola) según el gen `appendages`
5. Aplica **paleta OKLCH** — un espacio de color perceptualmente uniforme para que todas las criaturas tengan buen contraste
6. Pinta el **sombreado** con dirección de luz fija (arriba-izquierda) para que parezca dibujado a mano
7. Dibuja los **ojos** encima de todo (el detalle que hace que lea como ser vivo)
8. Si tiene rarezas, agrega efectos visuales (aura, patrón especial en la piel)

Todo eso escala a 6× o 9× según la pantalla, con interpolación nearest-neighbor para que el pixel art no se vea borroso.

---

### ¿Cómo se puede ver hoy?

**Vercel (producción):**
```
https://petbits.vercel.app
```
👉 **Esto sigue funcionando exactamente igual**. No cambia nada en la web.

**Local:**
```bash
npm install
npm run dev
# → http://localhost:5173
```

---

### Los sistemas que ya tiene

#### 🧬 Genoma 64-bit
El seed es un entero de 64 bits. Se desempaqueta en campos:
- 4 bits → Linaje (16 opciones: Nébula, Fungo, Cristal, Limo, Pluma, Escama...)
- 4 bits → Forma del cuerpo
- 4 bits → Ojos
- 4 bits → Boca
- 4 bits → Apéndices (bitfield: cuernos, orejas, alas, cola)
- 8 bits → Tono de color (0-255 → 0°-360°)
- 3 bits → Modo de paleta
- 3 bits → Temperamento (Plácido, Curioso, Arisco, Leal, Errante, Voraz, Tímido, Feroz)
- 3 bits → Metabolismo (qué tan rápido gasta energía)
- 3 bits → Afinidad elemental (Brasa, Marea, Raíz, Chispa, Escarcha, Polvo, Eco, Vacío)
- etc.

#### ⏱ Simulación por timestamp real
La criatura **vive aunque no estés mirando**. El sistema guarda el timestamp del último tick y al volver calcula todo lo que pasó. Una criatura sola 48 horas entra en letargo — igual que un Tamagotchi real.

#### 🌿 Evolución ramificada
```
         Bebé
          │
    ┌─────┴─────┐
  Pétreo    Vaporoso          ← depende de qué comió (proteína vs dulce)
    │              │
┌───┴───┐    ┌────┴───┐
Coloso  Guardián  Errante  Oráculo   ← depende de cuánto jugó vs acarició
```
La misma criatura criada de formas opuestas termina siendo **un bicho completamente distinto**.

#### ✨ Rarezas matemáticas (8 en total)

| Rareza | Regla | Frecuencia |
|---|---|---|
| Equilibrado | Exactamente 32 bits en 1 | ~10% |
| Vacío | 24 bits o menos en 1 | ~3% |
| Saturado | 40 bits o más en 1 | ~3% |
| Racha | 9+ bits consecutivos en 1 | ~5.5% |
| Primordial | El seed es un número primo | ~2.1% |
| Uróboros | Primer byte = último byte | ~0.35% |
| Espejo | Los 16 bits altos son mirror de los 16 bajos | ~0.0015% |
| Pangrama | Los 16 nibbles son los 16 valores sin repetir | ~0.00011% |

#### 🗺 Expediciones
La criatura puede salir a explorar y vuelve sola después del tiempo acordado:
- El Patio (15 min, sin requisito) — trae 1-2 alimentos
- El Bosque (90 min, juvenil+) — trae 2-3 items, chance de encontrar un seed
- Las Ruinas (4 horas, adulto) — trae 3-5 items, 55% de encontrar un seed

El botín es **determinista**: lo decide el seed + el timestamp de salida. Si reiniciás, el botín no cambia.

#### 🧪 Breeding genético
Dos criaturas adultas con suficiente vínculo (20+) pueden cruzarse. El hijo hereda genes de ambos padres **por campo completo** (no bit a bit), más 2-3 mutaciones. El resultado es visible: tiene los ojos de uno y el color del otro.

#### 📚 Codex
Registro de todo lo que descubriste: linajes vistos, formas alcanzadas, rarezas encontradas. Los legendarios (Espejo, Pangrama) son casi imposibles — el codex está diseñado para que nunca se complete del todo.

---

## 2. Lo que estamos construyendo (Godot + C++ — v3.0)

### La idea central

Tomamos todo lo que ya existe en la web y le agregamos **una capa de juego nativa** al estilo Pokémon. La metáfora es directa:

> *Si Tamagotchi le preguntás "¿cómo está tu criatura?", Pokémon te pregunta "¿qué puede hacer tu criatura?"*

PetBits v3.0 responde las dos preguntas.

---

### ¿Cómo se va a ver?

La versión Godot tiene **resolución base de 480×270** (exactamente la de Game Boy Color escalado) con pixel art 16×16 tiles. La paleta visual tiene 4 colores por sprite — clásica GBC.

```
┌─────────────────────────────────────────┐
│ 🌳 PUEBLO PETBITS                       │
│                                         │
│  ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒  │
│  ▒ [~] [~] [~]      ╔══╗  ╔══╗       ▒ │  ← tilemap top-down 16×16
│  ▒  ▓▓▓▓▓▓▓▓▓▓▓▓   ║  ║  ║  ║       ▒ │
│  ▒  ▓ JUGADOR ▓▓▓   ╚══╝  ╚══╝       ▒ │
│  ▒  ▓▓▓▓▓▓▓▓▓▓▓▓               NPC  ▒ │
│  ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒  │
│                                         │
│  ┌──────────────────────────────┐       │
│  │ Hola, entrenador. Tu criatura│       │  ← dialog box estilo GBC
│  │ Prisma Voraz quiere explorar.│       │    typewriter character by char
│  └──────────────────────────────┘       │
└─────────────────────────────────────────┘
```

#### Pantalla de criatura
```
┌───────────────────────────────────────────┐
│  PRISMA · COLOSO        Nv.12            │
│  ┌─────────────┐   HP  ████████░  82/100 │
│  │             │   EN  ████████░  78/100 │
│  │  [SPRITE    │   ÁN  █████░░░░  52/100 │
│  │  ANIMADO    │   VÍ  ████████   90/100 │
│  │  4 FRAMES]  │                         │
│  │             │   ✨ PRIMORDIAL          │
│  └─────────────┘   🌟 URÓBOROS            │
│                                           │
│  [ ALIMENTAR ] [ JUGAR ] [ EXPEDICIÓN ]   │
│  [ ACARICIAR ] [ CODEX ] [ BATALLA   ]   │
└───────────────────────────────────────────┘
```

#### Pantalla de combate
```
┌───────────────────────────────────────────┐
│                       ERRANTE SALVAJE     │
│                       HP  ████░░░░  45%  │
│                       ┌──────────────┐   │
│                       │  [SPRITE     │   │
│                       │  ADVERSARIO] │   │
│                       └──────────────┘   │
│  ┌──────────────┐                        │
│  │  [SPRITE     │  TU COLOSO             │
│  │  PROPIO]     │  HP  ████████  82%     │
│  └──────────────┘                        │
│                                          │
│  ╔════════════════════════════════╗      │
│  ║ ¿Qué va a hacer COLOSO?       ║      │
│  ╠════════════════════════════════╣      │
│  ║ ▶ GOLPE PÉTREO  │ TERRAFORMAR ║      │
│  ║   AGUANTE       │ TERREMOTO   ║      │
│  ╚════════════════════════════════╝      │
└───────────────────────────────────────────┘
```

---

### Las 6 zonas del mundo

```
┌────────────────────────────────────────────┐
│                  MAPA MUNDO                │
│                                            │
│    [PUEBLO]────[EL PATIO]                  │
│       │                                    │
│    [EL BOSQUE]                             │  Cada zona = una expedición
│       │           [EL CRIADERO]            │  del TS convertida en lugar
│    [LAS RUINAS]       │                    │  físico navigable
│                   [EL CODEX]               │
└────────────────────────────────────────────┘
```

- **Pueblo PetBits**: Inicio. NPC Profesor te explica el juego. Tienda de items.
- **El Patio**: Zona de exploración rápida (mirror de la expedición "patio"). Criaturas débiles.
- **El Bosque**: Zona media. Criaturas de nivel moderado. Seeds salvajes.
- **Las Ruinas**: Zona avanzada (solo adultos). Criaturas raras. Seeds legendarios.
- **El Criadero**: Zona de breeding visual. Ves la herencia gen a gen.
- **El Codex**: Pokédex visual. Las formas que descubriste, las rarezas encontradas.

---

### Los 4 sets de movimientos de combate

Los movimientos salen de la **Form** de la criatura — de cómo la criaste:

| Form | Movimientos | Estilo |
|---|---|---|
| **Coloso** | Golpe Pétreo, Terraformar, Aguante, Terremoto | Tanque. Mucho daño, lento |
| **Guardián** | Escudo Vital, Regenerar, Custodiar, Aura de Defensa | Defensor. Se cura, protege |
| **Errante** | Ráfaga, Esquivar, Explorar, Destello | Velocista. Ataca primero, huye bien |
| **Oráculo** | Visión, Confundir, Absorber, Profecía | Soporte. Debilita, roba vida |

Y la **Afinidad elemental** define ventajas/desventajas como los tipos en Pokémon:
```
AFINIDADES (tabla 8×8 de multiplicadores):
Brasa  → Super efectivo contra Escarcha, Raíz
Marea  → Super efectivo contra Brasa, Polvo
Raíz   → Super efectivo contra Marea, Eco
Chispa → Super efectivo contra Marea, Escarcha
... (8 tipos × 8 tipos = 64 interacciones)
```

---

## 3. ¿Qué pasa con Vercel? ¿Sigue funcionando?

### Respuesta corta: **Sí. La web no cambia.**

La versión Godot es una **capa adicional** al proyecto, no un reemplazo. El código TypeScript del `src/` queda exactamente igual. Vercel sigue desplegando la PWA web como siempre.

Lo que cambia es que el repo ahora tiene **dos productos**:

```
PetBits en la Web           PetBits Nativo
──────────────────          ─────────────────────
URL: petbits.vercel.app     .exe / .apk descargable
Stack: TypeScript + Vite    Stack: Godot 4 + C++
Cómo abrís: navegador       Cómo abrís: ejecutable
Backend: PocketBase ✓       Backend: mismo PocketBase ✓
Saves: IndexedDB            Saves: archivo JSON compatible
Sprites: canvas 32×32       Sprites: AnimatedSprite2D
```

Los saves son **compatibles entre plataformas**. El seed de tu criatura web la carga en Godot y viceversa — porque el algoritmo C++ y el TypeScript son el mismo código con distinto lenguaje, con tests que lo comprueban.

---

## 4. ¿Cómo lo ves mientras trabajás?

### Web (TypeScript) — sin cambios

```bash
npm run dev
# → http://localhost:5173
# Recarga en caliente. Cambiás un .ts y el navegador se actualiza solo.
```

Para ver sprites específicos:
```bash
npm run sheet   # genera una hoja de contacto con N criaturas
npm run simular # simula N días y muestra el log de eventos
npm run formas  # muestra todas las formas posibles en pixel art
```

### Godot + C++ — workflow de desarrollo

```
┌─────────────────────────────────────────────────┐
│ Workflow de desarrollo Godot                    │
│                                                 │
│  1. Editás C++ en VS Code                       │
│        ↓                                        │
│  2. scons  (en la carpeta gdext/)               │
│        ↓                                        │
│  3. La .dll se copia a godot/bin/ automático    │
│        ↓                                        │
│  4. En el Godot Editor: Project → Reload        │
│     (o F5 para correr directamente)             │
│        ↓                                        │
│  5. Ves el juego en la ventana del editor       │
└─────────────────────────────────────────────────┘
```

**Para GDScript** (la UI, las escenas): Godot tiene recarga en caliente. Cambiás el `.gd`, guardás, y el juego se actualiza sin recompilar.

**Para C++**: necesitás recompilar con SCons (unos segundos para cambios pequeños, ~30 segundos para cambios grandes). Godot detecta el nuevo `.dll` solo.

### Android

```bash
# Compilar GDExtension para Android
cd gdext
scons platform=android arch=arm64 target=template_release

# Exportar desde Godot
# Project → Export → Android → Export Project
# (necesita Android SDK y NDK configurados)
```

Durante desarrollo lo más práctico es trabajar en desktop y exportar a Android solo para testing de UX/controles.

---

## 5. El juego en números

| Elemento | Cantidad | Notas |
|---|---|---|
| Linajes posibles | 16 | Nébula, Fungo, Cristal, Limo... |
| Formas evolutivas | 6 | 2 juveniles + 4 adultos |
| Rarezas | 8 | De ~10% a ~0.00011% |
| Afinidades elementales | 8 | Brasa, Marea, Raíz... |
| Temperamentos | 8 | Plácido, Curioso, Arisco... |
| Arquetipos de cuerpo | 16 | 8 arquetipos × 2 (con/sin patas) |
| Seeds distintos posibles | 2⁶⁴ ≈ 1.8 × 10¹⁹ | Sin repetición práctica |
| Movimientos de combate | 16 | 4 Forms × 4 movimientos cada una |
| Zonas del mundo | 6 | Pueblo, Patio, Bosque, Ruinas, Criadero, Codex |
| Alimentos | 4 | Baya, Raíz, Larva, Cristal |
| Destinos de expedición | 3 | Patio, Bosque, Ruinas |

---

## 6. Principios de diseño que guían todo

### "La criatura existe aunque no la mires"
El tiempo es real. No hay `setInterval` como fuente de verdad. Si dejás tu criatura 3 días, cuando volvás habrá pasado exactamente ese tiempo, con todos sus ticks procesados de una.

### "La rareza se verifica, no se cree"
Las 8 rarezas son propiedades matemáticas comprobables. Podés abrir una calculadora, mirar tu seed, y verificar vos mismo si es primo, si tiene 32 bits en 1, si los nibbles son todos distintos. No hay porcentajes escondidos ni pity timers.

### "La crianza importa más que el azar"
Dos criaturas con el mismo genoma criadas distinto terminan siendo diferentes. El azar entra en el seed inicial — después, el jugador decide.

### "Toda acción tiene costo y beneficio"
Comer demasiado hace mal. Jugar gasta energía. El vínculo tiene tope diario. No hay botones que solo sumen barras — cada decisión tiene tradeoffs.

### "El Codex nunca se completa del todo"
El Pangrama (1 en 880.000) y el Espejo (1 en 65.000) aseguran que siempre haya algo que descubrir. El juego no tiene final.

---

## 7. El puente entre TS y C++

La parte técnica más importante del proyecto: los algoritmos del TypeScript se portan a C++ byte a byte, con tests que comprueban paridad exacta.

```typescript
// TypeScript (genome.ts)
export function decodeGenome(seed: bigint): Genes {
  const normalized = seed & MASK64;
  return {
    lineage:    bits(normalized,  0, 4),
    bodyShape:  bits(normalized,  4, 4),
    // ...
  };
}
```

```cpp
// C++ (genome.cpp)
Genes decodeGenome(Seed seed) {
    Genes g;
    g.lineage    = static_cast<uint8_t>(bits(seed,  0, 4));
    g.bodyShape  = static_cast<uint8_t>(bits(seed,  4, 4));
    // ...
    return g;
}
```

Son dos pasos. El primero ejecuta el TypeScript y guarda lo que devuelve; el
segundo compila el C++ contra esos valores y compara.

```bash
npm run parity
```

```bash
cd gdext/tests && cl /std:c++17 /EHsc /utf-8 /O2 /Fe:run_tests.exe test_parity.cpp ..\src\genome.cpp ..\src\traits.cpp ..\src\evolution.cpp && run_tests.exe
```

Lo importante es que los valores esperados **salen de correr el TypeScript, no
de leerlo**. Escribirlos a mano sería pedirle al mismo criterio que hizo el port
que se corrija solo. El detalle está en [`gdext/tests/README.md`](gdext/tests/README.md).

Esto es lo que garantiza que un save de la web se pueda cargar en Godot y al
revés: son el mismo juego con distinta presentación.

---

> **Este documento describe hacia dónde va el proyecto, no dónde está.**
> Para el estado real de cada pieza —qué está portado, qué compila, qué
> todavía es un header vacío— el archivo es [ROADMAP.md](ROADMAP.md).

**Developer**: Julian Soto — [@juliandeveloper05](https://github.com/juliandeveloper05) · [juliansoto.dev@gmail.com](mailto:juliansoto.dev@gmail.com)
**Repo**: [github.com/juliandeveloper05/petbits](https://github.com/juliandeveloper05/petbits)
