/**
 * Bulldozer.cpp
 * Construction vehicle implementation with placement validation.
 */

#include "Bulldozer.h"
#include "FloorSnapper.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/prism_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Bulldozer::_bind_methods() {
    // Build commands
    ClassDB::bind_method(D_METHOD("start_placing_building", "type"), &Bulldozer::start_placing_building);
    ClassDB::bind_method(D_METHOD("cancel_placing"), &Bulldozer::cancel_placing);
    ClassDB::bind_method(D_METHOD("confirm_build_location", "location"), &Bulldozer::confirm_build_location);
    ClassDB::bind_method(D_METHOD("update_ghost_position", "position"), &Bulldozer::update_ghost_position);
    
    ClassDB::bind_method(D_METHOD("get_is_constructing"), &Bulldozer::get_is_constructing);
    ClassDB::bind_method(D_METHOD("get_construction_progress"), &Bulldozer::get_construction_progress);
    ClassDB::bind_method(D_METHOD("get_is_placing_building"), &Bulldozer::get_is_placing_building);
    ClassDB::bind_method(D_METHOD("get_placing_type"), &Bulldozer::get_placing_type);
    
    ClassDB::bind_method(D_METHOD("get_power_plant_cost"), &Bulldozer::get_power_plant_cost);
    ClassDB::bind_method(D_METHOD("get_barracks_cost"), &Bulldozer::get_barracks_cost);
    
    ClassDB::bind_method(D_METHOD("set_power_plant_model", "path"), &Bulldozer::set_power_plant_model);
    ClassDB::bind_method(D_METHOD("get_power_plant_model"), &Bulldozer::get_power_plant_model);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "power_plant_model", PROPERTY_HINT_FILE, "*.glb,*.gltf"), "set_power_plant_model", "get_power_plant_model");
    
    ClassDB::bind_method(D_METHOD("set_barracks_model", "path"), &Bulldozer::set_barracks_model);
    ClassDB::bind_method(D_METHOD("get_barracks_model"), &Bulldozer::get_barracks_model);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "barracks_model", PROPERTY_HINT_FILE, "*.glb,*.gltf"), "set_barracks_model", "get_barracks_model");
    
    // Signals
    ADD_SIGNAL(MethodInfo("construction_started", PropertyInfo(Variant::INT, "building_type")));
    ADD_SIGNAL(MethodInfo("construction_completed", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("construction_cancelled"));
    ADD_SIGNAL(MethodInfo("placing_building_started", PropertyInfo(Variant::INT, "building_type")));
    ADD_SIGNAL(MethodInfo("placing_building_cancelled"));
}

Bulldozer::Bulldozer() {
    vehicle_name = "Bulldozer";
    move_speed = 3.0f;  // Slower than regular units
    health = 300;
    max_health = 300;
}

Bulldozer::~Bulldozer() {
}

void Bulldozer::_ready() {
    Vehicle::_ready();
    
    UtilityFunctions::print("Bulldozer: Ready");
}

void Bulldozer::_process(double delta) {
    Vehicle::_process(delta);
    
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Update construction progress
    if (is_constructing) {
        update_construction(delta);
    }
}

void Bulldozer::_physics_process(double delta) {
    // Don't move while constructing
    if (is_constructing) {
        return;
    }
    
    Vehicle::_physics_process(delta);
    
    // Check if we've reached the build location
    if (is_moving && current_build_type != BuildingType::NONE) {
        Vector3 current_pos = get_global_position();
        Vector3 to_target = build_location - current_pos;
        to_target.y = 0;
        
        if (to_target.length() < 2.0f) {
            stop_moving();
            start_construction();
        }
    }
}

void Bulldozer::create_vehicle_mesh() {
    if (!mesh_instance) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
        
        // Bulldozer shape - wide and low
        Ref<BoxMesh> box_mesh;
        box_mesh.instantiate();
        box_mesh->set_size(Vector3(1.8f, 0.7f, 2.5f));
        mesh_instance->set_mesh(box_mesh);
        
        mesh_instance->set_position(Vector3(0, 0.35f, 0));
        
        // Yellow construction vehicle color
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_albedo(Color(0.95f, 0.75f, 0.1f));
        mat->set_roughness(0.5f);
        mat->set_metallic(0.3f);
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    // Add blade mesh at front
    MeshInstance3D *blade = memnew(MeshInstance3D);
    add_child(blade);
    
    Ref<BoxMesh> blade_mesh;
    blade_mesh.instantiate();
    blade_mesh->set_size(Vector3(2.2f, 0.6f, 0.2f));
    blade->set_mesh(blade_mesh);
    blade->set_position(Vector3(0, 0.3f, 1.4f));
    
    Ref<StandardMaterial3D> blade_mat;
    blade_mat.instantiate();
    blade_mat->set_albedo(Color(0.3f, 0.3f, 0.3f));
    blade_mat->set_metallic(0.8f);
    blade->set_surface_override_material(0, blade_mat);
}

