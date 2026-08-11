#pragma once
/**
 * register_types.h — Entry point de la GDExtension
 *
 * Aquí se registran todos los nodos C++ que Godot puede usar desde GDScript.
 */

#include <godot_cpp/core/class_db.hpp>

void initialize_petbits_module(godot::ModuleInitializationLevel p_level);
void uninitialize_petbits_module(godot::ModuleInitializationLevel p_level);
