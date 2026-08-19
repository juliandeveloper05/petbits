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

Los catorce módulos, todos con paridad verificada contra vectores generados
ejecutando el TypeScript.

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
| `breeding.cpp` — cruza por gen | ✅ portado, compilado y verificado |
| `codex.cpp` — linajes, formas y rarezas descubiertas | ✅ portado, compilado y verificado |

**Los tests de paridad** (`gdext/tests/`) comparan 2010 genomas, 80 crianzas, 21
entradas de parseo, 12 hashes, 7 semillas de PRNG, 14 escenarios de simulación,
780 rampas de color, 2560 sprites, 23 escenarios de acciones, 3 guardados y 108
botines de expedición contra lo que devuelve el TypeScript. No hacen falta Godot ni SCons: un
compilador y un comando.

Estado medido con MSVC 2022 sobre Windows: **54.267 comprobaciones, 0 fallas.**

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

**La cruza tenía cuatro trampas** y las cuatro son de las que no fallan
ruidosamente: si te equivocás, el hijo sale distinto y nada más se rompe.

La primera es que la etiqueta con la que se siembra el PRNG lleva el seed en
DECIMAL, porque en JavaScript interpolar un `bigint` rinde base diez. La segunda
es que la etiqueta "A"/"B" de cada gen se decide comparando contra el primer
argumento y no contra el menor de los dos, así que con `seedA > seedB` la "A"
sale cuando el gen vino del mayor — y con dos padres de genoma idéntico, todos
los campos salen "A". La tercera es que el bucle de mutación recorre los 64 bits
siempre, porque cada vuelta consume un número del PRNG y cortar antes correría
la secuencia. La cuarta es que el texto del cooldown redondea hacia arriba: con
división entera, la última hora entera diría "faltan 0 h".

Las cuatro están escritas en `breeding.h`, arriba de todo, porque son
exactamente las que alguien va a querer "corregir".

**El codex no estaba en la lista de módulos y era un agujero igual.** El C++ lo
leía y lo volvía a escribir tal cual, dentro del bloque de campos sin
interpretar. Con tests que lo probaban: el campo viajaba intacto, un save de la
web pasaba por el nativo y volvía sin perder nada.

Y estaba mal por la misma razón por la que estuvo mal el inventario: **nadie lo
escribía**. Podías llegar a una forma adulta nueva en el nativo y el codex no se
enteraba, porque enterarse era trabajo del código que no existía. Ahora
`registrar` se llama al nacer, al cargar y cada vez que la simulación reporta una
evolución — esa última es la que faltaba.

Sus tres ordenamientos son distintos entre sí y ninguno es el obvio: los linajes
van por número, las rarezas por id, y las formas **por su id en texto y no por
el valor del enum**, que va en otro orden completamente. Ordenar por enum produce
un archivo distinto del que escribe el navegador y ningún validador se quejaría.

Interpretarlo permitió además poner las claves donde el TS las pone —
`codex` antes de `inventario`, y no al final— y de paso corregir un comentario
que afirmaba que ya era así cuando no lo era. El orden no cambia el significado,
pero sí el diff entre un save de la web y uno del nativo, y ese diff es la
herramienta con la que se encontró el bug del inventario.

### Fase 2 — Criatura en pantalla 🚧

| | |
|---|---|
| Sprites generados desde el C++ | ✅ 2560 comparados contra el TypeScript |
| `PetBitsCore.sprite()` devuelve una `Image` de 32×32 | ✅ |
| `PetView.tscn` — sprite, barras, registro de eventos, parpadeo | ✅ corre; es la escena principal del proyecto |
| `hoja_de_contacto.gd` — muchas criaturas de una, como `npm run sheet` | ✅ |
| Acciones: alimentar, jugar, acariciar | ✅ con sus tradeoffs y el tope diario de vínculo |
| Tipografía estilo Game Boy, generada por código | ✅ 117 glifos |
| Caja de diálogo con tipeo, paginado y marco | ✅ |
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

**La tipografía también sale de código.** Es una bitmap de 5×7 en una caja de
5×11, con el trazo de un píxel y sin antialiasing que tenían las fuentes de
consola portátil. No es un `.ttf` descargado por dos razones: metería la
licencia de un tercero en el repo por la única cosa que aparece en todas las
pantallas, y una fuente vectorial escalada a once píxeles se ve blanda al lado
de un sprite de 32×32. El texto y el pixel art tienen que estar en la misma
grilla o la pantalla se parte en dos estéticas.

