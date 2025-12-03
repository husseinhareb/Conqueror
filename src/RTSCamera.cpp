/**
 * RTSCamera.cpp
 * RTS-style camera controller implementation.
 */

#include "RTSCamera.h"

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void RTSCamera::_bind_methods() {
    // Properties
    ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &RTSCamera::set_move_speed);
    ClassDB::bind_method(D_METHOD("get_move_speed"), &RTSCamera::get_move_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed", PROPERTY_HINT_RANGE, "1.0,100.0,0.5"), "set_move_speed", "get_move_speed");

    ClassDB::bind_method(D_METHOD("set_zoom_speed", "speed"), &RTSCamera::set_zoom_speed);
    ClassDB::bind_method(D_METHOD("get_zoom_speed"), &RTSCamera::get_zoom_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "zoom_speed", PROPERTY_HINT_RANGE, "0.5,10.0,0.1"), "set_zoom_speed", "get_zoom_speed");

    ClassDB::bind_method(D_METHOD("set_min_zoom", "zoom"), &RTSCamera::set_min_zoom);
    ClassDB::bind_method(D_METHOD("get_min_zoom"), &RTSCamera::get_min_zoom);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_zoom", PROPERTY_HINT_RANGE, "5.0,30.0,1.0"), "set_min_zoom", "get_min_zoom");

    ClassDB::bind_method(D_METHOD("set_max_zoom", "zoom"), &RTSCamera::set_max_zoom);
    ClassDB::bind_method(D_METHOD("get_max_zoom"), &RTSCamera::get_max_zoom);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_zoom", PROPERTY_HINT_RANGE, "30.0,100.0,1.0"), "set_max_zoom", "get_max_zoom");
}

RTSCamera::RTSCamera() {
}

RTSCamera::~RTSCamera() {
}

void RTSCamera::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Make this the current camera
    set_current(true);
    
    // Initialize target position at origin (ground level)
    target_position = Vector3(0, 0, 0);
    current_zoom = 30.0f; // Start at a good viewing distance
    
    update_camera_transform();
}

void RTSCamera::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    handle_keyboard_movement(delta);
    handle_edge_scroll(delta);
    
    update_camera_transform();
}

void RTSCamera::_input(const Ref<InputEvent> &event) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Mouse button events (zoom, rotation toggle)
    Ref<InputEventMouseButton> mouse_button = event;
    if (mouse_button.is_valid()) {
        if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_UP) {
            handle_zoom(-1.0f);
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_DOWN) {
            handle_zoom(1.0f);
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_MIDDLE) {
            is_rotating = mouse_button->is_pressed();
        }
    }
    
    // Mouse motion events (rotation)
    Ref<InputEventMouseMotion> mouse_motion = event;
    if (mouse_motion.is_valid() && is_rotating) {
        handle_rotation(mouse_motion->get_relative());
    }
}

void RTSCamera::handle_keyboard_movement(double delta) {
    Input *input = Input::get_singleton();
    
    Vector3 direction = Vector3(0, 0, 0);
    
    // Get camera-relative forward and right vectors (on XZ plane)
    float yaw_rad = Math::deg_to_rad(camera_yaw);
    Vector3 forward = Vector3(-Math::sin(yaw_rad), 0, -Math::cos(yaw_rad));
    Vector3 right = Vector3(Math::cos(yaw_rad), 0, -Math::sin(yaw_rad));
    
    if (input->is_action_pressed("ui_up") || input->is_key_pressed(Key::KEY_W)) {
        direction += forward;
    }
    if (input->is_action_pressed("ui_down") || input->is_key_pressed(Key::KEY_S)) {
        direction -= forward;
    }
    if (input->is_action_pressed("ui_left") || input->is_key_pressed(Key::KEY_A)) {
        direction -= right;
    }
    if (input->is_action_pressed("ui_right") || input->is_key_pressed(Key::KEY_D)) {
        direction += right;
    }
    
    if (direction.length_squared() > 0) {
        direction = direction.normalized();
        target_position += direction * move_speed * delta;
    }
}

