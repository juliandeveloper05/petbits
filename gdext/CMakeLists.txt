# Proyecto para trabajar el núcleo C++ dentro de Visual Studio 2022.
#
#   En Visual Studio: Archivo → Abrir → Carpeta… → elegir gdext/
#
# VS detecta este archivo solo y configura todo: IntelliSense sobre el código
# real, compilación con Ctrl+Shift+B, y F5 para correr los tests de paridad con
# el depurador enganchado. Se pueden poner breakpoints adentro de simulate() y
# mirar los stats tick a tick.
#
# ---
#
# ESTO NO REEMPLAZA A SCONS, Y NO PUEDE.
#
# La GDExtension la tiene que construir SCons, porque godot-cpp trae su propio
# sistema de build y genera cientos de archivos de binding a partir de la API
# del motor antes de compilar nada. Reimplementar eso en CMake sería mantener
# una copia de algo que ya existe y que cambia con cada versión de Godot.
#
# Lo que sí cubre esto es el núcleo portado, que es C++ puro —no incluye un solo
# header de Godot— y es donde de verdad se quiere un depurador. El reparto queda
# así:
#
#   CMake / Visual Studio  →  escribir y depurar el núcleo, correr la paridad
#   SCons                  →  construir el .dll que carga Godot
#
# Los dos compilan las mismas fuentes con el mismo compilador, así que lo que
# funciona en uno funciona en el otro.

cmake_minimum_required(VERSION 3.21)
project(petbits_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Los módulos portados: C++ puro, sin nada del motor. petbits_core.cpp y
# register_types.cpp quedan afuera a propósito — esos sí dependen de godot-cpp
# y solo los puede compilar SCons.
add_library(petbits_nucleo STATIC
    src/actions.cpp
    src/inventory.cpp
    src/json.cpp
    src/save_manager.cpp
    src/genome.cpp
    src/traits.cpp
    src/evolution.cpp
    src/rng.cpp
    src/simulation.cpp
    src/palette.cpp
    src/sprite_gen.cpp
)
target_include_directories(petbits_nucleo PUBLIC src)

if(MSVC)
    # /utf-8 no es opcional. Los catálogos están llenos de acentos ("Nébula",
    # "Raíz", "Plácido") y sin esta opción MSVC lee los archivos con la
    # codificación regional de Windows: los nombres llegan rotos y algún vector
    # de paridad falla sin motivo aparente.
    target_compile_options(petbits_nucleo PUBLIC /utf-8 /W4)
else()
    target_compile_options(petbits_nucleo PUBLIC -Wall -Wextra -Wpedantic)
endif()

# Los tests de paridad contra el TypeScript.
add_executable(run_tests tests/test_parity.cpp)
target_link_libraries(run_tests PRIVATE petbits_nucleo)

# El ejecutable queda dentro de su carpeta de build —build/debug/ o
# build/release/— y NO en tests/.
#
# Mandarlo a tests/ era más cómodo para correrlo a mano y estaba mal: las dos
# configuraciones escribían el mismo archivo. Compilabas release, después debug,
# y el de release desaparecía; peor todavía, al volver a release Ninja podía ver
# un .exe más nuevo que sus objetos y no relinkear, dejándote corriendo el
# binario de debug creyendo que era el otro. Un test que miente sobre qué está
# probando es peor que no tenerlo.
#
# Visual Studio y ctest lo encuentran solos. El test no lee ningún archivo en
# tiempo de ejecución —los vectores se compilan adentro— así que no le importa
# desde dónde se lo invoque.

# Produce un save para que lo valide el esquema de la web. Ver
# gdext/tests/escribir_save.cpp: es la única prueba de que el nativo escribe
# algo que la web puede leer.
add_executable(escribir_save tests/escribir_save.cpp)
target_link_libraries(escribir_save PRIVATE petbits_nucleo)

enable_testing()
add_test(NAME paridad COMMAND run_tests)
