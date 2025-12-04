/**
 * UnitSpawner.cpp
 * Spawns and manages RTS units with MultiMesh optimization.
 */

#include "UnitSpawner.h"
#include "Unit.h"
#include "SelectionManager.h"
#include "FlowFieldManager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void UnitSpawner::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("spawn_unit", "position"), &UnitSpawner::spawn_unit);
    ClassDB::bind_method(D_METHOD("spawn_units_in_formation", "count", "center", "radius"), &UnitSpawner::spawn_units_in_formation);
    ClassDB::bind_method(D_METHOD("despawn_unit", "unit"), &UnitSpawner::despawn_unit);
    ClassDB::bind_method(D_METHOD("despawn_all_units"), &UnitSpawner::despawn_all_units);
    ClassDB::bind_method(D_METHOD("get_unit_count"), &UnitSpawner::get_unit_count);
    
    ClassDB::bind_method(D_METHOD("set_selection_manager", "manager"), &UnitSpawner::set_selection_manager);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "manager"), &UnitSpawner::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_unit_scene", "scene"), &UnitSpawner::set_unit_scene);
    ClassDB::bind_method(D_METHOD("set_unit_mesh", "mesh"), &UnitSpawner::set_unit_mesh);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_max_units", "count"), &UnitSpawner::set_max_units);
    ClassDB::bind_method(D_METHOD("get_max_units"), &UnitSpawner::get_max_units);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_units", PROPERTY_HINT_RANGE, "10,500,10"), "set_max_units", "get_max_units");
    
    ClassDB::bind_method(D_METHOD("set_auto_spawn", "enabled"), &UnitSpawner::set_auto_spawn);
    ClassDB::bind_method(D_METHOD("get_auto_spawn"), &UnitSpawner::get_auto_spawn);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_spawn"), "set_auto_spawn", "get_auto_spawn");
    
    ClassDB::bind_method(D_METHOD("set_auto_spawn_count", "count"), &UnitSpawner::set_auto_spawn_count);
    ClassDB::bind_method(D_METHOD("get_auto_spawn_count"), &UnitSpawner::get_auto_spawn_count);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "auto_spawn_count", PROPERTY_HINT_RANGE, "1,100,1"), "set_auto_spawn_count", "get_auto_spawn_count");
}

UnitSpawner::UnitSpawner() {
}

UnitSpawner::~UnitSpawner() {
}

void UnitSpawner::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    spawn_center = get_global_position();
    
    // Setup MultiMesh for optimized rendering
    if (use_multi_mesh) {
        setup_multi_mesh();
    }
    
    // Auto-spawn units if enabled
    if (auto_spawn) {
        spawn_units_in_formation(auto_spawn_count, spawn_center, spawn_radius);
    }
}

void UnitSpawner::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Update flow vectors for all units
    update_units_flow_vectors();
    
    // Sync unit transforms to MultiMesh
    if (use_multi_mesh && multi_mesh.is_valid()) {
        update_multi_mesh_transforms();
    }
}