void RTSCamera::handle_edge_scroll(double delta) {
    Input *input = Input::get_singleton();
    Viewport *viewport = get_viewport();
    
    if (!viewport) return;
    
    Vector2 mouse_pos = viewport->get_mouse_position();
    Vector2 viewport_size = viewport->get_visible_rect().size;
    
    Vector3 direction = Vector3(0, 0, 0);
    
    float yaw_rad = Math::deg_to_rad(camera_yaw);
    Vector3 forward = Vector3(-Math::sin(yaw_rad), 0, -Math::cos(yaw_rad));
    Vector3 right = Vector3(Math::cos(yaw_rad), 0, -Math::sin(yaw_rad));
    
    // Edge detection
    if (mouse_pos.x < edge_scroll_margin) {
        direction -= right;
    } else if (mouse_pos.x > viewport_size.x - edge_scroll_margin) {
        direction += right;
    }
    
    if (mouse_pos.y < edge_scroll_margin) {
        direction += forward;
    } else if (mouse_pos.y > viewport_size.y - edge_scroll_margin) {
        direction -= forward;
    }
    
    if (direction.length_squared() > 0) {
        direction = direction.normalized();
        target_position += direction * edge_scroll_speed * delta;
    }
}

void RTSCamera::handle_zoom(float direction) {
    current_zoom += direction * zoom_speed;
    current_zoom = Math::clamp(current_zoom, min_zoom, max_zoom);
    update_camera_transform();
}

void RTSCamera::handle_rotation(const Vector2 &relative) {
    camera_yaw += relative.x * rotation_speed * 50.0f;
    
    // Clamp pitch for Generals-style view
    camera_pitch -= relative.y * rotation_speed * 30.0f;
    camera_pitch = Math::clamp(camera_pitch, -80.0f, -30.0f);
    
    update_camera_transform();
}

void RTSCamera::update_camera_transform() {
    // Calculate camera position based on target, zoom, and angles
    float pitch_rad = Math::deg_to_rad(camera_pitch);
    float yaw_rad = Math::deg_to_rad(camera_yaw);
    
    // Offset from target position
    Vector3 offset;
    offset.y = current_zoom * Math::sin(-pitch_rad);
    float horizontal_dist = current_zoom * Math::cos(-pitch_rad);
    offset.x = horizontal_dist * Math::sin(yaw_rad);
    offset.z = horizontal_dist * Math::cos(yaw_rad);
    
    Vector3 camera_pos = target_position + offset;
    
    // Ensure camera is above ground
    if (camera_pos.y < 5.0f) {
        camera_pos.y = 5.0f;
    }
    
    set_global_position(camera_pos);
    
    // Calculate look direction
    Vector3 look_target = target_position;
    Vector3 direction = look_target - camera_pos;
    
    // Only look_at if we have a valid direction and aren't looking straight down
    if (direction.length_squared() > 0.01f) {
        // Use a custom up vector when looking nearly straight down
        Vector3 up = Vector3(0, 1, 0);
        
        // Check if we're looking nearly straight down (direction almost parallel to up)
        Vector3 dir_normalized = direction.normalized();
        float dot = Math::abs(dir_normalized.dot(up));
        
        if (dot > 0.99f) {
            // Looking straight down, use forward as up instead
            up = Vector3(0, 0, -1).rotated(Vector3(0, 1, 0), yaw_rad);
        }
        
        look_at(look_target, up);
    }
}

void RTSCamera::set_move_speed(float speed) {
    move_speed = speed;
}

float RTSCamera::get_move_speed() const {
    return move_speed;
}

void RTSCamera::set_zoom_speed(float speed) {
    zoom_speed = speed;
}

float RTSCamera::get_zoom_speed() const {
    return zoom_speed;
}

void RTSCamera::set_min_zoom(float zoom) {
    min_zoom = zoom;
}

float RTSCamera::get_min_zoom() const {
    return min_zoom;
}

void RTSCamera::set_max_zoom(float zoom) {
    max_zoom = zoom;
}

float RTSCamera::get_max_zoom() const {
    return max_zoom;
}

} // namespace rts