void Bulldozer::start_placing_building(int type) {
    if (is_constructing) {
        UtilityFunctions::print("Bulldozer: Cannot place building while constructing");
        return;
    }
    
    placing_type = static_cast<BuildingType>(type + 1);  // +1 because NONE is 0
    is_placing_building = true;
    
    create_ghost_building(placing_type);
    
    emit_signal("placing_building_started", type);
    UtilityFunctions::print("Bulldozer: Started placing building type ", type);
}

void Bulldozer::cancel_placing() {
    if (!is_placing_building) return;
    
    is_placing_building = false;
    placing_type = BuildingType::NONE;
    remove_ghost_building();
    
    emit_signal("placing_building_cancelled");
    UtilityFunctions::print("Bulldozer: Cancelled placing");
}

void Bulldozer::confirm_build_location(const Vector3 &location) {
    if (!is_placing_building) return;
    
    // Check if placement is valid
    if (!check_placement_valid(location, current_ghost_size)) {
        UtilityFunctions::print("Bulldozer: Cannot place building here - location blocked!");
        return;
    }
    
    build_location = location;
    current_build_type = placing_type;
    
    is_placing_building = false;
    placing_type = BuildingType::NONE;
    remove_ghost_building();
    
    // Move bulldozer to the location
    move_to(location);
    
    UtilityFunctions::print("Bulldozer: Moving to build location: ", location);
}

void Bulldozer::update_ghost_position(const Vector3 &position) {
    if (ghost_building) {
        // Snap ghost to terrain height
        Vector3 snapped_pos = position;
        Node *terrain_node = get_tree()->get_root()->find_child("TerrainGenerator", true, false);
        if (terrain_node) {
            Variant height_result = terrain_node->call("get_height_at", position.x, position.z);
            if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
                snapped_pos.y = (float)height_result;
            }
        }
        ghost_building->set_global_position(snapped_pos);
        update_ghost_validity();
    }
}

void Bulldozer::start_construction() {
    if (current_build_type == BuildingType::NONE) return;
    
    is_constructing = true;
    construction_progress = 0.0f;
    
    emit_signal("construction_started", static_cast<int>(current_build_type) - 1);
    UtilityFunctions::print("Bulldozer: Started construction");
}

void Bulldozer::update_construction(double delta) {
    construction_progress += delta / construction_time;
    
    if (construction_progress >= 1.0f) {
        complete_construction();
    }
}

void Bulldozer::complete_construction() {
    Building *building = spawn_building(current_build_type, build_location);
    
    is_constructing = false;
    construction_progress = 0.0f;
    
    BuildingType completed_type = current_build_type;
    current_build_type = BuildingType::NONE;
    
    if (building) {
        emit_signal("construction_completed", building);
        UtilityFunctions::print("Bulldozer: Completed construction of ", building->get_building_name());
    }
    
    // Move away from the building
    Vector3 offset = get_global_position() - build_location;
    offset.y = 0;
    if (offset.length() < 0.1f) {
        offset = Vector3(3, 0, 0);
    } else {
        offset = offset.normalized() * 3.0f;
    }
    move_to(build_location + offset);
}

void Bulldozer::cancel_construction() {
    if (!is_constructing) return;
    
    is_constructing = false;
    construction_progress = 0.0f;
    current_build_type = BuildingType::NONE;
    
    emit_signal("construction_cancelled");
    UtilityFunctions::print("Bulldozer: Construction cancelled");
}

bool Bulldozer::get_is_constructing() const {
    return is_constructing;
}

float Bulldozer::get_construction_progress() const {
    return construction_progress;
}

bool Bulldozer::get_is_placing_building() const {
    return is_placing_building;
}

int Bulldozer::get_placing_type() const {
    return static_cast<int>(placing_type) - 1;  // -1 because NONE is 0
}

int Bulldozer::get_power_plant_cost() const {
    return power_plant_cost;
}

int Bulldozer::get_barracks_cost() const {
    return barracks_cost;
}

void Bulldozer::set_power_plant_model(const String &path) {
    power_plant_model = path;
}

String Bulldozer::get_power_plant_model() const {
    return power_plant_model;
}

void Bulldozer::set_barracks_model(const String &path) {
    barracks_model = path;
}

String Bulldozer::get_barracks_model() const {
    return barracks_model;
}

