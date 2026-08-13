# Tests de paridad TypeScript ↔ C++

Verifican que un mismo seed produzca exactamente la misma criatura en la web y
en el nativo. Es la promesa central del proyecto: si esto falla, los saves no
son compatibles y las rarezas que muestra cada plataforma no coinciden.

**No hacen falta Godot, SCons, godot-cpp ni ningún framework de tests.** Los
tres módulos que se verifican son C++ puro y no incluyen un solo header del
motor, así que alcanza con un compilador. Se pueden correr el primer día,
antes de instalar nada más.

---

## Cómo funciona

Son dos piezas:

| | |
|---|---|
| `npm run parity` | Ejecuta el TypeScript de `src/core/` y vuelca lo que devuelve en `vectores_generados.h` |
| `test_parity.cpp` | Compila contra ese header y compara, valor por valor |

Lo importante es de dónde salen los valores esperados. **Salen de correr el
TypeScript, no de leerlo.** Un test con los números escritos a mano no verifica
la paridad: si quien hizo el port entendió mal el original, lo entiende mal las
dos veces y el test pasa en verde con el bug adentro.

El archivo generado se versiona a propósito. Así el test compila apenas clonás
el repo, sin pasar por Node — y además cualquier cambio de comportamiento del
TypeScript aparece como un diff, que es justo lo que uno quiere ver.

---

## Correr los tests

Desde `gdext/tests/`:

### Windows — MSVC

Desde una consola *Developer Command Prompt for VS 2022* (la común no tiene `cl`
en el PATH):

```bash
cl /std:c++17 /EHsc /utf-8 /O2 /Fe:run_tests.exe test_parity.cpp ..\src\genome.cpp ..\src\traits.cpp ..\src\evolution.cpp ..\src\rng.cpp ..\src\simulation.cpp ..\src\palette.cpp ..\src\sprite_gen.cpp ..\src\actions.cpp ..\src\inventory.cpp ..\src\json.cpp ..\src\save_manager.cpp && run_tests.exe
ng.cpp ..\src\simulation.cpp ..\src\palette.cpp ..\src\sprite_gen.cpp ..\srcctions.cpp ..\src\inventory.cpp ..\src\json.cpp ..\src\save_manager.cpp && run_tests.exe
```

`/utf-8` no es adorno: los catálogos están llenos de acentos y sin esa opción
MSVC lee los archivos con la codificación regional de Windows. Los nombres
llegan con la acentuación rota y algún vector de `parseSeed` falla sin motivo
aparente.

### Windows — MinGW, Linux o macOS

```bash
g++ -std=c++17 -O2 test_parity.cpp ../src/genome.cpp ../src/traits.cpp ../src/evolution.cpp ../src/rng.cpp ../src/simulation.cpp ../src/palette.cpp ../src/sprite_gen.cpp ../src/actions.cpp ../src/inventory.cpp ../src/json.cpp ../src/save_manager.cpp -o run_tests && ./run_tests
```

### Desde Visual Studio 2022

Hay un `CMakeLists.txt` en `gdext/`. En Visual Studio:

> **Archivo → Abrir → Carpeta…** y elegir `gdext/`

VS lo detecta solo y configura todo. Después:

| | |
|---|---|
| Compilar | `Ctrl+Shift+B` |
| Correr con el depurador | Elegir `run_tests.exe` como elemento de inicio y `F5` |
| Correr sin depurar | `Ctrl+F5` |

Elegí la configuración **msvc-release** para correr la paridad y **msvc-debug**
para depurar. Los cincuenta mil chequeos tardan unas décimas de segundo en
release y alrededor de un segundo en debug, así que la diferencia no molesta:
elegí por lo que necesites, no por la velocidad.

Cada configuración deja su ejecutable en su propia carpeta —`build/release/` y
`build/debug/`— así que se pueden tener las dos compiladas a la vez sin que una
pise a la otra.

Es lo que conviene cuando algo no coincide: se pone un breakpoint adentro de
`simulate()` y se miran los stats tick a tick, en vez de deducir qué pasó
leyendo un número al final.

> **Esto no reemplaza a SCons, y no puede.** La GDExtension la tiene que
> construir SCons, porque godot-cpp trae su propio sistema de build y genera
> cientos de archivos de binding antes de compilar nada. Lo que cubre CMake es
> el núcleo portado, que es C++ puro y es justamente donde se quiere un
> depurador. Los dos compilan las mismas fuentes con el mismo compilador.

---

## Salida esperada

```
PetBits — paridad TypeScript <-> C++
2010 genomas, 80 crianzas, 21 parseos, 12 hashes, 14 simulaciones,
780 rampas de color, 2560 sprites, 23 escenarios de acciones

decodeGenome / formatSeed
parseSeed
hashString (FNV-1a 64)
detectTraits / rarityTier
resolverJuvenil / resolverAdulto
mulberry32 / deriveSeed
buildRamp (OKLCH)
generateSprite
simulate
alimentar / jugar / acariciar
cargar y guardar la partida
la despensa se gasta
guardados corruptos
invariante de partición
reloj hacia atrás

50557 comprobaciones, 0 fallas
Paridad OK: el C++ da exactamente lo mismo que el TypeScript.
```

Solo se imprimen las fallas. Si no aparece ninguna línea `FALLA`, está todo
bien. El programa devuelve 1 si algo no coincide, así que sirve tal cual en un
script.

