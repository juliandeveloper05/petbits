// battle.cpp — por ahora, solamente el chequeo de que battle.h compila.
//
// La Fase 4 (combate) no empezó: las siete funciones que declara `battle.h` no
// tienen implementación. Este archivo existe igual, y está en la lista de
// fuentes de CMake, por un motivo chico y concreto: un header que nadie incluye
// no lo mira nadie.
//
// `battle.h` estuvo roto sin que se notara — usaba `CreatureState` sin incluir
// `simulation.h` — justamente porque ningún .cpp lo tocaba. Incluyéndolo desde
// acá, el compilador lo revisa en cada build y el error aparece el día que se
// comete, no el día que alguien va a usarlo.
//
// Cuando la Fase 4 arranque, las definiciones van acá y este comentario se va.

#include "battle.h"
