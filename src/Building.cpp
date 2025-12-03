/**
 * Building.cpp
 * RTS building implementation - static structures.
 */

#include "Building.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Building::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &Building::set_selected);
    ClassDB::bind_method(D_METHOD("get_selected"), &Building::get_selected);
    
    ClassDB::bind_method(D_METHOD("set_hovered", "hovered"), &Building::set_hovered);
    ClassDB::bind_method(D_METHOD("get_hovered"), &Building::get_hovered);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_building_name", "name"), &Building::set_building_name);
    ClassDB::bind_method(D_METHOD("get_building_name"), &Building::get_building_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "building_name"), "set_building_name", "get_building_name");
    
    ClassDB::bind_method(D_METHOD("set_health", "health"), &Building::set_health);
    ClassDB::bind_method(D_METHOD("get_health"), &Building::get_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "health"), "set_health", "get_health");
    
    ClassDB::bind_method(D_METHOD("set_max_health", "max_health"), &Building::set_max_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &Building::get_max_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_health"), "set_max_health", "get_max_health");
    
    ClassDB::bind_method(D_METHOD("set_building_size", "size"), &Building::set_building_size);
    ClassDB::bind_method(D_METHOD("get_building_size"), &Building::get_building_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "building_size", PROPERTY_HINT_RANGE, "1.0,20.0,0.5"), "set_building_size", "get_building_size");
    
    ClassDB::bind_method(D_METHOD("set_building_height", "height"), &Building::set_building_height);
    ClassDB::bind_method(D_METHOD("get_building_height"), &Building::get_building_height);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "building_height", PROPERTY_HINT_RANGE, "1.0,10.0,0.5"), "set_building_height", "get_building_height");
    
    ClassDB::bind_method(D_METHOD("set_building_id", "id"), &Building::set_building_id);
    ClassDB::bind_method(D_METHOD("get_building_id"), &Building::get_building_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "building_id"), "set_building_id", "get_building_id");
    
    ClassDB::bind_method(D_METHOD("set_armor", "armor"), &Building::set_armor);
    ClassDB::bind_method(D_METHOD("get_armor"), &Building::get_armor);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "armor"), "set_armor", "get_armor");
    
    // Signals
    ADD_SIGNAL(MethodInfo("building_selected", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_deselected", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_destroyed", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_hovered", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_unhovered", PropertyInfo(Variant::OBJECT, "building")));
}

Building::Building() {
}

Building::~Building() {
}

void Building::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Set collision layer to 4 (buildings) - separate from units (2) and ground (1)
    set_collision_layer(4);
    // Buildings don't need to detect collisions with anything
    set_collision_mask(0);
    
    // Create the building mesh and collision
    create_building_mesh();
    setup_collision();
}

void Building::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    // Buildings are static, no update needed
}

void Building::create_building_mesh() {
    // Check if mesh already exists as child
    for (int i = 0; i < get_child_count(); i++) {
        mesh_instance = Object::cast_to<MeshInstance3D>(get_child(i));
        if (mesh_instance) break;
    }
    
    // Create mesh if it doesn't exist
    if (!mesh_instance) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
        
        // Create box mesh for building
        Ref<BoxMesh> box_mesh;
        box_mesh.instantiate();
        box_mesh->set_size(Vector3(building_size, building_height, building_size));
        mesh_instance->set_mesh(box_mesh);
        
        // Position mesh so bottom is at ground level
        mesh_instance->set_position(Vector3(0, building_height / 2.0f, 0));
        
        // Create material
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_albedo(Color(0.5f, 0.4f, 0.3f)); // Brown/tan color for building
        mat->set_roughness(0.8f);
        mesh_instance->set_surface_override_material(0, mat);
    }
}

void Building::setup_collision() {
    // Check if collision shape already exists
    for (int i = 0; i < get_child_count(); i++) {
        collision_shape = Object::cast_to<CollisionShape3D>(get_child(i));
        if (collision_shape) break;
    }
    
    // Create collision shape if it doesn't exist
    if (!collision_shape) {
        collision_shape = memnew(CollisionShape3D);
        add_child(collision_shape);
        
        // Create box shape matching the building size
        Ref<BoxShape3D> box_shape;
        box_shape.instantiate();
        box_shape->set_size(Vector3(building_size, building_height, building_size));
        collision_shape->set_shape(box_shape);
        
        // Position collision shape so bottom is at ground level
        collision_shape->set_position(Vector3(0, building_height / 2.0f, 0));
    }
}

void Building::set_selected(bool selected) {
    if (is_selected == selected) {
        return;
    }
    
    is_selected = selected;
    update_selection_visual();
    
    if (is_selected) {
        emit_signal("building_selected", this);
    } else {
        emit_signal("building_deselected", this);
    }
}

bool Building::get_selected() const {
    return is_selected;
}

void Building::set_hovered(bool hovered) {
    if (is_hovered == hovered) {
        return;
    }
    
    is_hovered = hovered;
    update_hover_visual();
    
    if (is_hovered) {
        emit_signal("building_hovered", this);
    } else {
        emit_signal("building_unhovered", this);
    }
}

bool Building::get_hovered() const {
    return is_hovered;
}

void Building::update_selection_visual() {
    if (!mesh_instance) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    if (is_selected) {
        // Selected: bright green with emission
        mat->set_albedo(Color(0.3f, 0.7f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.1f, 0.4f, 0.1f));
        mat->set_emission_energy_multiplier(1.5f);
    } else if (is_hovered) {
        update_hover_visual();
    } else {
        // Default: brown/tan
        mat->set_albedo(Color(0.5f, 0.4f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Building::update_hover_visual() {
    if (!mesh_instance) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    // Don't override selection visual
    if (is_selected) {
        return;
    }
    
    if (is_hovered) {
        // Hovered: light green tint with emission
        mat->set_albedo(Color(0.5f, 0.6f, 0.4f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.2f, 0.5f, 0.2f));
        mat->set_emission_energy_multiplier(0.6f);
    } else {
        // Default: brown/tan
        mat->set_albedo(Color(0.5f, 0.4f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Building::set_building_name(const String &name) {
    building_name = name;
}

String Building::get_building_name() const {
    return building_name;
}

void Building::set_health(int hp) {
    health = hp;
}

int Building::get_health() const {
    return health;
}

void Building::set_max_health(int hp) {
    max_health = hp;
}

int Building::get_max_health() const {
    return max_health;
}

void Building::set_building_size(float size) {
    building_size = size;
}

float Building::get_building_size() const {
    return building_size;
}

void Building::set_building_height(float height) {
    building_height = height;
}

float Building::get_building_height() const {
    return building_height;
}

void Building::set_building_id(int id) {
    building_id = id;
}

int Building::get_building_id() const {
    return building_id;
}

void Building::set_armor(int value) {
    armor = value;
}

int Building::get_armor() const {
    return armor;
}

} // namespace rts
