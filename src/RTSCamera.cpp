/**
 * RTSCamera.cpp
 * RTS-style camera controller implementation.
 */

#include "RTSCamera.h"
#include "Unit.h"

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
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
    
    // Bind toggle method for button signal
    ClassDB::bind_method(D_METHOD("_on_toggle_button_pressed"), &RTSCamera::toggle_bottom_panel);;
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
    
    // Setup custom cursor
    setup_custom_cursor();
    
    // Setup selection box
    setup_selection_box();
    
    // Setup bottom panel
    setup_bottom_panel();
    
    update_camera_transform();
}

void RTSCamera::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    update_cursor(delta);
    update_cursor_mode();
    update_selection_box();
    handle_keyboard_movement(delta);
    handle_edge_scroll(delta);
    
    // Update hover detection every frame
    update_hover_detection();
    
    // Update unit info in panel
    update_unit_info_panel();
    
    update_camera_transform();
}

void RTSCamera::_input(const Ref<InputEvent> &event) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Mouse button events (zoom, rotation toggle, selection)
    Ref<InputEventMouseButton> mouse_button = event;
    if (mouse_button.is_valid()) {
        if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_UP) {
            handle_zoom(-1.0f);
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_DOWN) {
            handle_zoom(1.0f);
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_MIDDLE) {
            is_rotating = mouse_button->is_pressed();
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_RIGHT) {
            // Right-click: deselect current unit
            if (mouse_button->is_pressed() && selected_unit) {
                select_unit(nullptr);
            }
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_LEFT) {
            if (mouse_button->is_pressed()) {
                // Check if clicking on toggle button
                if (bottom_panel_toggle_btn) {
                    Rect2 btn_rect = bottom_panel_toggle_btn->get_global_rect();
                    if (btn_rect.has_point(cursor_position)) {
                        toggle_bottom_panel();
                        return; // Don't start selection when clicking button
                    }
                }
                
                // If a unit is selected, left-click issues move order
                if (selected_unit) {
                    // Check if clicking on another unit to select it instead
                    Unit *clicked_unit = raycast_for_unit(cursor_position);
                    if (clicked_unit) {
                        select_unit(clicked_unit);
                        return;
                    }
                    
                    // Otherwise, issue move order to ground position
                    Vector3 ground_pos = raycast_ground(cursor_position);
                    issue_move_order(ground_pos);
                    return;
                }
                
                // No unit selected - check if clicking on a unit to select it
                Unit *clicked_unit = raycast_for_unit(cursor_position);
                if (clicked_unit) {
                    select_unit(clicked_unit);
                    return; // Don't start box selection when clicking a unit
                }
                
                // Start box selection
                is_selecting = true;
                selection_start = cursor_position;
                if (selection_box) {
                    selection_box->set_visible(true);
                    selection_box->set_position(selection_start);
                    selection_box->set_size(Vector2(0, 0));
                }
            } else {
                // End selection
                is_selecting = false;
                if (selection_box) {
                    selection_box->set_visible(false);
                }
            }
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
    Viewport *viewport = get_viewport();
    
    if (!viewport || !cursor_initialized) return;
    
    Vector2 viewport_size = viewport->get_visible_rect().size;
    
    Vector3 direction = Vector3(0, 0, 0);
    
    float yaw_rad = Math::deg_to_rad(camera_yaw);
    Vector3 forward = Vector3(-Math::sin(yaw_rad), 0, -Math::cos(yaw_rad));
    Vector3 right = Vector3(Math::cos(yaw_rad), 0, -Math::sin(yaw_rad));
    
    // Edge detection using custom cursor position
    if (cursor_position.x <= edge_scroll_margin) {
        direction -= right;
    } else if (cursor_position.x >= viewport_size.x - edge_scroll_margin) {
        direction += right;
    }
    
    if (cursor_position.y <= edge_scroll_margin) {
        direction += forward;
    } else if (cursor_position.y >= viewport_size.y - edge_scroll_margin) {
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
    // Rotate yaw (left/right)
    camera_yaw += relative.x * rotation_speed * 50.0f;
    
    // Wrap yaw to 0-360 range
    while (camera_yaw > 360.0f) camera_yaw -= 360.0f;
    while (camera_yaw < 0.0f) camera_yaw += 360.0f;
    
    // Rotate pitch (up/down) - clamp for Generals-style view
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

void RTSCamera::setup_custom_cursor() {
    Viewport *viewport = get_viewport();
    if (!viewport) return;
    
    // Hide system cursor
    Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_HIDDEN);
    
    // Get viewport size and center cursor
    Vector2 viewport_size = viewport->get_visible_rect().size;
    cursor_position = viewport_size / 2.0f;
    
    // Create canvas layer for cursor and UI (always on top)
    cursor_layer = memnew(CanvasLayer);
    cursor_layer->set_layer(100); // High layer to be on top
    get_tree()->get_root()->call_deferred("add_child", cursor_layer);
    
    // Create cursor sprite - add directly to layer (will be added when layer is ready)
    cursor_sprite = memnew(Sprite2D);
    cursor_layer->add_child(cursor_sprite);
    
    // Try to load cursor image directly from file
    Ref<Image> cursor_img = Image::create(1, 1, false, Image::FORMAT_RGBA8);
    Error load_err = cursor_img->load("res://assets/cursor.png");
    
    if (load_err == OK) {
        cursor_normal_texture = ImageTexture::create_from_image(cursor_img);
        cursor_sprite->set_texture(cursor_normal_texture);
        
        // Scale cursor to reasonable size (target ~32px)
        Vector2 img_size = cursor_img->get_size();
        float target_size = 32.0f;
        float scale_factor = target_size / Math::max(img_size.x, img_size.y);
        cursor_sprite->set_scale(Vector2(scale_factor, scale_factor));
        
        UtilityFunctions::print("Loaded custom cursor from assets/cursor.png");
    } else {
        // Fallback: create a simple cursor programmatically
        UtilityFunctions::print("cursor.png not found, using fallback cursor");
        Ref<Image> img = Image::create(32, 32, false, Image::FORMAT_RGBA8);
        img->fill(Color(0, 0, 0, 0));
        
        // Draw a simple crosshair/pointer
        for (int i = 0; i < 20; i++) {
            // Vertical line
            img->set_pixel(10, i, Color(1, 1, 1, 1));
            img->set_pixel(11, i, Color(0, 0, 0, 1));
            // Horizontal line  
            img->set_pixel(i, 10, Color(1, 1, 1, 1));
            img->set_pixel(i, 11, Color(0, 0, 0, 1));
        }
        
        cursor_normal_texture = ImageTexture::create_from_image(img);
        cursor_sprite->set_texture(cursor_normal_texture);
    }
    
    // Create the move cursor texture (+ crosshair)
    create_move_cursor_texture();
    
    cursor_sprite->set_position(cursor_position);
    cursor_sprite->set_offset(Vector2(0, 0)); // Top-left is hotspot
    
    cursor_initialized = true;
}

void RTSCamera::update_cursor(double delta) {
    if (!cursor_initialized || !cursor_sprite) return;
    
    Viewport *viewport = get_viewport();
    if (!viewport) return;
    
    Input *input = Input::get_singleton();
    Vector2 viewport_size = viewport->get_visible_rect().size;
    
    // Move cursor based on mouse movement (use actual mouse position delta)
    Vector2 current_mouse = viewport->get_mouse_position();
    static Vector2 last_mouse = current_mouse;
    Vector2 mouse_delta = current_mouse - last_mouse;
    last_mouse = current_mouse;
    
    // Don't move custom cursor while rotating camera
    if (!is_rotating) {
        // Apply movement to cursor
        cursor_position += mouse_delta;
        
        // Clamp cursor to viewport bounds
        cursor_position.x = Math::clamp(cursor_position.x, 0.0f, viewport_size.x);
        cursor_position.y = Math::clamp(cursor_position.y, 0.0f, viewport_size.y);
        
        // Update cursor sprite position
        cursor_sprite->set_position(cursor_position);
        
        // Warp mouse to center to allow continuous movement detection
        Vector2 center = viewport_size / 2.0f;
        if (current_mouse.distance_to(center) > 100.0f) {
            input->warp_mouse(center);
            last_mouse = center;
        }
    } else {
        // While rotating, just track the mouse position without warping
        last_mouse = current_mouse;
    }
}

void RTSCamera::setup_selection_box() {
    if (!cursor_layer) return;
    
    // Create selection box as a ColorRect
    selection_box = memnew(ColorRect);
    selection_box->set_color(Color(0.2f, 0.6f, 1.0f, 0.3f)); // Light blue with transparency
    selection_box->set_visible(false);
    cursor_layer->add_child(selection_box);
}

void RTSCamera::update_selection_box() {
    if (!is_selecting || !selection_box) return;
    
    // Calculate the selection rectangle
    Vector2 current_pos = cursor_position;
    
    // Calculate top-left corner and size
    float left = Math::min(selection_start.x, current_pos.x);
    float top = Math::min(selection_start.y, current_pos.y);
    float width = Math::abs(current_pos.x - selection_start.x);
    float height = Math::abs(current_pos.y - selection_start.y);
    
    selection_box->set_position(Vector2(left, top));
    selection_box->set_size(Vector2(width, height));
}

void RTSCamera::setup_bottom_panel() {
    Viewport *viewport = get_viewport();
    if (!viewport || !cursor_layer) return;
    
    Vector2 viewport_size = viewport->get_visible_rect().size;
    float panel_height = viewport_size.y * bottom_panel_height_percent;
    
    // Create a Control as anchor container for the entire panel
    bottom_panel_container = memnew(Control);
    bottom_panel_container->set_anchors_preset(Control::PRESET_FULL_RECT);
    bottom_panel_container->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    cursor_layer->add_child(bottom_panel_container);
    
    // Add border line at top of panel (will be hidden when collapsed)
    bottom_panel_border = memnew(ColorRect);
    bottom_panel_border->set_color(Color(0.4f, 0.35f, 0.2f, 1.0f)); // Bronze/gold border
    bottom_panel_border->set_anchor(Side::SIDE_LEFT, 0.0f);
    bottom_panel_border->set_anchor(Side::SIDE_RIGHT, 1.0f);
    bottom_panel_border->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    bottom_panel_border->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    bottom_panel_border->set_offset(Side::SIDE_LEFT, 0);
    bottom_panel_border->set_offset(Side::SIDE_RIGHT, 0);
    bottom_panel_border->set_offset(Side::SIDE_TOP, -2);
    bottom_panel_border->set_offset(Side::SIDE_BOTTOM, 0);
    bottom_panel_border->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(bottom_panel_border);
    
    // Create the header bar
    bottom_panel_header = memnew(ColorRect);
    bottom_panel_header->set_color(Color(0.15f, 0.15f, 0.2f, 0.95f)); // Dark blue-gray
    bottom_panel_header->set_anchor(Side::SIDE_LEFT, 0.0f);
    bottom_panel_header->set_anchor(Side::SIDE_RIGHT, 1.0f);
    bottom_panel_header->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    bottom_panel_header->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    bottom_panel_header->set_offset(Side::SIDE_LEFT, 0);
    bottom_panel_header->set_offset(Side::SIDE_RIGHT, 0);
    bottom_panel_header->set_offset(Side::SIDE_TOP, 0);
    bottom_panel_header->set_offset(Side::SIDE_BOTTOM, bottom_panel_header_height);
    bottom_panel_header->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(bottom_panel_header);
    
    // Create title label
    bottom_panel_title = memnew(Label);
    bottom_panel_title->set_text("  COMMAND PANEL");
    bottom_panel_title->set_anchor(Side::SIDE_LEFT, 0.0f);
    bottom_panel_title->set_anchor(Side::SIDE_RIGHT, 0.5f);
    bottom_panel_title->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    bottom_panel_title->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    bottom_panel_title->set_offset(Side::SIDE_LEFT, 10);
    bottom_panel_title->set_offset(Side::SIDE_RIGHT, 0);
    bottom_panel_title->set_offset(Side::SIDE_TOP, 5);
    bottom_panel_title->set_offset(Side::SIDE_BOTTOM, bottom_panel_header_height);
    bottom_panel_title->add_theme_color_override("font_color", Color(0.9f, 0.85f, 0.6f, 1.0f)); // Gold color
    bottom_panel_title->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(bottom_panel_title);
    
    // Create toggle button in the center top of the panel
    bottom_panel_toggle_btn = memnew(Button);
    bottom_panel_toggle_btn->set_text("▼ Hide");
    bottom_panel_toggle_btn->set_anchor(Side::SIDE_LEFT, 0.5f);
    bottom_panel_toggle_btn->set_anchor(Side::SIDE_RIGHT, 0.5f);
    bottom_panel_toggle_btn->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    bottom_panel_toggle_btn->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    bottom_panel_toggle_btn->set_offset(Side::SIDE_LEFT, -40);
    bottom_panel_toggle_btn->set_offset(Side::SIDE_RIGHT, 40);
    bottom_panel_toggle_btn->set_offset(Side::SIDE_TOP, 3);
    bottom_panel_toggle_btn->set_offset(Side::SIDE_BOTTOM, bottom_panel_header_height - 3);
    bottom_panel_toggle_btn->set_mouse_filter(Control::MOUSE_FILTER_STOP);
    bottom_panel_container->add_child(bottom_panel_toggle_btn);
    
    // Connect button signal to toggle function
    bottom_panel_toggle_btn->connect("pressed", Callable(this, "_on_toggle_button_pressed"));
    
    // Create the main panel body (collapsible)
    bottom_panel = memnew(ColorRect);
    bottom_panel->set_color(Color(0.1f, 0.1f, 0.15f, 0.9f)); // Darker background
    bottom_panel->set_anchor(Side::SIDE_LEFT, 0.0f);
    bottom_panel->set_anchor(Side::SIDE_RIGHT, 1.0f);
    bottom_panel->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    bottom_panel->set_anchor(Side::SIDE_BOTTOM, 1.0f);
    bottom_panel->set_offset(Side::SIDE_LEFT, 0);
    bottom_panel->set_offset(Side::SIDE_RIGHT, 0);
    bottom_panel->set_offset(Side::SIDE_TOP, bottom_panel_header_height);
    bottom_panel->set_offset(Side::SIDE_BOTTOM, 0);
    bottom_panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(bottom_panel);
    
    // Create unit info labels inside the panel
    float label_start_y = bottom_panel_header_height + 15;
    float label_height = 25;
    Color info_color = Color(0.8f, 0.8f, 0.9f, 1.0f);
    
    // Unit name label
    unit_info_name = memnew(Label);
    unit_info_name->set_text("No unit selected");
    unit_info_name->set_anchor(Side::SIDE_LEFT, 0.0f);
    unit_info_name->set_anchor(Side::SIDE_RIGHT, 0.3f);
    unit_info_name->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    unit_info_name->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    unit_info_name->set_offset(Side::SIDE_LEFT, 20);
    unit_info_name->set_offset(Side::SIDE_RIGHT, 0);
    unit_info_name->set_offset(Side::SIDE_TOP, label_start_y);
    unit_info_name->set_offset(Side::SIDE_BOTTOM, label_start_y + label_height);
    unit_info_name->add_theme_color_override("font_color", Color(0.9f, 0.85f, 0.6f, 1.0f)); // Gold
    unit_info_name->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(unit_info_name);
    
    // Health label
    unit_info_health = memnew(Label);
    unit_info_health->set_text("");
    unit_info_health->set_anchor(Side::SIDE_LEFT, 0.0f);
    unit_info_health->set_anchor(Side::SIDE_RIGHT, 0.3f);
    unit_info_health->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    unit_info_health->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    unit_info_health->set_offset(Side::SIDE_LEFT, 20);
    unit_info_health->set_offset(Side::SIDE_RIGHT, 0);
    unit_info_health->set_offset(Side::SIDE_TOP, label_start_y + label_height);
    unit_info_health->set_offset(Side::SIDE_BOTTOM, label_start_y + label_height * 2);
    unit_info_health->add_theme_color_override("font_color", Color(0.4f, 0.9f, 0.4f, 1.0f)); // Green
    unit_info_health->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(unit_info_health);
    
    // Attack label
    unit_info_attack = memnew(Label);
    unit_info_attack->set_text("");
    unit_info_attack->set_anchor(Side::SIDE_LEFT, 0.0f);
    unit_info_attack->set_anchor(Side::SIDE_RIGHT, 0.3f);
    unit_info_attack->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    unit_info_attack->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    unit_info_attack->set_offset(Side::SIDE_LEFT, 20);
    unit_info_attack->set_offset(Side::SIDE_RIGHT, 0);
    unit_info_attack->set_offset(Side::SIDE_TOP, label_start_y + label_height * 2);
    unit_info_attack->set_offset(Side::SIDE_BOTTOM, label_start_y + label_height * 3);
    unit_info_attack->add_theme_color_override("font_color", Color(0.9f, 0.5f, 0.5f, 1.0f)); // Red
    unit_info_attack->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(unit_info_attack);
    
    // Position label
    unit_info_position = memnew(Label);
    unit_info_position->set_text("");
    unit_info_position->set_anchor(Side::SIDE_LEFT, 0.0f);
    unit_info_position->set_anchor(Side::SIDE_RIGHT, 0.3f);
    unit_info_position->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    unit_info_position->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    unit_info_position->set_offset(Side::SIDE_LEFT, 20);
    unit_info_position->set_offset(Side::SIDE_RIGHT, 0);
    unit_info_position->set_offset(Side::SIDE_TOP, label_start_y + label_height * 3);
    unit_info_position->set_offset(Side::SIDE_BOTTOM, label_start_y + label_height * 4);
    unit_info_position->add_theme_color_override("font_color", info_color);
    unit_info_position->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    bottom_panel_container->add_child(unit_info_position);
    
    // Make sure cursor is on top
    if (cursor_sprite) {
        cursor_sprite->move_to_front();
    }
    
    UtilityFunctions::print("Bottom panel setup complete");
}

void RTSCamera::toggle_bottom_panel() {
    if (!bottom_panel || !bottom_panel_header || !bottom_panel_title || !bottom_panel_toggle_btn || !bottom_panel_border) return;
    
    bottom_panel_expanded = !bottom_panel_expanded;
    
    if (bottom_panel_expanded) {
        // Show full panel - restore anchors to show 20% from bottom
        bottom_panel->set_visible(true);
        bottom_panel_border->set_visible(true);
        
        // Header at top of panel area
        bottom_panel_header->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
        bottom_panel_header->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
        bottom_panel_header->set_offset(Side::SIDE_TOP, 0);
        bottom_panel_header->set_offset(Side::SIDE_BOTTOM, bottom_panel_header_height);
        
        // Title position
        bottom_panel_title->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
        bottom_panel_title->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
        bottom_panel_title->set_offset(Side::SIDE_TOP, 5);
        bottom_panel_title->set_offset(Side::SIDE_BOTTOM, bottom_panel_header_height);
        
        // Button position
        bottom_panel_toggle_btn->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
        bottom_panel_toggle_btn->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
        bottom_panel_toggle_btn->set_offset(Side::SIDE_TOP, 3);
        bottom_panel_toggle_btn->set_offset(Side::SIDE_BOTTOM, bottom_panel_header_height - 3);
        bottom_panel_toggle_btn->set_text("▼ Hide");
        
        bottom_panel_title->set_text("  COMMAND PANEL");
    } else {
        // Collapse - only show header at very bottom
        bottom_panel->set_visible(false);
        bottom_panel_border->set_visible(false);
        
        // Move header to bottom edge
        bottom_panel_header->set_anchor(Side::SIDE_TOP, 1.0f);
        bottom_panel_header->set_anchor(Side::SIDE_BOTTOM, 1.0f);
        bottom_panel_header->set_offset(Side::SIDE_TOP, -bottom_panel_header_height);
        bottom_panel_header->set_offset(Side::SIDE_BOTTOM, 0);
        
        // Title position
        bottom_panel_title->set_anchor(Side::SIDE_TOP, 1.0f);
        bottom_panel_title->set_anchor(Side::SIDE_BOTTOM, 1.0f);
        bottom_panel_title->set_offset(Side::SIDE_TOP, -bottom_panel_header_height + 5);
        bottom_panel_title->set_offset(Side::SIDE_BOTTOM, 0);
        
        // Button position
        bottom_panel_toggle_btn->set_anchor(Side::SIDE_TOP, 1.0f);
        bottom_panel_toggle_btn->set_anchor(Side::SIDE_BOTTOM, 1.0f);
        bottom_panel_toggle_btn->set_offset(Side::SIDE_TOP, -bottom_panel_header_height + 3);
        bottom_panel_toggle_btn->set_offset(Side::SIDE_BOTTOM, -3);
        bottom_panel_toggle_btn->set_text("▲ Show");
        
        bottom_panel_title->set_text("  COMMAND PANEL");
    }
}

void RTSCamera::update_bottom_panel_size() {
    // Called when viewport resizes - update panel positions
    Viewport *viewport = get_viewport();
    if (!viewport || !bottom_panel) return;
    
    Vector2 viewport_size = viewport->get_visible_rect().size;
    float panel_height = viewport_size.y * bottom_panel_height_percent;
    
    bottom_panel_header->set_size(Vector2(viewport_size.x, bottom_panel_header_height));
    bottom_panel->set_size(Vector2(viewport_size.x, panel_height - bottom_panel_header_height));
    
    if (bottom_panel_expanded) {
        bottom_panel_header->set_position(Vector2(0, viewport_size.y - panel_height));
        bottom_panel_title->set_position(Vector2(10, viewport_size.y - panel_height + 5));
        bottom_panel->set_position(Vector2(0, viewport_size.y - panel_height + bottom_panel_header_height));
    } else {
        bottom_panel_header->set_position(Vector2(0, viewport_size.y - bottom_panel_header_height));
        bottom_panel_title->set_position(Vector2(10, viewport_size.y - bottom_panel_header_height + 5));
    }
}

void RTSCamera::update_hover_detection() {
    // Don't do hover detection while rotating or selecting
    if (is_rotating || is_selecting) return;
    
    // Raycast to find unit under cursor
    Unit *new_hovered = raycast_for_unit(cursor_position);
    
    // Update hovered unit state
    if (new_hovered != hovered_unit) {
        // Unhover previous unit
        if (hovered_unit) {
            hovered_unit->set_hovered(false);
        }
        
        // Hover new unit
        hovered_unit = new_hovered;
        if (hovered_unit) {
            hovered_unit->set_hovered(true);
        }
    }
}

Unit* RTSCamera::raycast_for_unit(const Vector2 &screen_pos) {
    Viewport *viewport = get_viewport();
    if (!viewport) return nullptr;
    
    // Get the 3D world
    Ref<World3D> world = get_world_3d();
    if (world.is_null()) return nullptr;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return nullptr;
    
    // Project ray from screen position
    Vector3 ray_origin = project_ray_origin(screen_pos);
    Vector3 ray_direction = project_ray_normal(screen_pos);
    Vector3 ray_end = ray_origin + ray_direction * 1000.0f;
    
    // Create ray query - include layer 2 where units are
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(ray_origin, ray_end);
    query->set_collide_with_areas(false);
    query->set_collide_with_bodies(true);
    query->set_collision_mask(2); // Layer 2 is units
    
    // Perform raycast
    Dictionary result = space_state->intersect_ray(query);
    
    if (result.is_empty()) return nullptr;
    
    // Check if we hit a Unit
    Object *collider = Object::cast_to<Object>(result["collider"]);
    if (!collider) return nullptr;
    
    // Check if the collider or its parent is a Unit
    Unit *unit = Object::cast_to<Unit>(collider);
    if (unit) return unit;
    
    // Check parent (in case we hit a collision shape child)
    Node *parent = Object::cast_to<Node>(collider);
    if (parent) {
        parent = parent->get_parent();
        if (parent) {
            unit = Object::cast_to<Unit>(parent);
            if (unit) return unit;
        }
    }
    
    return nullptr;
}

void RTSCamera::select_unit(Unit *unit) {
    // Deselect previous unit
    if (selected_unit && selected_unit != unit) {
        selected_unit->set_selected(false);
    }
    
    // Select new unit
    selected_unit = unit;
    if (selected_unit) {
        selected_unit->set_selected(true);
        UtilityFunctions::print("Selected unit: ", selected_unit->get_unit_name());
    }
}

void RTSCamera::update_unit_info_panel() {
    if (!unit_info_name || !unit_info_health || !unit_info_attack || !unit_info_position) return;
    
    if (selected_unit) {
        // Update labels with selected unit info
        unit_info_name->set_text(String("Unit: ") + selected_unit->get_unit_name());
        
        String health_text = String("Health: ") + String::num_int64(selected_unit->get_health()) + 
                            String(" / ") + String::num_int64(selected_unit->get_max_health());
        unit_info_health->set_text(health_text);
        
        String attack_text = String("Attack: ") + String::num_int64(selected_unit->get_attack_damage()) + 
                            String(" (Range: ") + String::num(selected_unit->get_attack_range(), 1) + String(")");
        unit_info_attack->set_text(attack_text);
        
        Vector3 pos = selected_unit->get_global_position();
        String pos_text = String("Position: (") + String::num(pos.x, 1) + String(", ") + 
                          String::num(pos.y, 1) + String(", ") + String::num(pos.z, 1) + String(")");
        unit_info_position->set_text(pos_text);
    } else if (hovered_unit) {
        // Show hovered unit info in dimmer style (preview)
        unit_info_name->set_text(String("[Hover] ") + hovered_unit->get_unit_name());
        
        String health_text = String("Health: ") + String::num_int64(hovered_unit->get_health()) + 
                            String(" / ") + String::num_int64(hovered_unit->get_max_health());
        unit_info_health->set_text(health_text);
        
        unit_info_attack->set_text("");
        unit_info_position->set_text("(Click to select)");
    } else {
        // No unit selected or hovered
        unit_info_name->set_text("No unit selected");
        unit_info_health->set_text("");
        unit_info_attack->set_text("");
        unit_info_position->set_text("");
    }
}

void RTSCamera::update_cursor_mode() {
    if (!cursor_sprite || !cursor_initialized) return;
    
    bool should_use_move_cursor = (selected_unit != nullptr);
    
    if (should_use_move_cursor != using_move_cursor) {
        using_move_cursor = should_use_move_cursor;
        
        if (using_move_cursor && cursor_move_texture.is_valid()) {
            cursor_sprite->set_texture(cursor_move_texture);
            // Center the crosshair cursor
            cursor_sprite->set_offset(Vector2(-16, -16));
        } else if (cursor_normal_texture.is_valid()) {
            cursor_sprite->set_texture(cursor_normal_texture);
            cursor_sprite->set_offset(Vector2(0, 0));
        }
    }
}

void RTSCamera::create_move_cursor_texture() {
    // Create a + crosshair cursor for move mode
    Ref<Image> img = Image::create(32, 32, false, Image::FORMAT_RGBA8);
    img->fill(Color(0, 0, 0, 0)); // Transparent background
    
    // Draw the + crosshair
    int center = 16;
    int thickness = 2;
    int arm_length = 10;
    
    // Outer glow (light green)
    Color glow_color = Color(0.4f, 1.0f, 0.4f, 0.5f);
    for (int i = -arm_length - 2; i <= arm_length + 2; i++) {
        for (int t = -thickness - 1; t <= thickness + 1; t++) {
            // Horizontal arm glow
            int px = center + i;
            int py = center + t;
            if (px >= 0 && px < 32 && py >= 0 && py < 32) {
                img->set_pixel(px, py, glow_color);
            }
            // Vertical arm glow
            px = center + t;
            py = center + i;
            if (px >= 0 && px < 32 && py >= 0 && py < 32) {
                img->set_pixel(px, py, glow_color);
            }
        }
    }
    
    // Main crosshair (white with green tint)
    Color main_color = Color(0.9f, 1.0f, 0.9f, 1.0f);
    for (int i = -arm_length; i <= arm_length; i++) {
        for (int t = -thickness / 2; t <= thickness / 2; t++) {
            // Horizontal arm
            int px = center + i;
            int py = center + t;
            if (px >= 0 && px < 32 && py >= 0 && py < 32) {
                img->set_pixel(px, py, main_color);
            }
            // Vertical arm
            px = center + t;
            py = center + i;
            if (px >= 0 && px < 32 && py >= 0 && py < 32) {
                img->set_pixel(px, py, main_color);
            }
        }
    }
    
    // Center dot (bright green)
    Color center_color = Color(0.2f, 1.0f, 0.2f, 1.0f);
    for (int x = center - 1; x <= center + 1; x++) {
        for (int y = center - 1; y <= center + 1; y++) {
            if (x >= 0 && x < 32 && y >= 0 && y < 32) {
                img->set_pixel(x, y, center_color);
            }
        }
    }
    
    // Add small arrows pointing outward at the ends
    Color arrow_color = Color(0.3f, 0.9f, 0.3f, 1.0f);
    // Right arrow
    img->set_pixel(center + arm_length + 1, center, arrow_color);
    img->set_pixel(center + arm_length, center - 1, arrow_color);
    img->set_pixel(center + arm_length, center + 1, arrow_color);
    // Left arrow
    img->set_pixel(center - arm_length - 1, center, arrow_color);
    img->set_pixel(center - arm_length, center - 1, arrow_color);
    img->set_pixel(center - arm_length, center + 1, arrow_color);
    // Up arrow
    img->set_pixel(center, center - arm_length - 1, arrow_color);
    img->set_pixel(center - 1, center - arm_length, arrow_color);
    img->set_pixel(center + 1, center - arm_length, arrow_color);
    // Down arrow
    img->set_pixel(center, center + arm_length + 1, arrow_color);
    img->set_pixel(center - 1, center + arm_length, arrow_color);
    img->set_pixel(center + 1, center + arm_length, arrow_color);
    
    cursor_move_texture = ImageTexture::create_from_image(img);
}

Vector3 RTSCamera::raycast_ground(const Vector2 &screen_pos) {
    Viewport *viewport = get_viewport();
    if (!viewport) return Vector3(0, 0, 0);
    
    // Get the 3D world
    Ref<World3D> world = get_world_3d();
    if (world.is_null()) return Vector3(0, 0, 0);
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return Vector3(0, 0, 0);
    
    // Project ray from screen position
    Vector3 ray_origin = project_ray_origin(screen_pos);
    Vector3 ray_direction = project_ray_normal(screen_pos);
    Vector3 ray_end = ray_origin + ray_direction * 1000.0f;
    
    // Create ray query - hit ground (layer 1)
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(ray_origin, ray_end);
    query->set_collide_with_areas(false);
    query->set_collide_with_bodies(true);
    query->set_collision_mask(1); // Layer 1 is ground
    
    // Perform raycast
    Dictionary result = space_state->intersect_ray(query);
    
    if (result.is_empty()) {
        // Fallback: calculate intersection with Y=0 plane
        if (Math::abs(ray_direction.y) > 0.001f) {
            float t = -ray_origin.y / ray_direction.y;
            if (t > 0) {
                return ray_origin + ray_direction * t;
            }
        }
        return Vector3(0, 0, 0);
    }
    
    return result["position"];
}

void RTSCamera::issue_move_order(const Vector3 &target) {
    if (!selected_unit) return;
    
    selected_unit->set_move_target(target);
    UtilityFunctions::print("Move order issued to position: (", target.x, ", ", target.y, ", ", target.z, ")");
}

} // namespace rts