---

## Qué se verifica hoy

| Módulo | Qué se compara | Casos |
|---|---|---|
| `genome.cpp` | Los 17 campos de `decodeGenome`, más `formatSeed` | 2010 genomas |
| `genome.cpp` | `parseSeed` y `hashString` (FNV-1a 64 sobre UTF-16) | 21 + 12 textos |
| `traits.cpp` | Las 8 rarezas y el tier agregado | 2010 genomas |
| `evolution.cpp` | `resolverJuvenil` y `resolverAdulto` | 10 crianzas × 8 afinidades |
| `rng.cpp` | `mulberry32` y `deriveSeed` | 7 semillas × 8 salidas |
| `palette.cpp` | Los 5 colores de la rampa, canal por canal | 260 genomas × 3 formas |
| `sprite_gen.cpp` | El buffer RGBA entero, por hash, más el conteo de opacos | 2560 sprites |
| `simulation.cpp` | Estado completo, stats y conteo de eventos | 14 escenarios |
| `actions.cpp` | Stats, crianza, tope de vínculo y el mensaje al jugador | 23 escenarios |
| `save_manager.cpp` | Lee saves de la web, los reescribe y no pierde nada | 3 saves + 11 corruptos |
| `inventory.cpp` | Se gasta de a una, no baja de cero, y solo cobra si la acción salió bien | 1 bloque |

Los sprites se comparan por hash y no píxel por píxel porque cada uno son 4096
bytes: ponerlos crudos en el header serían megabytes. Junto al hash va el conteo
de píxeles opacos, y los dos juntos distinguen el tipo de falla — si cambió el
hash pero el conteo coincide, la silueta está bien y el problema es de color; si
cambió el conteo, se movió la geometría. Con el hash solo, un fallo dice "algo
cambió" y nada más.

Dos comprobaciones no salen de vectores porque son propiedades del código y no
del TypeScript:

**El invariante de partición.** Simular un intervalo de una sola vez tiene que
dar exactamente lo mismo que simularlo en pedazos. Se prueba con diez cortes
elegidos para caer en lugares incómodos: sobre el cambio de día, sobre el borde
del letargo, y en números que no son múltiplos redondos.

Es *la* propiedad de la que depende que el juego funcione. El jugador cierra la
pestaña cuando quiere, así que el mismo tiempo transcurrido se simula partido de
mil maneras distintas. Rompe si el azar se arrastra entre ticks en vez de
sembrarse por índice, o si algún tick lee el reloj de afuera en vez del de su
propia frontera.

**El reloj hacia atrás** no tiene que perder nada ni avanzar el tiempo.

---

## El guardado se verifica en la otra dirección

Los tests de acá comprueban que el C++ lee un save de la web y que lo que
escribe lo puede volver a leer él mismo. Eso deja afuera lo que de verdad
importa: que la WEB pueda leer lo que escribió el nativo.

Un round-trip contra uno mismo es fácil de pasar estando equivocado — basta con
equivocarse igual al leer y al escribir. Así que hay un paso más:

```bash
npm run parity && cd gdext && cmake --build --preset msvc-release
```

```bash
gdext/build/release/escribir_save.exe save.json && npm run validar-save -- save.json
```

`validar-save` pasa el archivo por el `parseSave` real de la web, con su esquema
de Zod. Si eso acepta, un save del nativo se abre en el navegador.

---

## Sobre comparar números con coma

Los stats se comparan por igualdad **exacta**, sin tolerancia, y es a propósito.

Una tolerancia sería lo razonable si esto midiera algo físico. Acá los dos lados
hacen las mismas operaciones en el mismo orden sobre los mismos IEEE-754: si
difieren aunque sea en el último bit, es que el orden o una constante no
coinciden. Y eso se amplifica tick a tick — a los mil cuatrocientos cuarenta de
un día, dos criaturas que arrancaron iguales ya no lo son. Un epsilon escondería
justamente lo que hay que ver.

Por eso los valores esperados en el header se ven así:

```
92.2119999999997
```

Ese número no está mal escrito. Es el resultado exacto de acumular en punto
flotante, y es el mismo que da la web. Si diera `92.212` redondo, algo estaría
haciendo las cuentas distinto.

Los 2010 genomas son 2000 de `splitmix64` con semilla fija más 10 elegidos a
mano. Los de a mano importan más de lo que parece: entre dos mil seeds al azar
no sale el cero, ni el máximo, ni un pangrama, ni el primo más grande que entra
en 64 bits. Son exactamente los valores donde un port se rompe.

## Qué falta

`breeding.cpp` y `expeditions.cpp` todavía no están portados, así que no hay
vectores para ellos. Cuando se porten, se agregan al generador y acá.

`battle.cpp` es distinto: no existe del lado web, así que no hay contra qué
comparar. Ese va a necesitar tests propios, escritos como tests de verdad y no
como vectores de paridad.

---

## Cuándo hay que volver a correr esto

Siempre que se toque `src/core/genome.ts`, `traits.ts` o `evolution.ts`. El
orden es: cambiar el TS → `npm run parity` → mirar el diff del header → ajustar
el C++ hasta que el test vuelva a dar cero fallas.

Si el diff del header sale más grande de lo esperado, ahí hay algo para mirar:
un cambio chico en el TS que mueve miles de vectores casi siempre significa que
movió algo que no se quería mover.
