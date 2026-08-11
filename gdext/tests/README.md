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
cl /std:c++17 /EHsc /utf-8 /O2 /Fe:run_tests.exe test_parity.cpp ..\src\genome.cpp ..\src\traits.cpp ..\src\evolution.cpp && run_tests.exe
```

`/utf-8` no es adorno: los catálogos están llenos de acentos y sin esa opción
MSVC lee los archivos con la codificación regional de Windows. Los nombres
llegan con la acentuación rota y algún vector de `parseSeed` falla sin motivo
aparente.

### Windows — MinGW, Linux o macOS

```bash
g++ -std=c++17 -O2 test_parity.cpp ../src/genome.cpp ../src/traits.cpp ../src/evolution.cpp -o run_tests && ./run_tests
```

Salida esperada:

```
PetBits — paridad TypeScript <-> C++
2010 genomas, 80 crianzas, 7 hashes

decodeGenome / formatSeed
hashString (FNV-1a 64)
detectTraits / rarityTier
resolverJuvenil / resolverAdulto

38200 comprobaciones, 0 fallas
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
| `genome.cpp` | `hashString` (FNV-1a 64) | 7 textos |
| `traits.cpp` | Las 8 rarezas y el tier agregado | 2010 genomas |
| `evolution.cpp` | `resolverJuvenil` y `resolverAdulto` | 10 crianzas × 8 afinidades |

Los 2010 genomas son 2000 de `splitmix64` con semilla fija más 10 elegidos a
mano. Los de a mano importan más de lo que parece: entre dos mil seeds al azar
no sale el cero, ni el máximo, ni un pangrama, ni el primo más grande que entra
en 64 bits. Son exactamente los valores donde un port se rompe.

## Qué falta

`simulation.cpp` y `breeding.cpp` todavía no están portados —hay header pero no
implementación—, así que no hay vectores para ellos. Cuando se porten, se
agregan al generador y al test.

---

## Cuándo hay que volver a correr esto

Siempre que se toque `src/core/genome.ts`, `traits.ts` o `evolution.ts`. El
orden es: cambiar el TS → `npm run parity` → mirar el diff del header → ajustar
el C++ hasta que el test vuelva a dar cero fallas.

Si el diff del header sale más grande de lo esperado, ahí hay algo para mirar:
un cambio chico en el TS que mueve miles de vectores casi siempre significa que
movió algo que no se quería mover.
