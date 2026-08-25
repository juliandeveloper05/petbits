<div align="center">

# 🧬 PetBits

### Criaturas de datos — cada mascota nace de una semilla de 64 bits

[![TypeScript](https://img.shields.io/badge/TypeScript-5.7-3178C6?style=for-the-badge&logo=typescript&logoColor=white)](https://www.typescriptlang.org/)
[![Vite](https://img.shields.io/badge/Vite-6-646CFF?style=for-the-badge&logo=vite&logoColor=white)](https://vite.dev/)
[![Godot](https://img.shields.io/badge/Godot-4.3-478CBF?style=for-the-badge&logo=godotengine&logoColor=white)](https://godotengine.org/)
[![C++17](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![PWA](https://img.shields.io/badge/PWA-instalable-5A0FC8?style=for-the-badge&logo=pwa&logoColor=white)](https://web.dev/progressive-web-apps/)

**[▶ Jugar](https://petbits.vercel.app)** · [Laboratorio genético](https://petbits.vercel.app/lab.html) · [Roadmap](ROADMAP.md)

</div>

---

## Qué es

Un Tamagotchi donde la criatura **no se elige de una lista: se calcula**.

Escribís un número de 64 bits —o tu nombre, que se convierte en uno— y de ahí
sale todo: la silueta, el color, el carácter, la velocidad a la que gasta
energía, hacia qué lado tiende a evolucionar. El mismo número da siempre la
misma criatura. No hay servidor que decida, ni tablas de botín, ni porcentajes
escondidos.

Las rarezas también salen del número, y son **verificables**. Tu criatura es
Primordial si el seed es primo; es Equilibrada si tiene exactamente 32 de sus 64
bits en 1; es Espejo si sus 16 bits altos son el reflejo de los 16 bajos. Podés
abrir una calculadora y comprobarlo vos mismo. Esa es la diferencia con un
sistema de rarezas que hay que creer.

```
Semilla   A3F0-91C4-77BE-2D08
          └─┬─┘ └─┬┘ └┬┘ └─┬─┘
            │     │   │    └── sesgo de stats, mutación
            │     │   └─────── temperamento, metabolismo, afinidad
            │     └─────────── tono de color, modo de paleta
            └───────────────── linaje, silueta, ojos, boca, apéndices
```

---

## Dos plataformas

La web está terminada y desplegada. El nativo está en construcción y **no la
reemplaza**: comparte los algoritmos, no el código de presentación.

|                  | Web                                              | Nativo                               |
| ---------------- | ------------------------------------------------ | ------------------------------------ |
| **Estado**       | ✅ v2.0 estable                                  | 🚧 v3.0 en curso                     |
| **Stack**        | TypeScript + Vite                                | Godot 4 + C++17 (GDExtension)        |
| **Cómo se abre** | [petbits.vercel.app](https://petbits.vercel.app) | ejecutable (todavía no)              |
| **Guardado**     | IndexedDB                                        | archivo JSON, el mismo formato       |
| **Sprites**      | canvas 32×32 procedural                          | los mismos, generados en C++         |

Los algoritmos en C++ son un port del TypeScript, con tests que comparan los dos
lado a lado. El detalle de qué está portado y qué no está en el
**[roadmap](ROADMAP.md)**.

---

## Sistemas del juego

|                                |                                                                                                      |
| ------------------------------ | ---------------------------------------------------------------------------------------------------- |
| 🧬 **Genoma de 64 bits**       | 14 genes empaquetados en un entero. Mismo número, misma criatura                                     |
| ✨ **8 rarezas emergentes**    | Del 10% al 0,0001%. Propiedades matemáticas del seed, comprobables a mano                            |
| 🎨 **Sprites procedurales**    | Superelipses espejadas, paletas OKLCH, luz direccional. Ninguno está dibujado                        |
| ⏱ **Simulación por timestamp** | La criatura vive con la pestaña cerrada. Al volver, un log de qué pasó                               |
| 💤 **Letargo, no muerte**      | A las 48 horas sin atención se congela. Se pierde vínculo, nunca la criatura                         |
| 🌿 **Evolución ramificada**    | Bebé → 2 juveniles → 4 adultos, según cómo la criaste. El azar entra en el seed; después decidís vos |
| ⚖️ **Acciones con costo**      | Comer de más hace mal, jugar gasta energía, el vínculo tiene tope diario                             |
| 🧪 **Cruza genética**          | Por gen entero, no por bit. El hijo tiene los ojos de uno y el color del otro, y se ve               |
| 🗺 **Expediciones**            | Sale sola y vuelve con comida. El botín lo decide el seed, no un dado                                |
| 📚 **Codex**                   | Lo que descubriste. Diseñado para no completarse nunca                                               |
| 🖼 **Pet Card**                | Tu criatura como PNG, con el seed impreso: quien la reciba puede incubar la misma                    |

---

## Arrancar en dos minutos (web)

Lo único que hace falta es **Node.js 20 o posterior**.

```bash
git clone --recurse-submodules https://github.com/juliandeveloper05/petbits.git
```

```bash
cd petbits && npm install && npm run dev
```

Se abre en `http://localhost:5173`. El laboratorio genético —una grilla de 60
criaturas para ver de un vistazo qué produce el generador— está en
`/lab.html`.

> Si ya lo habías clonado sin `--recurse-submodules`, corré
> `git submodule update --init --recursive`. Solo hace falta para el nativo.

---

## Instalar el entorno nativo (Godot + C++)

Esto es lo que hay que tener para trabajar en la versión nativa. **Nada de esto
hace falta para la web**, así que si solo querés tocar el juego web, saltealo.

Los pasos están en orden y cada uno se puede verificar antes de seguir. La idea
es que si algo falla, sepas exactamente dónde.

### 1. Compilador de C++ — Visual Studio 2022

Bajá **[Visual Studio 2022 Community](https://visualstudio.microsoft.com/es/downloads/)**
(gratis). En el instalador, la parte que importa es una sola:

> ☑ **Desarrollo para el escritorio con C++**

Con marcar esa carga de trabajo alcanza — trae el compilador MSVC v143 y el SDK
de Windows, que son las dos cosas que se necesitan. Son unos 7 GB.

Si preferís no instalar el IDE entero, en la misma página, más abajo, están las
**Herramientas de compilación para Visual Studio 2022**: solo el compilador, sin
editor. Marcá la misma carga de trabajo.

**Verificá que quedó:** buscá en el menú Inicio _"Developer Command Prompt for
VS 2022"_ y abrilo. Escribí `cl` y tiene que responder con la versión del
compilador. Si dice que no se reconoce el comando, la carga de trabajo de C++ no
se instaló.

> ⚠️ Ese _Developer Command Prompt_ no es la consola común. Es una consola con
> las variables de entorno de MSVC ya puestas. Los comandos de C++ de acá abajo
> van ahí, no en PowerShell.
>
> Hay una razón práctica además de las variables: el _Developer Command Prompt_
> es `cmd`, donde `&&` encadena comandos. **Windows PowerShell 5.1 —el que trae
> Windows por defecto— no soporta `&&`** y responde _"El token '&&' no es un
> separador de instrucciones válido en esta versión"_. Si te aparece ese error,
> no es que el comando esté mal: estás en la consola equivocada. En PowerShell 5
> los comandos van de a uno, en líneas separadas.

### 2. Python y SCons

SCons es el sistema de build que usa godot-cpp.

```bash
pip install --user scons
```

El `--user` no es opcional en una instalación de Python hecha para todos los
usuarios. Ahí `C:\PythonXX\Scripts\` pertenece al sistema y hace falta ser
administrador para escribir en esa carpeta. Sin `--user`, pip instala el módulo
—que sí va a otro lado— pero no puede crear el `scons.exe`, y el resultado es
desconcertante: `pip list` muestra SCons instalado y el comando `scons` no
existe. Con `--user` el ejecutable va a tu carpeta personal, que ya está en el
PATH.

**Verificá:** `scons --version` tiene que responder 4.x. Si el comando no
aparece, abrí una terminal nueva — el PATH se lee al arrancar la consola, así
que una que ya estaba abierta no ve el ejecutable recién instalado.

Si por lo que sea el comando sigue sin estar, esto es equivalente y siempre
funciona:

```bash
python -m SCons
```

### 3. Godot

Bajá **[Godot Engine](https://godotengine.org/download/windows/)**, la versión
**estándar** — no la .NET/C#, que es para proyectos en C# y acá no se usa.

No tiene instalador: es un `.exe` suelto. Ponelo donde te quede cómodo.

Probado sobre **4.7.1**. El proyecto quedó marcado con esa versión, así que abrirlo
con una anterior va a dar aviso.

> **Por qué `godot-cpp` está fijado en la rama 4.3 si Godot es 4.7.**
>
> No quedó viejo: es a propósito. GDExtension es compatible hacia adelante —una
> extensión compilada contra godot-cpp 4.3 y declarada con
> `compatibility_minimum = "4.3"` carga en 4.3 y en todas las posteriores—, y
> está comprobado corriendo en 4.7.1. Subir el submódulo a 4.7 **achicaría** el
> rango de versiones que pueden cargarla, no lo agrandaría.

### 4. Compilar la GDExtension

Todo esto va desde el _Developer Command Prompt_, **parado en la carpeta del
repo**. Vale la pena decirlo porque es el error más fácil de cometer y el que
peor se diagnostica: los comandos corren igual desde otro repo, no se quejan de
nada, y simplemente no hacen lo que esperabas.

```bash
cd C:\Users\julia\Desktop\code-26\petBits-25
```

Bajar `godot-cpp`, los bindings de C++ para Godot. Son unos 100 MB y se hace una
sola vez; si clonaste con `--recurse-submodules` ya está y este comando no hace
nada:

```bash
git submodule update --init --recursive
```

```bash
cd gdext
```

```bash
scons -j4
```

La primera vez tarda bastante —diez o quince minutos— porque compila godot-cpp
entero. Las siguientes son segundos: solo recompila lo que tocaste. El `-j4`
usa cuatro núcleos en paralelo.

**Verificá:** tiene que aparecer un archivo en `godot/bin/` llamado
`libpetbits_core.windows.template_debug.x86_64.dll`. El nombre importa: es
exactamente el que busca `godot/bin/petbits_core.gdextension`, y si no coinciden
Godot no carga nada y no explica bien por qué.

### 5. Abrir el proyecto

Abrí Godot, _Importar_, y elegí la carpeta **`godot/`** del repo (no la raíz).

Apretá **F5**: nace una criatura al azar, con su sprite, sus cuatro barras y su
registro de eventos.

Si algo no anda, la escena `scenes/Arranque.tscn` es el diagnóstico: dice si la
GDExtension cargó, y si cargó decodifica un seed conocido y muestra su linaje y
sus rarezas. Comparalo con lo que muestra [la web](https://petbits.vercel.app)
para el mismo seed — tiene que dar igual.

Para ver muchas criaturas de una, la hoja de contacto:

```bash
godot --headless --path godot --script res://scripts/hoja_de_contacto.gd
```

> Las GDExtensions se cargan cuando arranca el editor. Si recompilás con
> `scons`, cerrá y volvé a abrir Godot.

Lo mismo se puede comprobar sin abrir el editor, y sirve para un script:

```bash
godot --headless --path godot --script res://scripts/verificar_puente.gd
```

Comprueba que la clase quede registrada y que los valores lleguen intactos hasta
GDScript. Devuelve 0 si está bien y 1 si algo no coincide.

### Resumen de qué instalar

| Programa                                                               | Versión | Qué marcar / elegir                                          | Para qué                |
| ---------------------------------------------------------------------- | ------- | ------------------------------------------------------------ | ----------------------- |
| [Node.js](https://nodejs.org/)                                         | 20+     | —                                                            | La web (ya lo tenés)    |
| [Visual Studio 2022](https://visualstudio.microsoft.com/es/downloads/) | 2022    | ☑ Desarrollo para el escritorio con C++                      | Compilar el C++         |
| [Python](https://www.python.org/)                                      | 3.10+   | —                                                            | SCons (ya lo tenés)     |
| SCons                                                                  | 4+      | `pip install --user scons` — el `--user` importa, ver arriba | Build de la GDExtension |
| [Godot](https://godotengine.org/download/windows/)                     | 4.3+    | Versión **estándar**, no .NET                                | El motor                |

Para exportar a Android hacen falta además el SDK y el NDK de Android y un JDK
17, pero eso es de la Fase 6 y no sirve de nada tenerlo ahora.

---

## Cómo se verifica que las dos plataformas dan lo mismo

Es la promesa central del proyecto: el mismo seed tiene que dar la misma
criatura en la web y en el nativo. Si eso no se cumple, los saves no son
compatibles y las rarezas que muestra cada uno no coinciden.

```bash
npm run parity
```

Ese comando **ejecuta el TypeScript** de `src/` y vuelca lo que devuelve —2010
genomas, 80 crianzas, 21 entradas de `parseSeed`, 14 escenarios de simulación,
780 rampas de color, 2560 sprites, 23 escenarios de acciones, 3 guardados, 108
botines de expedición, 72 cruzas y 38 pasos de codex— en un header de C++.
Después, desde el _Developer Command Prompt_:

```bash
cd gdext && cmake --build --preset msvc-release && .\build\release\run_tests.exe
```

Estado actual: **80.629 comprobaciones, 0 fallas.**

No hacen falta Godot ni SCons ni godot-cpp: los módulos portados son C++ puro.
Con un compilador alcanza, así que la paridad se puede comprobar antes de
instalar el resto.

**Lo importante es de dónde salen los valores esperados.** Salen de correr el
TypeScript, no de leerlo. Un test con los números escritos a mano no verifica
nada: si quien hizo el port entendió mal el original, lo entiende mal las dos
veces y el test pasa en verde con el bug adentro.

Ese enfoque encontró tres bugs de corrección que no se ven mirando el código —
un desbordamiento de enteros sin signo que dejaba media rama evolutiva
inalcanzable, un hash sobre bytes en vez de unidades UTF-16 que rompía cualquier
seed con tilde, y excepciones en un build que las tiene deshabilitadas. Están
explicados en [`gdext/tests/README.md`](gdext/tests/README.md).

---

## Estructura

```
petbits/
├── src/                    TypeScript — el motor canónico
│   ├── core/               genoma, rarezas, paletas, simulación, evolución,
│   │                       cruza, acciones, expediciones, codex
│   ├── render/             generador de sprites, canvas, Pet Card
│   ├── state/              guardado versionado, migraciones, persistencia
│   ├── game/               bucle del juego, audio
│   └── lab/                laboratorio genético
│
├── gdext/                  GDExtension en C++
│   ├── src/                los mismos algoritmos, portados
│   ├── tests/              paridad TS ↔ C++  ← empezá por acá
│   └── godot-cpp/          submódulo, fijado a 4.3
│
├── godot/                  Proyecto de Godot 4
│   ├── scenes/             las dos pantallas jugables, los arneses de
│   │                       verificación y las herramientas de PNG
│   └── scripts/            GDScript
│
├── tools/                  verify_parity.ts, validar_save.ts
├── scripts/                herramientas de desarrollo del lado web
├── test/                   168 tests de Vitest
└── legacy/                 la versión original en JS, como referencia
```

**La regla que sostiene todo:** `src/core/` no importa nada del DOM. Son
funciones puras. Por eso se puede testear entero, y por eso se puede portar a
C++ sin arrastrar medio navegador.

---

## Comandos

### Web

```bash
npm run dev          # servidor de desarrollo con recarga en caliente
npm run build        # typecheck + lint + 168 tests + build de producción
npm test             # solo los tests
npm run lint         # Biome
```

### Herramientas de desarrollo

```bash
npm run sheet          # hoja de contacto con N criaturas generadas
npm run formas         # todas las formas evolutivas posibles, en pixel art
npm run iconos         # los iconos de la PWA
npm run simular        # simula N días y muestra el log de eventos
npm run parity         # regenera los vectores de paridad para el C++
npm run validar-save   # pasa un save por el esquema de la web
```

`sheet`, `formas` y `simular` existen porque leer el código no alcanza para
saber si el generador produce criaturas o manchas. Cada uno encontró bugs que
los tests no.

### Nativo

```bash
cd gdext && scons                          # la GDExtension, debug
cd gdext && scons target=template_release  # la GDExtension, release
```

El núcleo portado —que es C++ puro y no necesita Godot— se compila y se
verifica aparte:

```bash
cd gdext && cmake --build --preset msvc-release && .\build\release\run_tests.exe
```

### Los arneses de Godot

Cada uno corre una parte del juego de verdad y sale con el número de fallas como
código de salida. Sin ventana, salvo el último.

```bash
godot --headless --path godot res://scenes/VerificarInfinito.tscn    # el mundo infinito
godot --headless --path godot res://scenes/VerificarMundo.tscn       # las dos pantallas, una partida
godot --headless --path godot res://scenes/VerificarInteriores.tscn  # criadero, codex y la cruza
godot --headless --path godot res://scenes/VerificarFuente.tscn      # los 117 glifos
godot --headless --path godot res://scenes/VerificarDialogo.tscn     # el corte de línea de la caja
godot --headless --path godot res://scenes/VerificarSaveRoto.tscn    # un save ilegible no se pisa
godot --headless --path godot res://scenes/MedirLayout.tscn          # que PetView entre en 480x270
godot --headless --path godot --script res://scripts/verificar_puente.gd
godot --headless --path godot --script res://scripts/probar_guardado.gd
```

Y para mirar, que es otra cosa que verificar:

```bash
godot --headless --path godot res://scenes/RegionAPng.tscn   # → godot/region.png
godot --headless --path godot res://scenes/MapaAPng.tscn     # → godot/mapa.png
godot --path godot res://scenes/Capturar.tscn                # → godot/captura_*.png
```

`Capturar` corre **con ventana** a propósito: Godot sin ventana no dibuja, y
pedirle una captura devuelve negro. Camina con el sistema de entrada de verdad,
saca siete capturas y cronometra cada cuadro. Es la única forma de ver cómo se
ve el juego en movimiento — y de saber si cruzar un borde de chunk da un tirón.

---

## Decisiones de diseño

Las que más se notan al usarlo:

**El tiempo es real.** No hay `setInterval` como fuente de verdad. Se guarda el
timestamp del último tick y al abrir se recalcula todo lo que pasó. Si cerrás
tres días, cuando volvés pasaron tres días. Hay un test que verifica que
simular un intervalo de una vez da idéntico a simularlo en pedazos: es el bug
que arruina las simulaciones idle y no se ve hasta que ya está en producción.

**Nadie desinstala por culpa.** El Tamagotchi original te mataba la mascota y
mucha gente no volvía a abrirlo. Acá a las 48 horas entra en letargo: el
deterioro se congela y al volver hay un ritual de reconexión. Perdés vínculo,
no la criatura.

**El vínculo premia la constancia, no el clickeo.** Tiene tope diario. Es lo que
convierte "abrir la app" en un hábito en vez de en una sesión de farmeo.

**Las paletas son OKLCH, no HSL.** En HSL dos colores con la misma luminosidad
se ven uno mucho más oscuro que el otro según el tono, y las criaturas salían
lavadas o ilegibles. El fuera-de-gamut se resuelve bajando croma por búsqueda
binaria, nunca recortando canales.

**El guardado se valida al leer y al escribir.** Validar solo al leer parece
suficiente y no lo es: alcanza con que un cambio en caliente actualice un módulo
antes que otro para escribir un save con la etiqueta de una versión y el
contenido de otra. Un save que no valida se pone en cuarentena, nunca se borra.

---

## Estado y qué sigue

El detalle completo está en el **[roadmap](ROADMAP.md)**. En tres líneas:

- La web está terminada y desplegada.
- **Los catorce módulos del núcleo están portados**, con paridad verificada
  contra vectores generados ejecutando el TypeScript: 80.629 comprobaciones, 0
  fallas. La criatura vive del lado nativo —envejece, evoluciona, entra en
  letargo, se cruza— con los mismos números que la web.
- Se camina un **mundo infinito** con pueblo, interiores, recolección y
  guardado propio. En curso la **Fase 3.6**: que ese mundo se vea como tiene que
  verse. Después viene el combate.

---

## Qué falta y qué está flojo

Esta sección existe porque un repo que solo cuenta lo que anda es un repo en el
que no se puede confiar. Todo lo que sigue está comprobado contra el código, con
archivo y línea en el [roadmap](ROADMAP.md) y en
[gdext/tests/README.md](gdext/tests/README.md).

### Cobertura que no está

La promesa del proyecto es que una misma semilla da la misma criatura de los dos
lados, y que el archivo de guardado es idéntico byte a byte. Estos huecos son
lugares donde esa promesa **hoy se sostiene por casualidad y no por verificación**:

- **`inventory.cpp` es el único módulo portado sin un solo vector generado.**
  Cambiar `inventarioInicial()` del lado TypeScript no mueve un byte del header
  de vectores, el C++ sigue devolviendo lo de antes, la suite da cero fallas — y
  la web arranca con una despensa y el nativo con otra.
- **`SAVE_VERSION` está escrito tres veces sin nada que las ate**: en
  `src/state/save.ts`, como literal `version: 5` en los fixtures de
  `tools/verify_parity.ts`, y en `gdext/src/save_manager.h` — cuyo comentario
  dice "tiene que coincidir con SAVE_VERSION del TS". Los fixtures de guardados
  se arman a mano: el generador nunca llama a `createSave`.
- **Seis funciones tienen gemelo en C++ y cero vectores**: `lineageName`,
  `temperamentName`, `affinityName`, `metabolismName`, `paletteModeName` y
  `formDescription`. Las cadenas se retipean del otro lado y nada las compara.
- **`saludPromedio`, `yaVolvio` y `conoceRareza` no las llama nadie** del lado
  TypeScript, y están portadas igual.
- **Las ramas no nulas de `expedicion` y `ultimaCruzaMs` del escritor** no las
  ejerce ninguna suite: los tres guardados de referencia salen de criaturas
  recién creadas, así que los dos campos viajan en `null`.

### Se ve mal, no está roto

Visto corriendo el juego con ventana, no deducido:

- **El pasto alto se lee como niebla.** El salto de luz contra el pasto base es
  enorme y adentro no hay textura que diga "alto".
- **El pueblo termina en un rectángulo.** Su verde (`#91AE7B`) y el del mundo
  (`#7EA24C`) se encuentran en una línea recta. Hoy lo tapa el cordón de
  árboles; en los huecos de las sendas se va a ver.
- **El pueblo sigue sin rehacerse**: caminos en ele de un tile, entradas de una
  piedra suelta, plaza rectangular. Está en el plan de la Fase 3.6 y no se hizo.

### Fases que no arrancaron

- **Fase 4, combate.** `battle.h` tiene el diseño y **ninguna** de sus siete
  funciones implementada; lo único que se verifica de él es que compile. Las
  criaturas salvajes del pasto alto dependen de esto.
- **Fase 5** (audio y pulido) y **Fase 6** (exportables).

### Suelto

- La acción de entrada `open_codex` está declarada en `project.godot` y no la
  maneja nadie. Quedó de cuando el codex era un menú y ahora es un lugar.

---

## Licencia

MIT.

---

<div align="center">

### Julian Soto

[![GitHub](https://img.shields.io/badge/GitHub-juliandeveloper05-181717?style=for-the-badge&logo=github)](https://github.com/juliandeveloper05)
[![Email](https://img.shields.io/badge/Email-juliansoto.dev@gmail.com-EA4335?style=for-the-badge&logo=gmail&logoColor=white)](mailto:juliansoto.dev@gmail.com)

"La identidad digital no nace del azar, sino del código. La rareza no debería ser una tabla de botín ni un dado oculto, sino una propiedad matemática inmutable de su propio ADN determinista."

</div>

### 👨‍💻 Sobre Mí

Soy Full-Stack Software Engineer especializado en construir sistemas deterministas, arquitecturas local-first y plataformas interactivas complejas. Me apasiona explorar la intersección entre la matemática pura, la generación procedural y el audio digital para crear experiencias únicas, robustas y de alto rendimiento.

### 🛠️ Stack Tecnológico

Frontend & Core: React, React Native, Next.js, TypeScript, Vite.

Backend & Datos: Node.js, Python (FastAPI), PHP, MySQL, Supabase.

Infraestructura & Ops: Git, Vercel, Render, Railway, Sentry.

Creative Coding & Ecosistema: Web Audio API, C++ (Godot / GDExtension), integración de herramientas de IA.
