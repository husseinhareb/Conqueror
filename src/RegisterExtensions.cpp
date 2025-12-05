/**
 * RegisterExtensions.cpp
 * GDExtension entry point - registers all RTS classes with Godot.
 */

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#include "RTSCamera.h"
#include "Unit.h"
#include "Building.h"
#include "CommandCenter.h"
#include "Barracks.h"
#include "Vehicle.h"
#include "Bulldozer.h"
#include "TerrainGenerator.h"
#include "SelectionManager.h"
#include "FlowFieldManager.h"
#include "UnitSpawner.h"
#include "GameManager.h"

using namespace godot;

void initialize_rts_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // Register all RTS classes
    ClassDB::register_class<rts::RTSCamera>();
    ClassDB::register_class<rts::Unit>();
    ClassDB::register_class<rts::Building>();
    ClassDB::register_class<rts::CommandCenter>();
    ClassDB::register_class<rts::Barracks>();
    ClassDB::register_class<rts::Vehicle>();
    ClassDB::register_class<rts::Bulldozer>();
    ClassDB::register_class<rts::TerrainGenerator>();
    ClassDB::register_class<rts::SelectionManager>();
    ClassDB::register_class<rts::FlowFieldManager>();
    ClassDB::register_class<rts::UnitSpawner>();
    ClassDB::register_class<rts::GameManager>();
}

void uninitialize_rts_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Cleanup if needed
}

extern "C" {

GDExtensionBool GDE_EXPORT rts_game_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization
) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_rts_module);
    init_obj.register_terminator(uninitialize_rts_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}

} // extern "C"