Unit* UnitSpawner::spawn_unit(const Vector3 &position) {
    if (units.size() >= max_units) {
        UtilityFunctions::print("UnitSpawner: Max units reached");
        return nullptr;
    }
    
    // Validate spawn position against terrain
    Vector3 spawn_pos = position;
    Node *terrain_node = get_tree()->get_root()->find_child("TerrainGenerator", true, false);
    if (terrain_node) {
        // Check if within bounds
        Variant bounds_result = terrain_node->call("is_within_bounds", position.x, position.z);
        if (bounds_result.get_type() == Variant::BOOL && !(bool)bounds_result) {
            UtilityFunctions::print("UnitSpawner: Cannot spawn unit outside terrain bounds");
            return nullptr;
        }
        
        // Check if not in water
        Variant water_result = terrain_node->call("is_water_at", position.x, position.z);
        if (water_result.get_type() == Variant::BOOL && (bool)water_result) {
            UtilityFunctions::print("UnitSpawner: Cannot spawn unit in water");
            return nullptr;
        }
        
        // Get terrain height
        Variant height_result = terrain_node->call("get_height_at", position.x, position.z);
        if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
            spawn_pos.y = (float)height_result;
        }
    }
    
    // Check for collisions at spawn location
    if (!is_spawn_location_valid(spawn_pos)) {
        // Try to find a nearby valid position
        spawn_pos = find_valid_spawn_location(spawn_pos, 5.0f);
        if (spawn_pos.y < -1000.0f) {
            UtilityFunctions::print("UnitSpawner: No valid spawn location found");
            return nullptr;
        }
    }
    
    Unit *unit = memnew(Unit);
    unit->set_unit_id(next_unit_id++);
    
    // Create collision shape
    CollisionShape3D *collision = memnew(CollisionShape3D);
    Ref<CapsuleShape3D> capsule_shape;
    capsule_shape.instantiate();
    capsule_shape->set_radius(0.4f);
    capsule_shape->set_height(1.0f);
    collision->set_shape(capsule_shape);
    unit->add_child(collision);
    
    // Create mesh visual
    MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
    Ref<CapsuleMesh> capsule_mesh;
    capsule_mesh.instantiate();
    capsule_mesh->set_radius(0.4f);
    capsule_mesh->set_height(1.0f);
    mesh_instance->set_mesh(capsule_mesh);
    
    // Set default material
    Ref<StandardMaterial3D> material;
    material.instantiate();
    material->set_albedo(Color(0.3f, 0.3f, 0.8f));
    mesh_instance->set_surface_override_material(0, material);
    
    unit->add_child(mesh_instance);
    
    // Set position and add to scene
    add_child(unit);
    unit->set_global_position(spawn_pos);
    
    // Set collision layer for unit (layer 2)
    // Collision mask: Ground(1), Units(2), Buildings(4), Vehicles(8)
    // This ensures units physically cannot pass through buildings or each other
    unit->set_collision_layer(2);
    unit->set_collision_mask(1 | 2 | 4 | 8);
    
    // Register with selection manager
    if (selection_manager) {
        selection_manager->register_unit(unit);
    }
    
    units.push_back(unit);
    
    // Update MultiMesh instance count
    if (use_multi_mesh && multi_mesh.is_valid()) {
        multi_mesh->set_instance_count(units.size());
    }
    
    return unit;
}

void UnitSpawner::spawn_units_in_formation(int count, const Vector3 &center, float radius) {
    for (int i = 0; i < count; i++) {
        // Spiral formation
        float angle = i * 2.4f; // Golden angle
        float r = radius * Math::sqrt(static_cast<float>(i) / count);
        
        Vector3 pos = center;
        pos.x += r * Math::cos(angle);
        pos.z += r * Math::sin(angle);
        pos.y = spawn_height;
        
        spawn_unit(pos);
    }
    
    UtilityFunctions::print("UnitSpawner: Spawned ", count, " units");
}

void UnitSpawner::spawn_units_grid(int count, const Vector3 &center, float spacing) {
    int side = static_cast<int>(Math::ceil(Math::sqrt(static_cast<float>(count))));
    int spawned = 0;
    
    for (int x = 0; x < side && spawned < count; x++) {
        for (int z = 0; z < side && spawned < count; z++) {
            Vector3 pos = center;
            pos.x += (x - side / 2) * spacing;
            pos.z += (z - side / 2) * spacing;
            pos.y = spawn_height;
            
            spawn_unit(pos);
            spawned++;
        }
    }
}

void UnitSpawner::despawn_unit(Unit *unit) {
    if (!unit) return;
    
    int idx = units.find(unit);
    if (idx != -1) {
        units.remove_at(idx);
        
        if (selection_manager) {
            selection_manager->unregister_unit(unit);
        }
        
        unit->queue_free();
        
        // Update MultiMesh
        if (use_multi_mesh && multi_mesh.is_valid()) {
            multi_mesh->set_instance_count(units.size());
        }
    }
}

void UnitSpawner::despawn_all_units() {
    for (int i = units.size() - 1; i >= 0; i--) {
        despawn_unit(units[i]);
    }
    units.clear();
}

void UnitSpawner::setup_multi_mesh() {
    multi_mesh_instance = memnew(MultiMeshInstance3D);
    add_child(multi_mesh_instance);
    
    multi_mesh.instantiate();
    multi_mesh->set_transform_format(MultiMesh::TRANSFORM_3D);
    
    // Create mesh for MultiMesh (same as unit mesh)
    Ref<CapsuleMesh> mesh;
    mesh.instantiate();
    mesh->set_radius(0.4f);
    mesh->set_height(1.0f);
    multi_mesh->set_mesh(mesh);
    
    multi_mesh_instance->set_multimesh(multi_mesh);
    
    // Hide MultiMesh initially (we use individual meshes for selection feedback)
    multi_mesh_instance->set_visible(false);
}

void UnitSpawner::update_multi_mesh_transforms() {
    if (!multi_mesh.is_valid()) return;
    
    for (int i = 0; i < units.size(); i++) {
        sync_unit_to_multi_mesh(i);
    }
}