Once filas y no ocho —que es lo que ocupaba una fuente de consola de la época—
porque el castellano no entra en ocho. Con esa altura hay que elegir entre
descendentes de un píxel, una `p` que parece una `o` con un palito, o acentos
pegados a la letra. Tres filas más es lo que cuesta que "Nébula está de
expedición" se lea bien.

**Los acentos no se dibujan, se componen.** La `á` es la `a` con una tilde
estampada dos filas por encima de donde empieza su cuerpo, y el `¿` es el `?`
rotado 180°, que es literalmente lo que es. Dibujar cada variante a mano serían
veinte glifos más y veinte oportunidades de que la tilde de la `ó` quede un
píxel más alta que la de la `á`. La única excepción es la `í`, que primero
pierde su punto y después recibe la tilde.

**Y hubo que medir qué caracteres imprime el juego antes de dibujarlos.** Salió
un censo de los literales del C++ y del GDScript: además del ASCII, el juego usa
`á é í ó ú Á É Ó`, el punto medio como separador (`Musgo · Plácido`), la raya de
diálogo, la flecha de los errores del lector de JSON y el rombo del arranque.
Una fuente a la que le falte uno no falla ruidosamente: dibuja un cuadrado
vacío, y eso aparece en producción.

Los tests del C++ comprueban cobertura, que ningún glifo se repita —el error de
copiar y pegar más fácil de cometer en una tabla de ciento dieciséis entradas— y
que las acentuadas sean su base más algo, arriba y sin comerle tinta. Del lado
de Godot hay otra verificación, porque entre el atlas y la pantalla hay una
traducción entera que puede perder una letra sin avisar:

```bash
godot --headless --path godot res://scenes/VerificarFuente.tscn
```

Como la fuente es de ancho fijo, cualquier texto de N caracteres tiene que medir
exactamente N × 6 píxeles. Si a Godot le falta un glifo, sustituye y la cuenta
no da. Hay además un control negativo —un carácter que la fuente NO tiene tiene
que romper esa cuenta—, sin el cual todo lo demás no probaría nada.

Y para lo que ningún test contesta, que es si se LEE:

```bash
godot --headless --path godot --script res://scripts/fuente_a_png.gd
```

**El presupuesto de pantalla es 480×270** —resolución de consola portátil, que
es la identidad visual del juego— y eso son 254 píxeles útiles de alto. La
primera versión de `PetView` apilaba todo en una columna y pedía unos 400:
Salud y Vínculo quedaban abajo del borde y el registro no se veía nunca. Godot
no avisa de eso, simplemente recorta.

Ahora va en dos columnas y hay un script que lo mide en vez de mirarlo a ojo:

```bash
godot --headless --path godot res://scenes/MedirLayout.tscn
```

Devuelve 1 si el contenido se pasa. Hoy pide 199 px de los 270 — subió al
pasar todo al tamaño nativo de la bitmap font, que es el único que no la
deforma.

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
| El criadero y el codex como interiores caminables | ✅ |
| Transiciones entre mapas, con fundido | ✅ |
| Un NPC con diálogo en la plaza | ✅ |
| Más NPCs y más para hacer en cada lugar | ⬜ |

**La decisión de diseño que ordena esta fase.** Tres de las zonas —patio, bosque,
ruinas— ya existen como mecánica: son destinos de expedición y tardan quince
minutos, hora y media o cuatro horas de tiempo real, mientras el juego está
cerrado.

Si caminar hasta el bosque tardara tres segundos, esa mecánica moriría: irías,
agarrarías el botín y volverías. Así que **el mapa reemplaza al menú, no a la
espera**. Llegás caminando hasta la entrada y ahí la mandás; la expedición sigue
tardando lo que tarda. El mundo vuelve tangible una elección que hoy es una
lista de botones.

**Son DOS interiores y no cinco, y esa es la corrección.** El plan original
decía "las otras cinco zonas como mapas propios", y contradecía la decisión que
ordena la fase entera. Patio, bosque y ruinas son destinos de expedición: tardan
quince minutos, hora y media o cuatro horas de tiempo real con el juego cerrado.
Caminar hasta ellas en tres segundos mataría esa espera, que es media mecánica.
Siguen siendo puertas donde la mandás.

El criadero y el codex sí se volvieron lugares, por el motivo inverso: ahí no
había espera que preservar. Eran un menú, y un menú se puede reemplazar entero
por un lugar sin perder nada.

