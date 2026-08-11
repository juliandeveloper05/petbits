/**
 * register_types.cpp — Entry point de la GDExtension de PetBits
 *
 * En Fase 1 se irán registrando aquí todos los nodos C++ a medida que
 * se implementen: PetCore, GenomeResource, TraitDetector, BattleManager, etc.
 */

#include "register_types.h"

// Headers de los nodos Godot (se añadirán en Fase 1 y 2)
// #include "pet_core.h"
// #include "genome_resource.h"
// #include "battle_manager.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_petbits_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

    // Fase 1: registrar nodos C++ aquí
    // ClassDB::register_class<PetCore>();
    // ClassDB::register_class<GenomeResource>();
    // ClassDB::register_class<BattleManager>();
}

void uninitialize_petbits_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

// Punto de entrada requerido por la GDExtension API
extern "C" {
GDExtensionBool GDE_EXPORT petbits_gdextension_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_petbits_module);
    init_obj.register_terminator(uninitialize_petbits_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