void UnitSpawner::sync_unit_to_multi_mesh(int index) {
    if (index < 0 || index >= units.size()) return;
    if (!multi_mesh.is_valid()) return;
    
    Unit *unit = units[index];
    if (!unit) return;
    
    Transform3D transform = unit->get_global_transform();
    multi_mesh->set_instance_transform(index, transform);
}

Vector<Unit*> UnitSpawner::get_all_units() const {
    return units;
}

Unit* UnitSpawner::get_unit_by_id(int id) const {
    for (int i = 0; i < units.size(); i++) {
        if (units[i] && units[i]->get_unit_id() == id) {
            return units[i];
        }
    }
    return nullptr;
}

int UnitSpawner::get_unit_count() const {
    return units.size();
}

void UnitSpawner::update_units_flow_vectors() {
    if (!flow_field_manager || !flow_field_manager->is_field_valid()) {
        return;
    }
    
    for (int i = 0; i < units.size(); i++) {
        Unit *unit = units[i];
        if (unit && unit->is_moving()) {
            Vector3 flow = flow_field_manager->get_flow_direction(unit->get_global_position());
            unit->apply_flow_vector(flow);
        }
    }
}

void UnitSpawner::set_selection_manager(SelectionManager *manager) {
    selection_manager = manager;
}

void UnitSpawner::set_flow_field_manager(FlowFieldManager *manager) {
    flow_field_manager = manager;
}

void UnitSpawner::set_unit_scene(const Ref<PackedScene> &scene) {
    unit_scene = scene;
}

void UnitSpawner::set_unit_mesh(const Ref<Mesh> &mesh) {
    unit_mesh = mesh;
}

void UnitSpawner::set_max_units(int count) {
    max_units = count;
}

int UnitSpawner::get_max_units() const {
    return max_units;
}

void UnitSpawner::set_auto_spawn(bool enabled) {
    auto_spawn = enabled;
}

bool UnitSpawner::get_auto_spawn() const {
    return auto_spawn;
}

void UnitSpawner::set_auto_spawn_count(int count) {
    auto_spawn_count = count;
}

int UnitSpawner::get_auto_spawn_count() const {
    return auto_spawn_count;
}

bool UnitSpawner::is_spawn_location_valid(const Vector3 &position) {
    // Check for collisions with existing objects
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return true; // Assume valid if no world
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return true;
    
    // Use a sphere shape to check for overlapping objects
    Ref<SphereShape3D> check_shape;
    check_shape.instantiate();
    check_shape->set_radius(0.5f); // Slightly larger than unit radius
    
    Ref<PhysicsShapeQueryParameters3D> shape_query;
    shape_query.instantiate();
    shape_query->set_shape(check_shape);
    shape_query->set_transform(Transform3D(Basis(), position + Vector3(0, 0.5f, 0)));
    shape_query->set_collision_mask(2 | 4 | 8); // Units, Buildings, Vehicles
    
    TypedArray<Dictionary> results = space_state->intersect_shape(shape_query, 1);
    
    return results.size() == 0;
}

Vector3 UnitSpawner::find_valid_spawn_location(const Vector3 &center, float search_radius) {
    // Try to find a valid spawn location in a spiral pattern around the center
    const int max_attempts = 16;
    const float angle_step = Math_PI * 2.0f / 8.0f;
    
    for (float radius = 1.0f; radius <= search_radius; radius += 1.0f) {
        for (int i = 0; i < 8; i++) {
            float angle = i * angle_step;
            Vector3 test_pos = center;
            test_pos.x += radius * Math::cos(angle);
            test_pos.z += radius * Math::sin(angle);
            
            // Get terrain height at test position
            Node *terrain_node = get_tree()->get_root()->find_child("TerrainGenerator", true, false);
            if (terrain_node) {
                Variant bounds_result = terrain_node->call("is_within_bounds", test_pos.x, test_pos.z);
                if (bounds_result.get_type() == Variant::BOOL && !(bool)bounds_result) {
                    continue; // Outside bounds
                }
                
                Variant water_result = terrain_node->call("is_water_at", test_pos.x, test_pos.z);
                if (water_result.get_type() == Variant::BOOL && (bool)water_result) {
                    continue; // In water
                }
                
                Variant height_result = terrain_node->call("get_height_at", test_pos.x, test_pos.z);
                if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
                    test_pos.y = (float)height_result;
                }
            }
            
            if (is_spawn_location_valid(test_pos)) {
                return test_pos;
            }
        }
    }
    
    // Return invalid position marker
    return Vector3(0, -9999.0f, 0);
}

} // namespace rts
