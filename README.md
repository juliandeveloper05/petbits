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

| | Web | Nativo |
|---|---|---|
| **Estado** | ✅ v2.0 estable | 🚧 v3.0 en curso |
| **Stack** | TypeScript + Vite | Godot 4 + C++17 (GDExtension) |
| **Cómo se abre** | [petbits.vercel.app](https://petbits.vercel.app) | ejecutable (todavía no) |
| **Guardado** | IndexedDB | archivo, compatible con el de la web |
| **Sprites** | canvas 32×32 procedural | pendiente |

Los algoritmos en C++ son un port del TypeScript, con tests que comparan los dos
lado a lado. El detalle de qué está portado y qué no está en el
**[roadmap](ROADMAP.md)**.

---

## Sistemas del juego

| | |
|---|---|
| 🧬 **Genoma de 64 bits** | 14 genes empaquetados en un entero. Mismo número, misma criatura |
| ✨ **8 rarezas emergentes** | Del 10% al 0,0001%. Propiedades matemáticas del seed, comprobables a mano |
| 🎨 **Sprites procedurales** | Superelipses espejadas, paletas OKLCH, luz direccional. Ninguno está dibujado |
| ⏱ **Simulación por timestamp** | La criatura vive con la pestaña cerrada. Al volver, un log de qué pasó |
| 💤 **Letargo, no muerte** | A las 48 horas sin atención se congela. Se pierde vínculo, nunca la criatura |
| 🌿 **Evolución ramificada** | Bebé → 2 juveniles → 4 adultos, según cómo la criaste. El azar entra en el seed; después decidís vos |
| ⚖️ **Acciones con costo** | Comer de más hace mal, jugar gasta energía, el vínculo tiene tope diario |
| 🧪 **Cruza genética** | Por gen entero, no por bit. El hijo tiene los ojos de uno y el color del otro, y se ve |
| 🗺 **Expediciones** | Sale sola y vuelve con comida. El botín lo decide el seed, no un dado |
| 📚 **Codex** | Lo que descubriste. Diseñado para no completarse nunca |
| 🖼 **Pet Card** | Tu criatura como PNG, con el seed impreso: quien la reciba puede incubar la misma |

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

**Verificá que quedó:** buscá en el menú Inicio *"Developer Command Prompt for
VS 2022"* y abrilo. Escribí `cl` y tiene que responder con la versión del
compilador. Si dice que no se reconoce el comando, la carga de trabajo de C++ no
se instaló.

> ⚠️ Ese *Developer Command Prompt* no es la consola común. Es una consola con
> las variables de entorno de MSVC ya puestas. Los comandos de C++ de acá abajo
> van ahí, no en PowerShell.
>
> Hay una razón práctica además de las variables: el *Developer Command Prompt*
> es `cmd`, donde `&&` encadena comandos. **Windows PowerShell 5.1 —el que trae
> Windows por defecto— no soporta `&&`** y responde *"El token '&&' no es un
> separador de instrucciones válido en esta versión"*. Si te aparece ese error,
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

Todo esto va desde el *Developer Command Prompt*, **parado en la carpeta del
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

Abrí Godot, *Importar*, y elegí la carpeta **`godot/`** del repo (no la raíz).

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

| Programa | Versión | Qué marcar / elegir | Para qué |
|---|---|---|---|
| [Node.js](https://nodejs.org/) | 20+ | — | La web (ya lo tenés) |
| [Visual Studio 2022](https://visualstudio.microsoft.com/es/downloads/) | 2022 | ☑ Desarrollo para el escritorio con C++ | Compilar el C++ |
| [Python](https://www.python.org/) | 3.10+ | — | SCons (ya lo tenés) |
| SCons | 4+ | `pip install --user scons` — el `--user` importa, ver arriba | Build de la GDExtension |
| [Godot](https://godotengine.org/download/windows/) | 4.3+ | Versión **estándar**, no .NET | El motor |

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
genomas, 80 crianzas, 21 entradas de parseo, 12 hashes, 14 escenarios de
simulación, 780 rampas de color y 2560 sprites— en un header de C++. Después,
desde el *Developer Command Prompt* en `gdext/tests`:

```bash
cl /std:c++17 /EHsc /utf-8 /O2 /Fe:run_tests.exe test_parity.cpp ..\src\genome.cpp ..\src\traits.cpp ..\src\evolution.cpp ..\src\rng.cpp ..\src\simulation.cpp ..\src\palette.cpp ..\src\sprite_gen.cpp && run_tests.exe
```

Estado actual: **50.071 comprobaciones, 0 fallas.**

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
│   ├── scenes/             pantalla de arranque (por ahora)
│   └── scripts/            GDScript
│
├── tools/                  verify_parity.ts, sprite_gen.py
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
npm run sheet        # hoja de contacto con N criaturas generadas
npm run formas       # todas las formas evolutivas posibles, en pixel art
npm run simular      # simula N días y muestra el log de eventos
npm run parity       # regenera los vectores de paridad para el C++
```

`sheet`, `formas` y `simular` existen porque leer el código no alcanza para
saber si el generador produce criaturas o manchas. Cada uno encontró bugs que
los tests no.

### Nativo

```bash
cd gdext && scons                          # debug
cd gdext && scons target=template_release  # release
```

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

El detalle completo está en el **[roadmap](ROADMAP.md)**. En dos líneas:

- La web está terminada y desplegada.
- Del nativo hay cinco módulos portados con paridad verificada (41.051
  comprobaciones, 0 fallas) y la GDExtension cargando en Godot 4.7.1. La
  criatura ya vive del lado nativo: envejece, evoluciona y entra en letargo con
  los mismos números que la web. **Lo próximo es la Fase 2**, poner eso en
  pantalla.

---

## Licencia

MIT.

---

<div align="center">

### Julian Soto

[![GitHub](https://img.shields.io/badge/GitHub-juliandeveloper05-181717?style=for-the-badge&logo=github)](https://github.com/juliandeveloper05)
[![Email](https://img.shields.io/badge/Email-juliansoto.dev@gmail.com-EA4335?style=for-the-badge&logo=gmail&logoColor=white)](mailto:juliansoto.dev@gmail.com)

*"La rareza no es una tabla de botín ni un tiro de dados escondido.
Es una propiedad matemática del propio número."*

</div>
