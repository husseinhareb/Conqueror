/**
 * Vehicle.cpp
 * RTS vehicle implementation - movable units with special actions.
 */

#include "Vehicle.h"
#include "FloorSnapper.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Vehicle::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &Vehicle::set_selected);
    ClassDB::bind_method(D_METHOD("get_selected"), &Vehicle::get_selected);
    
    ClassDB::bind_method(D_METHOD("set_hovered", "hovered"), &Vehicle::set_hovered);
    ClassDB::bind_method(D_METHOD("get_hovered"), &Vehicle::get_hovered);
    
    ClassDB::bind_method(D_METHOD("move_to", "position"), &Vehicle::move_to);
    ClassDB::bind_method(D_METHOD("stop_moving"), &Vehicle::stop_moving);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_vehicle_name", "name"), &Vehicle::set_vehicle_name);
    ClassDB::bind_method(D_METHOD("get_vehicle_name"), &Vehicle::get_vehicle_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "vehicle_name"), "set_vehicle_name", "get_vehicle_name");
    
    ClassDB::bind_method(D_METHOD("set_health", "health"), &Vehicle::set_health);
    ClassDB::bind_method(D_METHOD("get_health"), &Vehicle::get_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "health"), "set_health", "get_health");
    
    ClassDB::bind_method(D_METHOD("set_max_health", "max_health"), &Vehicle::set_max_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &Vehicle::get_max_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_health"), "set_max_health", "get_max_health");
    
    ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &Vehicle::set_move_speed);
    ClassDB::bind_method(D_METHOD("get_move_speed"), &Vehicle::get_move_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed", PROPERTY_HINT_RANGE, "1.0,20.0,0.5"), "set_move_speed", "get_move_speed");
    
    ClassDB::bind_method(D_METHOD("set_vehicle_id", "id"), &Vehicle::set_vehicle_id);
    ClassDB::bind_method(D_METHOD("get_vehicle_id"), &Vehicle::get_vehicle_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "vehicle_id"), "set_vehicle_id", "get_vehicle_id");
    
    ClassDB::bind_method(D_METHOD("set_model_path", "path"), &Vehicle::set_model_path);
    ClassDB::bind_method(D_METHOD("get_model_path"), &Vehicle::get_model_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "model_path", PROPERTY_HINT_FILE, "*.glb,*.gltf"), "set_model_path", "get_model_path");
    
    ClassDB::bind_method(D_METHOD("set_model_scale", "scale"), &Vehicle::set_model_scale);
    ClassDB::bind_method(D_METHOD("get_model_scale"), &Vehicle::get_model_scale);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "model_scale", PROPERTY_HINT_RANGE, "0.01,10.0,0.01"), "set_model_scale", "get_model_scale");
    
    // Signals
    ADD_SIGNAL(MethodInfo("vehicle_selected", PropertyInfo(Variant::OBJECT, "vehicle")));
    ADD_SIGNAL(MethodInfo("vehicle_deselected", PropertyInfo(Variant::OBJECT, "vehicle")));
}

Vehicle::Vehicle() {
}

Vehicle::~Vehicle() {
}

void Vehicle::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Set collision layer to 8 (vehicles)
    // Collision layers: 1=Ground, 2=Units, 4=Buildings, 8=Vehicles
    set_collision_layer(8);
    // Only collide with ground (1), not with units/buildings/other vehicles
    set_collision_mask(1);
    
    target_position = get_global_position();
    
    if (!model_path.is_empty()) {
        load_model();
    } else {
        create_vehicle_mesh();
    }
    
    setup_collision();
    
    // Snap to floor after all setup is complete
    FloorSnapConfig config;
    config.floor_collision_mask = 1; // Ground layer
    config.raycast_start_height = 10.0f;
    config.raycast_max_distance = 50.0f;
    config.ground_offset = 0.0f;
    
    FloorSnapResult result = FloorSnapper::snap_to_floor(this, config);
    if (result.success) {
        UtilityFunctions::print("Vehicle snapped to floor at Y=", result.final_y);
    }
}

void Vehicle::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
}

