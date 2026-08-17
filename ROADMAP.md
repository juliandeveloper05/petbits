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

Doce módulos de trece. Todos los portados tienen paridad verificada contra
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
| `actions.cpp` — alimentar, jugar, acariciar | ✅ portado, compilado y verificado |
| `inventory.cpp` — la despensa | ✅ portado, compilado y verificado |
| `json.cpp` — lector y escritor, con orden de claves estable | ✅ portado, compilado y verificado |
| `save_manager.cpp` — leer y escribir los saves de la web | ✅ validado con el esquema real de la web |
| `expeditions.cpp` — destinos, botín determinista | ✅ portado, compilado y verificado |
| `breeding.cpp` — cruza por gen | ⬜ ni empezado |

**Los tests de paridad** (`gdext/tests/`) comparan 2010 genomas, 80 crianzas, 21
entradas de parseo, 12 hashes, 7 semillas de PRNG, 14 escenarios de simulación,
780 rampas de color, 2560 sprites, 23 escenarios de acciones, 3 guardados y 108
botines de expedición contra lo que devuelve el TypeScript. No hacen falta Godot ni SCons: un
compilador y un comando.

Estado medido con MSVC 2022 sobre Windows: **51.000 comprobaciones, 0 fallas.**

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

Falta `breeding`, que es el único sistema de meta que queda y no bloquea nada:
necesita dos criaturas adultas con vínculo alto para que se note.

### Fase 2 — Criatura en pantalla 🚧

| | |
|---|---|
| Sprites generados desde el C++ | ✅ 2560 comparados contra el TypeScript |
| `PetBitsCore.sprite()` devuelve una `Image` de 32×32 | ✅ |
| `PetView.tscn` — sprite, barras, registro de eventos, parpadeo | ✅ corre; es la escena principal del proyecto |
| `hoja_de_contacto.gd` — muchas criaturas de una, como `npm run sheet` | ✅ |
| Acciones: alimentar, jugar, acariciar | ✅ con sus tradeoffs y el tope diario de vínculo |
| Tipografía y caja de diálogo estilo Game Boy | ⬜ |
| Guardado, compatible con el de la web | ✅ |
| Expediciones: la criatura sale y vuelve con comida | ✅ |

**La economía quedó cerrada, y hubo un momento en que no lo estaba.** Al hacer
que alimentar cobrara del inventario, el nativo pasó de tener comida infinita a
tener siete unidades y ninguna forma de conseguir más: a las siete alimentadas
la partida quedaba muerta. Las expediciones son la otra mitad de ese arreglo, no
una función aparte.

Por eso el patio no pide etapa ni cuesta energía. Es la trampa clásica de una
economía cerrada —si para conseguir comida hace falta comida, el jugador queda
trabado— y hay un test que recorre cuarenta salidas comprobando que nunca vuelva
con las manos vacías.

**El presupuesto de pantalla es 480×270** —resolución de consola portátil, que
es la identidad visual del juego— y eso son 254 píxeles útiles de alto. La
primera versión de `PetView` apilaba todo en una columna y pedía unos 400:
Salud y Vínculo quedaban abajo del borde y el registro no se veía nunca. Godot
no avisa de eso, simplemente recorta.

Ahora va en dos columnas y hay un script que lo mide en vez de mirarlo a ojo:

```bash
godot --headless --path godot res://scenes/MedirLayout.tscn
```

Devuelve 1 si el contenido se pasa. Hoy pide 177 px de los 270.

**El guardado es el formato de la web, y eso está verificado contra su propio
validador**, no contra el criterio de este código:

```bash
npm run validar-save -- "%APPDATA%/PetBits/partida.json"
```

Pasa el `parseSave` real —el mismo esquema de Zod y la misma validación cruzada
de `activaId` que corren en el navegador—. Un round-trip del C++ contra sí mismo
no habría probado nada: alcanza con equivocarse igual al leer y al escribir.

Lo que el C++ todavía no interpreta —codex y semillas— se lee y se vuelve a
escribir tal cual. Abrir tu partida en el nativo no te borra el codex, y eso es
lo que hace seguro compartir un formato entre dos programas que no están igual
de completos.

**El inventario sí se interpreta**, y esa distinción salió de jugar. Dejarlo
opaco parecía inofensivo: se guardaba y se devolvía intacto, con tests que lo
probaban. Pero nadie lo descontaba, así que del lado nativo la comida era
infinita — y como la dieta decide la rama evolutiva, se podía empujar una
evolución sin pagar lo que la web sí cobra.

No lo encontró ningún test. Apareció pasando una partida real de la web al
nativo y de vuelta, y comparando los dos archivos. Vale como recordatorio de
que "el formato viaja bien" y "las dos plataformas juegan el mismo juego" son
dos afirmaciones distintas.

