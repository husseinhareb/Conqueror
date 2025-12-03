/**
 * RTSCamera.h
 * RTS-style camera controller with WASD/edge scroll, mouse wheel zoom,
 * and middle-mouse rotation. Inspired by C&C Generals camera style.
 */

#ifndef RTS_CAMERA_H
#define RTS_CAMERA_H

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class RTSCamera : public godot::Camera3D {
    GDCLASS(RTSCamera, godot::Camera3D)

private:
    // Movement settings
    float move_speed = 20.0f;
    float edge_scroll_margin = 50.0f;
    float edge_scroll_speed = 15.0f;
    
    // Zoom settings
    float zoom_speed = 2.0f;
    float min_zoom = 10.0f;
    float max_zoom = 50.0f;
    float current_zoom = 25.0f;
    
    // Rotation settings
    float rotation_speed = 0.005f;
    bool is_rotating = false;
    
    // Camera angle (Generals-style tilt)
    float camera_pitch = -50.0f; // degrees
    float camera_yaw = 0.0f;
    
    // Target position for smooth movement
    godot::Vector3 target_position;

protected:
    static void _bind_methods();

public:
    RTSCamera();
    ~RTSCamera();

    void _ready() override;
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

    // Movement
    void handle_keyboard_movement(double delta);
    void handle_edge_scroll(double delta);
    void handle_zoom(float direction);
    void handle_rotation(const godot::Vector2 &relative);
    
    void update_camera_transform();

    // Getters/Setters
    void set_move_speed(float speed);
    float get_move_speed() const;
    
    void set_zoom_speed(float speed);
    float get_zoom_speed() const;
    
    void set_min_zoom(float zoom);
    float get_min_zoom() const;
    
    void set_max_zoom(float zoom);
    float get_max_zoom() const;
};

} // namespace rts

#endif // RTS_CAMERA_H
