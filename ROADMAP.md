# Roadmap

Estado real de cada pieza, sin optimismo. Un ✅ acá significa que existe, corre
y está verificado; si algo funciona a medias lo dice.

Última revisión: 11 de agosto de 2026.

---

## v2.0 — Web PWA ✅ terminada y desplegada

En [petbits.vercel.app](https://petbits.vercel.app). 168 tests en verde,
instalable como PWA, funciona sin internet.

| | Estado |
|---|---|
| Genoma de 64 bits, rarezas emergentes, paletas OKLCH | ✅ |
| Generador procedural de sprites + laboratorio genético | ✅ |
| Simulación por timestamp, letargo, log "mientras no estabas" | ✅ |
| Evolución ramificada, acciones con costo | ✅ |
| Persistencia versionada (v1→v5) con validación y cuarentena | ✅ |
| Audio chiptune, animación, PWA offline | ✅ |
| Codex, cruza, despensa, expediciones, Pet Card | ✅ |

---

## v3.0 — Godot 4 + C++ 🚧 en curso

La web no se toca ni se reemplaza. El nativo es una capa aparte que reusa los
mismos algoritmos.

### Fase 0 — Infraestructura ✅

| | |
|---|---|
| Estructura `godot/`, `gdext/`, `tools/` | ✅ |
| `godot-cpp` como submódulo, fijado a 4.3 | ✅ |
| SConstruct que compila y nombra la biblioteca como Godot la busca | ✅ |
| El proyecto de Godot abre, carga la extensión y arranca | ✅ probado en 4.7.1 |
| `.gitignore` para artefactos de SCons, Godot y Python | ✅ |

Sin GitHub Actions: el CI vive dentro de `npm run build`, que corre typecheck,
lint y tests antes de compilar. Vercel ejecuta ese comando, así que código roto
no llega a producción.

### Fase 1 — Port del núcleo a C++ 🚧

Tres módulos de siete. Los tres portados tienen paridad verificada contra
vectores generados ejecutando el TypeScript.

| Módulo | Estado |
|---|---|
| `genome.cpp` — decodificación, formato, parseo, hash | ✅ portado, compilado y verificado |
| `traits.cpp` — las 8 rarezas, Miller-Rabin | ✅ portado, compilado y verificado |
| `evolution.cpp` — ejes de crianza, resolución de forma | ✅ portado, compilado y verificado |
| `rng.cpp` — splitmix64, mulberry32, deriveSeed | ✅ portado, compilado y verificado |
| `simulation.cpp` — tick, letargo, eventos, evolución | ✅ portado, compilado y verificado |
| `palette.cpp` — paletas OKLCH, 8 modos, sesgo por forma | ✅ portado, compilado y verificado |
| `sprite_gen.cpp` — el pixel art de 32×32 | ✅ portado, compilado y verificado |
| `petbits_core.cpp` — puente a GDScript | ✅ carga en Godot 4.7.1 |
| `breeding.cpp` — cruza por gen | ⬜ ni empezado |
| `actions.cpp` — alimentar, jugar, acariciar | ⬜ ni empezado |
| `expeditions.cpp` — destinos, botín determinista | ⬜ ni empezado |
| `save_manager.cpp` — leer y escribir los saves de la web | ⬜ ni empezado |

**Los tests de paridad** (`gdext/tests/`) comparan 2010 genomas, 80 crianzas, 21
entradas de parseo, 12 hashes, 7 semillas de PRNG y 14 escenarios de simulación
contra lo que devuelve el TypeScript. No hacen falta Godot ni SCons: un
compilador y un comando.

Estado medido con MSVC 2022 sobre Windows: **41.051 comprobaciones, 0 fallas.**

Además se comprueba el **invariante de partición** —simular de una vez da lo
mismo que simular en pedazos— con diez cortes distintos, y el caso del reloj
corrido hacia atrás. Esas dos no salen de vectores: son propiedades del código.

Encontraron tres bugs de corrección que no se ven leyendo el código —
desbordamiento de enteros sin signo, hash sobre bytes en vez de unidades
UTF-16, y excepciones en un build que las tiene deshabilitadas. Están en
`gdext/tests/README.md`.

La biblioteca compila y linkea:
`godot/bin/libpetbits_core.windows.template_debug.x86_64.dll`, exportando
`petbits_gdextension_init` — el mismo nombre de archivo y el mismo símbolo que
declara el `.gdextension`.

**La cadena está cerrada de punta a punta.** Godot 4.7.1 carga la extensión,
`PetBitsCore` queda registrada, y los valores llegan intactos hasta GDScript —
incluidos los nombres con acento y los seeds con el bit 63 encendido, que son
los dos lugares donde el puente podía romperse solo. Se comprueba sin abrir el
editor:

```bash
godot --headless --path godot --script res://scripts/verificar_puente.gd
```

Esa verificación encontró un bug de conversión en `version()`: se armaba el
String con el constructor desde `const char*`, que no interpreta UTF-8, y el "·"
salía como "Â·". El resto del puente usaba el helper correcto; el error estaba
justo en la línea que no pasaba por él.

**Próximo paso concreto:** la Fase 2. El núcleo que necesita ya está: la
criatura tiene stats reales, envejece, evoluciona y entra en letargo, todo
accesible desde GDScript. Lo que falta portar (`breeding`, `actions`,
`expeditions`) es de sistemas de meta y no bloquea tener algo en pantalla.

### Fase 2 — Criatura en pantalla 🚧

| | |
|---|---|
| Sprites generados desde el C++ | ✅ 2560 comparados contra el TypeScript |
| `PetBitsCore.sprite()` devuelve una `Image` de 32×32 | ✅ |
| `PetView.tscn` — sprite, barras, registro de eventos, parpadeo | ✅ corre; es la escena principal del proyecto |
| `hoja_de_contacto.gd` — muchas criaturas de una, como `npm run sheet` | ✅ |
| Acciones: alimentar, jugar, acariciar | ⬜ necesita portar `actions.cpp` |
| Tipografía y caja de diálogo estilo Game Boy | ⬜ |

**La decisión de los sprites quedó tomada: se portó el algoritmo.** Las otras
dos opciones —dejar `tools/sprite_gen.py` o dibujarlos a mano— rompían la
promesa del proyecto, porque no hay forma de verificar que un dibujo hecho
aparte coincida con lo que calcula la web. Portándolo, el sprite entra en la
misma disciplina que todo lo demás: los vectores salen de ejecutar el
TypeScript y se compara el buffer completo.

`tools/sprite_gen.py` queda obsoleto. Era un boceto en HSL sin paridad; ahora
hay una implementación de verdad y conviene borrarlo antes de que alguien lo
use creyendo que sirve.

### Fase 3 — Mundo navegable ⬜

Seis zonas: Pueblo, El Patio, El Bosque, Las Ruinas, El Criadero, El Codex.
Tilemap 16×16, transiciones, NPCs con diálogo.

### Fase 4 — Combate por turnos ⬜

Cuatro movimientos por forma adulta, tabla de afinidades 8×8, criaturas
salvajes generadas con la misma pipeline de seeds. `gdext/src/battle.h` tiene el
diseño escrito; falta la implementación.

Lo que hay que cuidar acá: las recompensas de combate no pueden romper la
economía que ya está balanceada del lado web.

### Fase 5 — Audio y pulido ⬜

BGM por zona, efectos, partículas en la evolución, shader CRT opcional,
importar saves de la web arrastrando el archivo.

### Fase 6 — Exportables ⬜

Windows, Linux y Android. Binarios en GitHub Releases.

---

## Fuera del alcance por ahora

**Nube.** Cuentas, sincronización entre dispositivos, galería pública de seeds.
Estaba en el plan original de la web como fase opcional y sigue ahí. El juego
es local-first y funciona entero sin servidor; agregar uno es una decisión de
producto, no una deuda técnica.

**PocketBase.** La carpeta `backend/` quedó de la versión vieja del proyecto. La
web actual no la usa: guarda en IndexedDB y no habla con ningún servidor.

---

## Cómo se decide qué sigue

El orden no es negociable en un punto: **la Fase 1 se termina antes de empezar
la 2**. Si la simulación no está portada, la criatura en pantalla no puede
tener stats reales, y lo que se construya encima habría que rehacerlo. El resto
del orden sí es discutible.