Un save que no carga **no se borra**: se renombra a `partida.rota.json`. Puede
ser recuperable a mano, y esa decisión le toca al dueño de la partida.

**La decisión de los sprites quedó tomada: se portó el algoritmo.** Las otras
dos opciones —dejar `tools/sprite_gen.py` o dibujarlos a mano— rompían la
promesa del proyecto, porque no hay forma de verificar que un dibujo hecho
aparte coincida con lo que calcula la web. Portándolo, el sprite entra en la
misma disciplina que todo lo demás: los vectores salen de ejecutar el
TypeScript y se compara el buffer completo.

`tools/sprite_gen.py` queda obsoleto. Era un boceto en HSL sin paridad; ahora
hay una implementación de verdad y conviene borrarlo antes de que alguien lo
use creyendo que sirve.

### Fase 3 — Mundo navegable 🚧

| | |
|---|---|
| `tileset_gen.cpp` — los 8 tiles, generados por código | ✅ |
| El pueblo: caminos, plaza, estanque, borde de árboles | ✅ |
| Caminar con colisiones | ✅ |
| `mapa_a_png.gd` — compone el mundo en un PNG para poder mirarlo | ✅ |
| Una sola partida para las dos pantallas (autoload `Partida`) | ✅ |
| Mandar a la criatura desde la entrada de cada zona | ✅ |
| Ir y volver entre el pueblo y PetView | ✅ |
| Las otras cinco zonas como mapas propios | ⬜ |
| Transiciones entre zonas | ⬜ |
| NPCs con diálogo | ⬜ |

**La decisión de diseño que ordena esta fase.** Tres de las zonas —patio, bosque,
ruinas— ya existen como mecánica: son destinos de expedición y tardan quince
minutos, hora y media o cuatro horas de tiempo real, mientras el juego está
cerrado.

Si caminar hasta el bosque tardara tres segundos, esa mecánica moriría: irías,
agarrarías el botín y volverías. Así que **el mapa reemplaza al menú, no a la
espera**. Llegás caminando hasta la entrada y ahí la mandás; la expedición sigue
tardando lo que tarda. El mundo vuelve tangible una elección que hoy es una
lista de botones.

**Los tiles no tienen contraparte en la web**, y eso cambia cómo se verifican.
Todo lo demás en `gdext/` es un port con un TypeScript que dice cuál es la
respuesta correcta; acá no hay contra qué comparar. Sus tests comprueban
propiedades —que los tiles sean opacos, que se distingan entre sí, que el atlas
sea determinista— en vez de igualdad. Es una red más floja, y conviene tenerlo
presente: un tile feo pasa esos tests sin problema.

Por eso está `mapa_a_png.gd`. Godot en modo headless no dibuja nada —pedirle una
captura devuelve negro— así que el mapa se compone a mano, tile por tile, con
las mismas imágenes que usa el juego:

```bash
godot --headless --path godot --script res://scripts/mapa_a_png.gd
```

**Las dos pantallas tenían que ser un solo juego, y no lo eran.** Cada escena
creaba su propio `PetBitsCore`. Con una sola pantalla no se notaba; con el mapa
quedó a la vista: el pueblo mostraba una criatura al azar, no la tuya, y
mandarla de expedición desde ahí no habría tocado la partida que estabas
jugando. Dos cores son dos juegos corriendo a la vez.

Ahora el core, el guardado, el reloj y la bitácora viven en un autoload
(`Partida`), y las escenas se cuelgan de él. Cambiar de pantalla destruye nodos,
no la partida — hasta el registro de "mientras no estabas" sobrevive al viaje de
ida y vuelta.

Eso también es lo que hace que el mapa sirva para algo: pararse en la entrada
del patio y apretar Enter es **exactamente la misma llamada** que el botón de
PetView, sobre el mismo estado y el mismo archivo.

Y se comprueba, porque es justo el tipo de bug que una captura no muestra —una
pantalla rota se ve igual de bien—:

```bash
godot --headless --path godot res://scenes/VerificarMundo.tscn
```

Diecisiete comprobaciones que siguen a una criatura desde PetView al pueblo,
la mandan al patio caminando, releen el archivo con un core nuevo que no sabe
nada de lo que acaba de pasar, y vuelven a PetView a confirmar que sigue afuera.

Ese test encontró dos cosas de una. Que `add_child` durante `_ready()` falla con
un error en consola en vez de una excepción: la pantalla nunca entraba al árbol
y las afirmaciones daban verde igual, contra el autoload. Y que un script
lanzado con `--script` **reemplaza el main loop**, así que los autoloads no se
instancian y el script ni siquiera compila — por eso `medir_layout` y
`VerificarMundo` son escenas y no `--script`, y por eso la forma del pueblo se
mudó a `PuebloMapa.gd`, que no depende de nada.

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