**`Mundo.gd` dejó de ser "el pueblo" y pasó a ser "un mapa".** Con el pueblo
adentro, cada mapa nuevo habría sido una escena con su copia de caminar, chocar,
hablar y dibujar. Ahora la escena no sabe en cuál está: le pide el script a
`Mapas`, arma el tilemap con su grilla y despacha sus puntos por `tipo`
—"puerta", "expedicion", "cruzar", "estante", "npc"—. Agregar un mapa es agregar
un archivo de datos.

Los tiles de interior van **al final del enum** de `tileset_gen.cpp`, no en su
lugar lógico entre los de afuera. El índice de cada tile es lo que guardan las
grillas, así que insertar uno en el medio las reescribe todas en silencio: el
pueblo seguiría cargando y el pasto sería agua.

**Para que el criadero fuera alcanzable hubo que agregar `incubar`.** Una partida
nativa nunca tenía dos criaturas, así que la cruza habría sido código que no se
puede ejecutar jugando — la peor clase de código, porque parece terminado. El
nativo ya guardaba las semillas que encuentra en las expediciones; lo que
faltaba era convertirlas en criaturas, igual que hace la web.

```bash
godot --headless --path godot res://scenes/VerificarInteriores.tscn
```

Cuarenta comprobaciones: que las tres grillas estén completas y sin índices de
tile inventados, que las entradas no caigan sobre un tile sólido, que a cada
punto se pueda llegar, que las puertas lleven a algún lado, que salir te deje en
la puerta por la que entraste, y que cruzar produzca una tercera criatura que
sobreviva al archivo.

Ese test resultó ser, sin que fuera la intención, **la partida mínima que hay
que jugar para llegar al criadero**. Para cruzar hacen falta dos adultas sanas y
con vínculo, y la tentación era agregarle al C++ un método que pusiera la
criatura en adulta y listo — un backdoor en el código de producción para que
pasara un test. En vez de eso el test cría de verdad: seis días de simulación,
alimentando, acariciando dos veces por día para que no entre en letargo, y
mandándola al patio a buscar comida porque la despensa inicial no da. Es decir
que también prueba que la economía cierra.

Y falló dos veces con razón. La primera simulaba hacia el pasado y no avanzaba
un tick —la criatura acababa de nacer, su reloj ya estaba en "ahora"— y el
síntoma era "todavía no terminó de crecer", que parecía un problema de la regla.
La segunda solo acariciaba: llegó a adulta con vínculo 28 y **salud cero**, seis
días sin comer. La simulación estaba haciendo bien su trabajo y el test estaba
jugando mal.

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
godot --headless --path godot res://scenes/MapaAPng.tscn
```

Compone los tres mapas en una sola imagen, uno debajo del otro. Verlos juntos es
lo que dice si conviven: el pueblo es verde y frío, los interiores son madera, y
esa diferencia es la que hace que entrar a un lugar se sienta como entrar a un
lugar sin necesidad de ninguna transición.

Y encontró algo que ningún test podía decir. Los interiores medían 20 × 13 tiles
y se veían perfectos en el PNG —donde cada mapa se compone solo— pero el
viewport del juego es 30 × 17: en pantalla habrían dejado un tercio en negro a
la derecha. Todos los tests preguntaban por la grilla, y la grilla estaba
impecable. También se vio que el piso de madera, con sus juntas verticales, se
leía igual que la pared de ladrillo: adentro de una sala no se distinguía por
dónde se podía caminar.

**La caja de diálogo cierra la Fase 2.** Es el recurso más viejo del género y
sigue siendo el mejor: escribe letra por letra, espera que le den Enter, pasa de
página. Convierte un mensaje en un momento y le da al jugador el control de
cuánto tarda en leerlo.

Reemplaza a un Label suelto sobre el mapa, y la diferencia no es estética. Un
cartel que siempre está no se lee: se vuelve parte del decorado. Una caja que
aparece, escribe y se va convierte al texto en un evento — y de paso frena al
jugador, que es lo que hace que un diálogo se sienta como una conversación y no
como un cartel de ruta. Por eso, mientras habla, no se camina.

**La caja no se roba el teclado.** Expone `abierta()` y `avanzar()`, y la
pantalla que la contiene decide cuándo llamarlas. Si atajara Enter por su
cuenta, `Mundo` tendría que adivinar si la tecla le llegó o no, y ese es el tipo
de duda que después aparece como "a veces caminaba mientras hablaba".

**El corte de línea se cuenta, no se mide.** La tipografía es de ancho fijo:
seis píxeles por carácter, todos. Eso convierte "¿entra este texto?" en una
cuenta de caracteres en vez de una medición contra el servidor de texto — exacta
y comprobable sin abrir una ventana:

```bash
godot --headless --path godot res://scenes/VerificarDialogo.tscn
```

Treinta y seis frases a cuatro anchos distintos, comprobando que nada se salga
del renglón, que ninguna palabra se pierda y que no queden espacios colgando en
los bordes.

Ese test falló siete veces la primera vez, y las siete tenía razón el código:
rearmaba los renglones con espacios y comparaba contra el original, sin tener en
cuenta que una palabra más larga que el renglón se parte a la fuerza e inventa
un espacio que nunca estuvo. El invariante que vale para todos los anchos es que
la secuencia de caracteres que no son espacio sea la misma.

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

### Fase 3.5 — El mundo infinito 🚧

| | |
|---|---|
| `world_gen.cpp` — ruido de valor, biomas, chunks | ✅ |
| Los cuatro tests: costura, determinismo, variedad, caminabilidad | ✅ |
| `region_a_png.gd` — mirar el mundo antes de que sea jugable | ✅ |
| Cámara y carga de chunks en `Mundo.gd` | ⬜ |
| El pueblo adentro del mundo, con el borde abierto | ⬜ |
| Recolectar, hitos y semillas perdidas | ⬜ |
| `user://mundo.json` — dónde estabas | ⬜ |

