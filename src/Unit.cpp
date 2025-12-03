/**
 * Unit.cpp
 * RTS unit implementation with movement, selection, and flow field integration.
 */

#include "Unit.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Unit::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("set_move_target", "target"), &Unit::set_move_target);
    ClassDB::bind_method(D_METHOD("apply_flow_vector", "vector"), &Unit::apply_flow_vector);
    ClassDB::bind_method(D_METHOD("stop_movement"), &Unit::stop_movement);
    
    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &Unit::set_selected);
    ClassDB::bind_method(D_METHOD("get_selected"), &Unit::get_selected);
    
    ClassDB::bind_method(D_METHOD("is_moving"), &Unit::is_moving);
    ClassDB::bind_method(D_METHOD("get_target_position"), &Unit::get_target_position);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &Unit::set_move_speed);
    ClassDB::bind_method(D_METHOD("get_move_speed"), &Unit::get_move_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed", PROPERTY_HINT_RANGE, "1.0,20.0,0.5"), "set_move_speed", "get_move_speed");
    
    ClassDB::bind_method(D_METHOD("set_unit_id", "id"), &Unit::set_unit_id);
    ClassDB::bind_method(D_METHOD("get_unit_id"), &Unit::get_unit_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "unit_id"), "set_unit_id", "get_unit_id");
    
    // Signals
    ADD_SIGNAL(MethodInfo("unit_selected", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("unit_deselected", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("unit_arrived", PropertyInfo(Variant::OBJECT, "unit")));
}

Unit::Unit() {
}

Unit::~Unit() {
}

void Unit::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    target_position = get_global_position();
    current_velocity = Vector3(0, 0, 0);
    flow_vector = Vector3(0, 0, 0);
}

void Unit::_physics_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    update_movement(delta);
}

void Unit::set_move_target(const Vector3 &target) {
    target_position = target;
    target_position.y = get_global_position().y; // Keep same height
    has_move_order = true;
}

void Unit::apply_flow_vector(const Vector3 &vector) {
    flow_vector = vector;
}

void Unit::stop_movement() {
    has_move_order = false;
    flow_vector = Vector3(0, 0, 0);
    current_velocity = Vector3(0, 0, 0);
}

void Unit::update_movement(double delta) {
    if (!has_move_order) {
        // Decelerate to stop
        if (current_velocity.length_squared() > 0.01f) {
            current_velocity = current_velocity.move_toward(Vector3(0, 0, 0), deceleration * delta);
            set_velocity(current_velocity);
            move_and_slide();
        }
        return;
    }
    
    Vector3 current_pos = get_global_position();
    Vector3 to_target = target_position - current_pos;
    to_target.y = 0;
    
    float distance = to_target.length();
    
    // Check if arrived
    if (distance < arrival_threshold) {
        has_move_order = false;
        current_velocity = Vector3(0, 0, 0);
        emit_signal("unit_arrived", this);
        return;
    }
    
    // Calculate desired velocity
    Vector3 desired_velocity;
    
    if (use_flow_field && flow_vector.length_squared() > 0.01f) {
        // Use flow field direction
        desired_velocity = flow_vector.normalized() * move_speed;
    } else {
        // Direct path to target
        desired_velocity = to_target.normalized() * move_speed;
    }
    
    // Apply steering
    Vector3 steering = calculate_steering(desired_velocity);
    current_velocity += steering * delta;
    
    // Clamp to max speed
    if (current_velocity.length() > move_speed) {
        current_velocity = current_velocity.normalized() * move_speed;
    }
    
    // Apply movement
    set_velocity(current_velocity);
    move_and_slide();
    
    // Rotate to face movement direction
    if (current_velocity.length_squared() > 0.1f) {
        Vector3 look_dir = current_velocity.normalized();
        float target_angle = Math::atan2(look_dir.x, look_dir.z);
        Vector3 rotation = get_rotation();
        rotation.y = Math::lerp_angle(rotation.y, target_angle, static_cast<float>(steering_strength * delta));
        set_rotation(rotation);
    }
}

Vector3 Unit::calculate_steering(const Vector3 &desired_velocity) const {
    Vector3 steering = desired_velocity - current_velocity;
    
    // Limit steering force
    float max_steering = acceleration;
    if (steering.length() > max_steering) {
        steering = steering.normalized() * max_steering;
    }
    
    return steering;
}

void Unit::set_selected(bool selected) {
    if (is_selected == selected) {
        return;
    }
    
    is_selected = selected;
    update_selection_visual();
    
    if (is_selected) {
        emit_signal("unit_selected", this);
    } else {
        emit_signal("unit_deselected", this);
    }
}

bool Unit::get_selected() const {
    return is_selected;
}

void Unit::update_selection_visual() {
    // Find mesh instance child and update material
    for (int i = 0; i < get_child_count(); i++) {
        MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(get_child(i));
        if (mesh) {
            Ref<StandardMaterial3D> mat = mesh->get_surface_override_material(0);
            if (mat.is_null()) {
                mat.instantiate();
                mesh->set_surface_override_material(0, mat);
            }
            
            if (is_selected) {
                mat->set_albedo(Color(0.2f, 0.8f, 0.2f)); // Green when selected
                mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                mat->set_emission(Color(0.1f, 0.4f, 0.1f));
            } else {
                mat->set_albedo(Color(0.3f, 0.3f, 0.8f)); // Blue when not selected
                mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
            }
        }
    }
}

void Unit::set_move_speed(float speed) {
    move_speed = speed;
}

float Unit::get_move_speed() const {
    return move_speed;
}

void Unit::set_unit_id(int id) {
    unit_id = id;
}

int Unit::get_unit_id() const {
    return unit_id;
}

bool Unit::is_moving() const {
    return has_move_order || current_velocity.length_squared() > 0.1f;
}

Vector3 Unit::get_target_position() const {
    return target_position;
}

} // namespace rts