void Vehicle::_physics_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    if (!is_moving) {
        return;
    }
    
    Vector3 current_pos = get_global_position();
    Vector3 direction = target_position - current_pos;
    direction.y = 0; // Keep on ground plane
    
    float distance = direction.length();
    
    if (distance < 0.5f) {
        is_moving = false;
        current_velocity = Vector3();
        return;
    }
    
    direction = direction.normalized();
    
    // Rotate towards target
    float target_angle = atan2(direction.x, direction.z);
    float current_angle = get_rotation().y;
    float angle_diff = target_angle - current_angle;
    
    // Normalize angle
    while (angle_diff > Math_PI) angle_diff -= Math_PI * 2;
    while (angle_diff < -Math_PI) angle_diff += Math_PI * 2;
    
    float rotation_step = turn_speed * delta;
    if (abs(angle_diff) < rotation_step) {
        set_rotation(Vector3(0, target_angle, 0));
    } else {
        set_rotation(Vector3(0, current_angle + (angle_diff > 0 ? rotation_step : -rotation_step), 0));
    }
    
    // Move forward
    current_velocity = direction * move_speed;
    set_velocity(current_velocity);
    move_and_slide();
}

void Vehicle::create_vehicle_mesh() {
    if (!mesh_instance) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
        
        // Create box mesh for vehicle (wider than tall)
        Ref<BoxMesh> box_mesh;
        box_mesh.instantiate();
        box_mesh->set_size(Vector3(1.5f, 0.8f, 2.0f));
        mesh_instance->set_mesh(box_mesh);
        
        mesh_instance->set_position(Vector3(0, 0.4f, 0));
        
        // Yellow color for construction vehicle
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_albedo(Color(0.9f, 0.7f, 0.1f));
        mat->set_roughness(0.6f);
        mesh_instance->set_surface_override_material(0, mat);
    }
}

void Vehicle::load_model() {
    if (model_path.is_empty()) {
        create_vehicle_mesh();
        return;
    }
    
    ResourceLoader *loader = ResourceLoader::get_singleton();
    
    if (!loader->exists(model_path)) {
        UtilityFunctions::print("Vehicle: Model file does not exist: ", model_path);
        create_vehicle_mesh();
        return;
    }
    
    Ref<PackedScene> scene = loader->load(model_path);
    
    if (scene.is_null()) {
        UtilityFunctions::print("Vehicle: Failed to load model: ", model_path);
        create_vehicle_mesh();
        return;
    }
    
    model_instance = Object::cast_to<Node3D>(scene->instantiate());
    if (!model_instance) {
        create_vehicle_mesh();
        return;
    }
    
    add_child(model_instance);
    model_instance->set_scale(Vector3(model_scale, model_scale, model_scale));
    
    // Model is positioned at origin - FloorSnapper will handle ground positioning
    model_instance->set_position(Vector3(0, 0, 0));
    
    // Rotate model 180 degrees so front faces forward (Z+ direction)
    model_instance->set_rotation(Vector3(0, Math_PI, 0));
    
    find_mesh_instances_recursive(model_instance);
    
    UtilityFunctions::print("Vehicle: Loaded model from ", model_path);
}

void Vehicle::find_mesh_instances_recursive(Node *node) {
    if (!node) return;
    
    MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
    if (mesh && !mesh_instance) {
        mesh_instance = mesh;
    }
    
    for (int i = 0; i < node->get_child_count(); i++) {
        find_mesh_instances_recursive(node->get_child(i));
    }
}

void Vehicle::setup_collision() {
    if (!collision_shape) {
        collision_shape = memnew(CollisionShape3D);
        add_child(collision_shape);
        
        Ref<BoxShape3D> box_shape;
        box_shape.instantiate();
        // Scale collision with model
        float col_scale = model_scale > 0 ? model_scale : 1.0f;
        box_shape->set_size(Vector3(1.5f * col_scale, 1.0f * col_scale, 2.0f * col_scale));
        collision_shape->set_shape(box_shape);
        // Position collision shape so bottom is at Y=0
        collision_shape->set_position(Vector3(0, 0.5f * col_scale, 0));
    }
}

void Vehicle::move_to(const Vector3 &position) {
    target_position = position;
    target_position.y = get_global_position().y;
    is_moving = true;
}

