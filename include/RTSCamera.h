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
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
 #include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class Unit; // Forward declaration

class RTSCamera : public godot::Camera3D {
    GDCLASS(RTSCamera, godot::Camera3D)

private:
    // Movement settings
    float move_speed = 20.0f;
    float edge_scroll_margin = 50.0f;
    float edge_scroll_speed = 25.0f;
    
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
    
    // Custom cursor
    godot::Sprite2D *cursor_sprite = nullptr;
    godot::CanvasLayer *cursor_layer = nullptr;
    godot::Vector2 cursor_position = godot::Vector2(0, 0);
    float cursor_speed = 800.0f;
    bool cursor_initialized = false;
    
    // Cursor textures
    godot::Ref<godot::Texture2D> cursor_normal_texture;
    godot::Ref<godot::Texture2D> cursor_move_texture;
    bool using_move_cursor = false;
    
    // Selection box
    godot::ColorRect *selection_box = nullptr;
    godot::Vector2 selection_start = godot::Vector2(0, 0);
    bool is_selecting = false;
    
    // Unit hover and selection
    Unit *hovered_unit = nullptr;
    Unit *selected_unit = nullptr;
    
    // Bottom panel
    godot::Control *bottom_panel_container = nullptr;
    godot::ColorRect *bottom_panel = nullptr;
    godot::ColorRect *bottom_panel_header = nullptr;
    godot::ColorRect *bottom_panel_border = nullptr;
    godot::Label *bottom_panel_title = nullptr;
    godot::Button *bottom_panel_toggle_btn = nullptr;
    bool bottom_panel_expanded = true;
    float bottom_panel_height_percent = 0.20f; // 20% of screen height
    float bottom_panel_header_height = 30.0f;
    
    // Unit info labels in panel
    godot::Label *unit_info_name = nullptr;
    godot::Label *unit_info_health = nullptr;
    godot::Label *unit_info_attack = nullptr;
    godot::Label *unit_info_position = nullptr;

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
    void setup_custom_cursor();
    void update_cursor(double delta);
    void setup_selection_box();
    void update_selection_box();
    void setup_bottom_panel();
    void toggle_bottom_panel();
    void update_bottom_panel_size();
    
    // Hover and selection
    void update_hover_detection();
    Unit* raycast_for_unit(const godot::Vector2 &screen_pos);
    void select_unit(Unit *unit);
    void update_unit_info_panel();
    void update_cursor_mode();
    void create_move_cursor_texture();
    godot::Vector3 raycast_ground(const godot::Vector2 &screen_pos);
    void issue_move_order(const godot::Vector3 &target);

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