**El mundo sale del genoma de tu primera criatura.** Es la tesis del proyecto
llevada hasta el final: la criatura ES la semilla, y ahora el mundo también. La
primera y no la activa, para que incubar o cruzar no cambie el mundo debajo de
tus pies.

**Y caminar no reemplaza a las expediciones.** Es la misma decisión de la Fase 3
sostenida ahora que caminar deja de tener límite: caminar da lo que se consigue
estando ahí, las expediciones siguen siendo el modo idle. No compiten porque no
dan lo mismo.

**El ruido se escribe a mano, sobre `splitmix64`.** No se usa `FastNoiseLite`:
eso dejaría la generación adentro del motor, sin tests que corran con un
compilador y un comando, y con el mundo dependiendo de la versión de Godot
instalada. Así, el mundo de una semilla es el mismo hoy, en otra máquina y dentro
de tres versiones del motor.

**Las costuras no se arreglan: no existen.** El ruido se muestrea en coordenadas
de MUNDO, así que el tile (159, 40) se calcula igual esté al final de un chunk o
al principio del siguiente. La forma natural de escribir un generador por chunks
—un PRNG sembrado por chunk que se consume mientras se recorre la grilla— tiene
exactamente el defecto contrario, y el primer test está para que eso siga siendo
cierto.

**Tres números salieron de medir, no de razonar**, y los tres se equivocaron
primero:

*El contraste.* Promediar octavas concentra el resultado cerca del medio —es el
teorema central del límite haciendo su trabajo— así que los umbrales extremos
casi nunca se cruzaban: 56% de pasto y CERO piedra. Se arregla devolviéndole al
ruido el rango que el promedio le sacó, no moviendo los umbrales hacia el centro.

*La densidad de árboles.* Empezó en 56% razonando que "poco más de la mitad"
dejaría senderos, y el test contestó que desde el centro se llegaba al 17% del
suelo. Hay una constante que lo explica: en una grilla cuadrada el espacio libre
solo percola si supera alrededor del **59,3%** — el umbral de percolación de
sitios. Con 56% de árboles los claros son el 44%, por debajo del umbral, y no se
conectan. No es una cuestión de semilla: es una propiedad de la grilla. Con 34%
los claros quedan en 66% y el bosque se sigue leyendo como bosque.

*La tierra firme alrededor del origen.* Al renderizar las tres primeras semillas,
DOS tenían el origen adentro de un lago: el pueblo habría quedado bajo el agua.
Ningún test lo dijo, porque los cuatro preguntan por el mundo en general y
ninguno por ese punto. Se levanta el terreno con una campana que se apaga sola —
y el primer intento de eso metió el pueblo adentro de un roquedal, que tampoco se
camina. El techo es la otra mitad del piso.

```bash
godot --headless --path godot res://scenes/RegionAPng.tscn
```

Dibuja tres semillas de 160 × 160 tiles, cada tile promediado a un color. A esa
escala no se ve el detalle de un tile y se ven las formas: si los lagos tienen
costa, si los bosques tienen claros, si los biomas se tocan de forma creíble. Y
cuenta lo que hay alrededor del origen, que es lo único que hay que garantizar.

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
