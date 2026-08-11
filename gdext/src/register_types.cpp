/**
 * register_types.cpp — Entry point de la GDExtension de PetBits
 *
 * Acá se registran las clases C++ que GDScript puede instanciar. Van sumándose
 * a medida que se portan módulos: PetCore con la simulación, BattleManager con
 * el combate.
 */

#include "register_types.h"

#include "petbits_core.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_petbits_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

    GDREGISTER_CLASS(PetBitsCore);
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