void Vehicle::stop_moving() {
    is_moving = false;
    current_velocity = Vector3();
}

bool Vehicle::get_is_moving() const {
    return is_moving;
}

void Vehicle::set_selected(bool selected) {
    if (is_selected == selected) return;
    
    is_selected = selected;
    update_selection_visual();
    
    if (is_selected) {
        emit_signal("vehicle_selected", this);
    } else {
        emit_signal("vehicle_deselected", this);
    }
}

bool Vehicle::get_selected() const {
    return is_selected;
}

void Vehicle::set_hovered(bool hovered) {
    if (is_hovered == hovered) return;
    
    is_hovered = hovered;
    update_hover_visual();
}

bool Vehicle::get_hovered() const {
    return is_hovered;
}

void Vehicle::update_selection_visual() {
    if (model_instance) {
        apply_visual_to_model(model_instance, is_selected, is_hovered);
        return;
    }
    
    if (!mesh_instance) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    if (is_selected) {
        mat->set_albedo(Color(0.9f, 0.8f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.2f, 0.5f, 0.2f));
        mat->set_emission_energy_multiplier(1.5f);
    } else if (is_hovered) {
        update_hover_visual();
    } else {
        mat->set_albedo(Color(0.9f, 0.7f, 0.1f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Vehicle::update_hover_visual() {
    if (model_instance) {
        apply_visual_to_model(model_instance, is_selected, is_hovered);
        return;
    }
    
    if (!mesh_instance) return;
    if (is_selected) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    if (is_hovered) {
        mat->set_albedo(Color(0.95f, 0.85f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.2f, 0.4f, 0.2f));
        mat->set_emission_energy_multiplier(0.5f);
    } else {
        mat->set_albedo(Color(0.9f, 0.7f, 0.1f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Vehicle::apply_visual_to_model(Node *node, bool selected, bool hovered) {
    if (!node) return;
    
    MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
    if (mesh) {
        Ref<Mesh> mesh_res = mesh->get_mesh();
        if (mesh_res.is_valid()) {
            int surface_count = mesh_res->get_surface_count();
            for (int s = 0; s < surface_count; s++) {
                Ref<StandardMaterial3D> mat = mesh->get_surface_override_material(s);
                if (mat.is_null()) {
                    Ref<Material> original_mat = mesh_res->surface_get_material(s);
                    Ref<StandardMaterial3D> original_std = original_mat;
                    
                    if (original_std.is_valid()) {
                        mat = original_std->duplicate();
                    } else {
                        mat.instantiate();
                    }
                    mesh->set_surface_override_material(s, mat);
                }
                
                if (selected) {
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                    mat->set_emission(Color(0.1f, 0.5f, 0.1f));
                    mat->set_emission_energy_multiplier(1.5f);
                } else if (hovered) {
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                    mat->set_emission(Color(0.2f, 0.5f, 0.2f));
                    mat->set_emission_energy_multiplier(0.6f);
                } else {
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
                }
            }
        }
    }
    
    for (int i = 0; i < node->get_child_count(); i++) {
        apply_visual_to_model(node->get_child(i), selected, hovered);
    }
}

void Vehicle::set_vehicle_name(const String &name) {
    vehicle_name = name;
}

String Vehicle::get_vehicle_name() const {
    return vehicle_name;
}

void Vehicle::set_health(int hp) {
    health = hp;
}

int Vehicle::get_health() const {
    return health;
}

void Vehicle::set_max_health(int hp) {
    max_health = hp;
}

int Vehicle::get_max_health() const {
    return max_health;
}

void Vehicle::set_move_speed(float speed) {
    move_speed = speed;
}

float Vehicle::get_move_speed() const {
    return move_speed;
}

void Vehicle::set_vehicle_id(int id) {
    vehicle_id = id;
}

int Vehicle::get_vehicle_id() const {
    return vehicle_id;
}

void Vehicle::set_model_path(const String &path) {
    model_path = path;
}

String Vehicle::get_model_path() const {
    return model_path;
}

void Vehicle::set_model_scale(float scale) {
    model_scale = scale;
}

float Vehicle::get_model_scale() const {
    return model_scale;
}

} // namespace rts