void Bulldozer::create_ghost_building(BuildingType type) {
    remove_ghost_building();
    
    ghost_building = memnew(Node3D);
    get_tree()->get_root()->add_child(ghost_building);
    
    ghost_mesh = memnew(MeshInstance3D);
    ghost_building->add_child(ghost_mesh);
    
    Ref<BoxMesh> mesh;
    mesh.instantiate();
    
    float size = 4.0f;
    float height = 3.0f;
    
    if (type == BuildingType::POWER_PLANT) {
        size = 8.0f;
        height = 6.0f;
    } else if (type == BuildingType::BARRACKS) {
        size = 6.0f;
        height = 3.0f;
    }
    
    current_ghost_size = size;
    
    mesh->set_size(Vector3(size, height, size));
    ghost_mesh->set_mesh(mesh);
    ghost_mesh->set_position(Vector3(0, height / 2.0f, 0));
    
    // Semi-transparent green material (valid placement)
    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_albedo(Color(0.2f, 0.8f, 0.2f, 0.4f));
    mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
    ghost_mesh->set_surface_override_material(0, mat);
    
    is_placement_valid = true;
}

void Bulldozer::remove_ghost_building() {
    if (ghost_building) {
        ghost_building->queue_free();
        ghost_building = nullptr;
        ghost_mesh = nullptr;
    }
}

Building* Bulldozer::spawn_building(BuildingType type, const Vector3 &position) {
    Building *building = memnew(Building);
    
    if (type == BuildingType::POWER_PLANT) {
        building->set_building_name("Power Plant");
        building->set_building_size(8.0f);
        building->set_building_height(6.0f);
        building->set_max_health(1000);
        building->set_health(1000);
        if (!power_plant_model.is_empty()) {
            building->set_model_path(power_plant_model);
            building->set_model_scale(0.05f);
        }
    } else if (type == BuildingType::BARRACKS) {
        building->set_building_name("Barracks");
        building->set_building_size(6.0f);
        building->set_building_height(3.0f);
        building->set_max_health(1500);
        building->set_health(1500);
        if (!barracks_model.is_empty()) {
            building->set_model_path(barracks_model);
            building->set_model_scale(0.05f);
        }
    }
    
    // Add to scene
    get_tree()->get_root()->add_child(building);
    
    // Try to get terrain height directly from TerrainGenerator
    float terrain_y = position.y;
    Node *terrain_node = get_tree()->get_root()->find_child("TerrainGenerator", true, false);
    if (terrain_node) {
        UtilityFunctions::print("Building: Found TerrainGenerator node");
        // Call get_height_at on the terrain generator
        Variant height_result = terrain_node->call("get_height_at", position.x, position.z);
        UtilityFunctions::print("Building: get_height_at returned type ", height_result.get_type(), " value ", height_result);
        if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
            terrain_y = (float)height_result;
            UtilityFunctions::print("Building: Got terrain height ", terrain_y, " at (", position.x, ", ", position.z, ")");
        }
    } else {
        UtilityFunctions::print("Building: TerrainGenerator node NOT FOUND!");
    }
    
    // Set position on terrain
    Vector3 spawn_pos = position;
    spawn_pos.y = terrain_y;
    building->set_global_position(spawn_pos);
    
    UtilityFunctions::print("Building spawned at Y=", spawn_pos.y);
    
    // Notify the FlowFieldManager that a building was placed
    building->notify_flow_field_of_placement();
    
    return building;
}

void Bulldozer::update_ghost_validity() {
    if (!ghost_building || !ghost_mesh) return;
    
    Vector3 pos = ghost_building->get_global_position();
    bool valid = check_placement_valid(pos, current_ghost_size);
    
    if (valid != is_placement_valid) {
        is_placement_valid = valid;
        
        Ref<StandardMaterial3D> mat = ghost_mesh->get_surface_override_material(0);
        if (mat.is_null()) {
            mat.instantiate();
            ghost_mesh->set_surface_override_material(0, mat);
        }
        
        mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
        
        if (valid) {
            // Green for valid placement
            mat->set_albedo(Color(0.2f, 0.8f, 0.2f, 0.5f));
            mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
            mat->set_emission(Color(0.1f, 0.4f, 0.1f));
            mat->set_emission_energy_multiplier(0.3f);
        } else {
            // Red for invalid placement
            mat->set_albedo(Color(0.8f, 0.2f, 0.2f, 0.5f));
            mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
            mat->set_emission(Color(0.4f, 0.1f, 0.1f));
            mat->set_emission_energy_multiplier(0.6f);
        }
    }
}

bool Bulldozer::check_placement_valid(const Vector3 &position, float size) {
    // Use the static method from Building class for consistency
    uint32_t check_mask = 0b1110; // Units(2), Buildings(4), Vehicles(8)
    return Building::is_position_valid_for_building(this, position, size, check_mask);
}

} // namespace rts
