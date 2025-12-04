/**
 * RTSCamera.cpp
 * RTS-style camera controller implementation.
 */

#include "RTSCamera.h"
#include "Unit.h"
#include "Building.h"
#include "Bulldozer.h"
#include "FloorSnapper.h"

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
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <functional>

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
    ClassDB::bind_method(D_METHOD("_on_toggle_button_pressed"), &RTSCamera::toggle_bottom_panel);
    ClassDB::bind_method(D_METHOD("_on_build_power_pressed"), &RTSCamera::on_build_power_pressed);
    ClassDB::bind_method(D_METHOD("_on_build_barracks_pressed"), &RTSCamera::on_build_barracks_pressed);
    ClassDB::bind_method(D_METHOD("_on_build_bulldozer_pressed"), &RTSCamera::on_build_bulldozer_pressed);
    ClassDB::bind_method(D_METHOD("_on_train_unit_pressed"), &RTSCamera::on_train_unit_pressed);
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
    
    // Set far plane for large terrain
    set_far(2000.0f);
    
    // Initialize target position at origin (ground level)
    target_position = Vector3(0, 0, 0);
    current_zoom = 50.0f; // Start at a good viewing distance for large map
    
    UtilityFunctions::print("RTSCamera: Initial target=", target_position, " zoom=", current_zoom, " pitch=", camera_pitch);
    
    // Setup custom cursor
    setup_custom_cursor();
    
    // Setup selection box
    setup_selection_box();
    
    // Setup bottom panel
    setup_bottom_panel();
    
    update_camera_transform();
    
    UtilityFunctions::print("RTSCamera: Camera position=", get_global_position());
}

void RTSCamera::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Safety check: ensure drag panning stops if right mouse button is released
    // This handles cases where the release event was missed (e.g., window lost focus)
    Input *input = Input::get_singleton();
    if (is_drag_panning && !input->is_mouse_button_pressed(MouseButton::MOUSE_BUTTON_RIGHT)) {
        is_drag_panning = false;
        if (drag_arrow_sprite) {
            drag_arrow_sprite->set_visible(false);
        }
        if (cursor_sprite) {
            cursor_sprite->set_visible(true);
        }
    }
    
    // Same safety check for rotation
    if (is_rotating && !input->is_mouse_button_pressed(MouseButton::MOUSE_BUTTON_MIDDLE)) {
        is_rotating = false;
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
    
    // Update build buttons visibility
    update_build_buttons();
    
    // Update ghost building position if placing
    if (is_placing_building && selected_bulldozer) {
        Vector3 ground_pos = raycast_ground(cursor_position);
        selected_bulldozer->update_ghost_position(ground_pos);
    }
    
    update_camera_transform();
}

void RTSCamera::_input(const Ref<InputEvent> &event) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Mouse button events (zoom, rotation toggle, selection)
    Ref<InputEventMouseButton> mouse_button = event;
    if (mouse_button.is_valid()) {
        // Zoom always works
        if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_UP) {
            handle_zoom(-1.0f);
            return;
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_DOWN) {
            handle_zoom(1.0f);
            return;
        }
        
        // Block all other mouse interactions when cursor is over panel
        if (is_cursor_over_panel()) {
            if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_LEFT && mouse_button->is_pressed()) {
                handle_panel_click();
            }
            return; // Block everything else when over panel
        }
        
        if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_MIDDLE) {
            is_rotating = mouse_button->is_pressed();
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_RIGHT) {
            // Right-click: drag pan or cancel/deselect
            if (mouse_button->is_pressed()) {
                // Start drag panning
                is_drag_panning = true;
                drag_start_position = cursor_position;
                
                // Show arrow at start position, hide cursor
                if (drag_arrow_sprite) {
                    drag_arrow_sprite->set_visible(true);
                    drag_arrow_sprite->set_position(drag_start_position);
                    drag_arrow_sprite->set_rotation(0);
                }
                if (cursor_sprite) {
                    cursor_sprite->set_visible(false);
                }
            } else {
                // Right-click released
                // Check if mouse moved significantly during drag
                float drag_distance = (cursor_position - drag_start_position).length();
                if (drag_distance < 5.0f) {
                    // Minimal movement - treat as click for cancel/deselect
                    if (is_placing_building) {
                        cancel_building_placement();
                    } else {
                        deselect_all();
                    }
                }
                is_drag_panning = false;
                
                // Hide arrow, show cursor
                if (drag_arrow_sprite) {
                    drag_arrow_sprite->set_visible(false);
                }
                if (cursor_sprite) {
                    cursor_sprite->set_visible(true);
                }
            }
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_LEFT) {
            if (mouse_button->is_pressed()) {
                // Check if clicking on toggle button (when collapsed, it's at bottom)
                if (bottom_panel_toggle_btn) {
                    Rect2 btn_rect = bottom_panel_toggle_btn->get_global_rect();
                    if (btn_rect.has_point(cursor_position)) {
                        toggle_bottom_panel();
                        return;
                    }
                }
                
                // If placing a building, confirm placement
                if (is_placing_building) {
                    confirm_building_placement();
                    return;
                }
                
                // If a unit is selected, left-click issues move order (unless clicking another unit/building/bulldozer)
                bool has_unit_selection = selected_unit || !selected_units.empty();
                bool has_bulldozer_selection = selected_bulldozer || !selected_bulldozers.empty();
                bool has_any_selection = has_unit_selection || has_bulldozer_selection;
                
                if (has_any_selection) {
                    Unit *clicked_unit = raycast_for_unit(cursor_position);
                    if (clicked_unit) {
                        select_unit(clicked_unit);
                        return;
                    }
                    
                    Bulldozer *clicked_bulldozer = raycast_for_bulldozer(cursor_position);
                    if (clicked_bulldozer) {
                        select_bulldozer(clicked_bulldozer);
                        return;
                    }
                    
                    Building *clicked_building = raycast_for_building(cursor_position);
                    if (clicked_building) {
                        select_building(clicked_building);
                        return;
                    }
                    
                    // Issue move order to all selected units and bulldozers
                    Vector3 ground_pos = raycast_ground(cursor_position);
                    issue_move_order(ground_pos);
                    return;
                }
                
                // No unit/bulldozer selected - check what to select
                Bulldozer *clicked_bulldozer = raycast_for_bulldozer(cursor_position);
                if (clicked_bulldozer) {
                    select_bulldozer(clicked_bulldozer);
                    return;
                }
                
                Unit *clicked_unit = raycast_for_unit(cursor_position);
                if (clicked_unit) {
                    select_unit(clicked_unit);
                    return;
                }
                
                Building *clicked_building = raycast_for_building(cursor_position);
                if (clicked_building) {
                    select_building(clicked_building);
                    return;
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
                // End selection - perform box selection only if we were actually box selecting
                if (is_selecting) {
                    is_selecting = false;
                    if (selection_box) {
                        selection_box->set_visible(false);
                        // Perform box selection if the box has some size
                        Vector2 box_size = selection_box->get_size();
                        if (box_size.x > 5.0f || box_size.y > 5.0f) {
                            perform_box_selection();
                        }
                    }
                }
            }
        }
    }
    
    // Mouse motion events
    Ref<InputEventMouseMotion> mouse_motion = event;
    if (mouse_motion.is_valid()) {
        if (is_rotating) {
            handle_rotation(mouse_motion->get_relative());
        } else if (is_drag_panning) {
            handle_drag_pan(mouse_motion->get_relative());
        }
    }
    
    // Escape key cancels building placement
    Ref<InputEventKey> key_event = event;
    if (key_event.is_valid() && key_event->is_pressed()) {
        if (key_event->get_keycode() == Key::KEY_ESCAPE) {
            if (is_placing_building) {
                cancel_building_placement();
            }
        }
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
        clamp_camera_to_bounds();
    }
}

void RTSCamera::handle_edge_scroll(double delta) {
    // Don't edge scroll while drag panning
    if (is_drag_panning) return;
    
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
        clamp_camera_to_bounds();
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
    
    // Rotate pitch (up/down) - clamp to look more downward so edges aren't visible
    camera_pitch -= relative.y * rotation_speed * 30.0f;
    camera_pitch = Math::clamp(camera_pitch, -80.0f, -45.0f);
    
    update_camera_transform();
}

void RTSCamera::handle_drag_pan(const Vector2 &relative) {
    // Right-click drag panning - "grab and drag" style
    // When you drag the mouse, it's like grabbing the map and pulling it
    // So dragging right makes the camera move right (map slides left under cursor)
    // Dragging down makes the camera move down/backward (map slides up)
    
    float yaw_rad = Math::deg_to_rad(camera_yaw);
    Vector3 forward = Vector3(-Math::sin(yaw_rad), 0, -Math::cos(yaw_rad));
    Vector3 right = Vector3(Math::cos(yaw_rad), 0, -Math::sin(yaw_rad));
    
    // Scale movement by zoom level for consistent feel
    float zoom_factor = current_zoom / 25.0f;  // Normalize to default zoom
    float speed = drag_pan_speed * drag_pan_multiplier * zoom_factor;
    
    // Move camera in the same direction as mouse drag
    // Drag right = camera moves right = we see more to the right
    // Drag down = camera moves backward = we see more below
    target_position += right * relative.x * speed;
    target_position -= forward * relative.y * speed;
    clamp_camera_to_bounds();
    
    // Update arrow direction based on total offset from start
    if (drag_arrow_sprite) {
        Vector2 total_offset = cursor_position - drag_start_position;
        float dist = total_offset.length();
        
        if (dist > 15.0f) {
            // Godot 2D rotation: 0 = pointing RIGHT, increases counter-clockwise
            // The arrow image (down-arrow.png) points DOWN at rotation 0
            // So: image pointing down = Godot rotation 0 means the image is pre-rotated -90° from Godot's default
            // 
            // We want the arrow to point in the direction of total_offset:
            // - offset (1, 0) = right → arrow should point right → need rotation -π/2 (or +3π/2)
            // - offset (0, 1) = down → arrow should point down → need rotation 0
            // - offset (-1, 0) = left → arrow should point left → need rotation π/2
            // - offset (0, -1) = up → arrow should point up → need rotation π
            //
            // Standard atan2(y, x) gives angle from positive X axis, counter-clockwise
            // We need angle from positive Y axis (down), clockwise
            // angle = atan2(-x, y) or equivalently: atan2(y, x) - π/2
            float angle = atan2(total_offset.y, total_offset.x) - Math_PI / 2.0f;
            
            // Snap to 8 directions (every 45 degrees)
            float snap = Math_PI / 4.0f;
            float snapped = round(angle / snap) * snap;
            drag_arrow_sprite->set_rotation(snapped);
        }
    }
    
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

void RTSCamera::clamp_camera_to_bounds() {
    // Keep camera within map bounds - leave margin so edges aren't visible
    // Map is 512 * 2 = 1024 world units, centered at origin (-512 to +512)
    // Leave a larger margin to prevent seeing the edges
    float map_half_size = 512.0f;
    float margin = 150.0f; // Keep camera this far from edges
    float max_coord = map_half_size - margin;
    
    target_position.x = Math::clamp(target_position.x, -max_coord, max_coord);
    target_position.z = Math::clamp(target_position.z, -max_coord, max_coord);
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
    
    // Create drag arrow sprite
    drag_arrow_sprite = memnew(Sprite2D);
    cursor_layer->add_child(drag_arrow_sprite);
    
    // Load arrow image or create fallback
    Ref<Image> arrow_img = Image::create(1, 1, false, Image::FORMAT_RGBA8);
    if (arrow_img->load("res://assets/down-arrow.png") == OK) {
        drag_arrow_texture = ImageTexture::create_from_image(arrow_img);
        Vector2 arrow_size = arrow_img->get_size();
        float scale = 48.0f / Math::max(arrow_size.x, arrow_size.y);
        drag_arrow_sprite->set_scale(Vector2(scale, scale));
    } else {
        // Fallback arrow
        Ref<Image> img = Image::create(32, 32, false, Image::FORMAT_RGBA8);
        img->fill(Color(0, 0, 0, 0));
        for (int y = 0; y < 20; y++) {
            img->set_pixel(15, y, Color(1, 1, 1, 1));
            img->set_pixel(16, y, Color(1, 1, 1, 1));
        }
        for (int i = 0; i < 8; i++) {
            img->set_pixel(15 - i, 12 + i, Color(1, 1, 1, 1));
            img->set_pixel(16 + i, 12 + i, Color(1, 1, 1, 1));
        }
        drag_arrow_texture = ImageTexture::create_from_image(img);
    }
    drag_arrow_sprite->set_texture(drag_arrow_texture);
    drag_arrow_sprite->set_visible(false);
    
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
        
        // Update cursor sprite position (only if not drag panning - cursor is hidden during drag)
        if (!is_drag_panning) {
            cursor_sprite->set_position(cursor_position);
            
            // Warp mouse to center to allow continuous movement detection
            Vector2 center = viewport_size / 2.0f;
            if (current_mouse.distance_to(center) > 100.0f) {
                input->warp_mouse(center);
                last_mouse = center;
            }
        } else {
            // During drag panning, just track mouse without warping
            last_mouse = current_mouse;
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
    bottom_panel_toggle_btn->set_text("Hide");
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
    
    // Setup build buttons (initially hidden)
    setup_build_buttons();
    
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
        bottom_panel_toggle_btn->set_text("Hide");
        
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
        bottom_panel_toggle_btn->set_text("Show");
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
    // Don't do hover detection while rotating, selecting, or over panel
    if (is_rotating || is_selecting || is_cursor_over_panel()) {
        // Clear any existing hover states when over panel
        if (is_cursor_over_panel()) {
            if (hovered_bulldozer) {
                hovered_bulldozer->set_hovered(false);
                hovered_bulldozer = nullptr;
            }
            if (hovered_unit) {
                hovered_unit->set_hovered(false);
                hovered_unit = nullptr;
            }
            if (hovered_building) {
                hovered_building->set_hovered(false);
                hovered_building = nullptr;
            }
        }
        return;
    }
    
    // Raycast to find what's under cursor (in priority order)
    Bulldozer *new_hovered_bulldozer = raycast_for_bulldozer(cursor_position);
    Unit *new_hovered_unit = nullptr;
    Building *new_hovered_building = nullptr;
    
    if (!new_hovered_bulldozer) {
        new_hovered_unit = raycast_for_unit(cursor_position);
    }
    
    if (!new_hovered_bulldozer && !new_hovered_unit) {
        new_hovered_building = raycast_for_building(cursor_position);
    }
    
    // Update hovered bulldozer state
    if (new_hovered_bulldozer != hovered_bulldozer) {
        if (hovered_bulldozer) {
            hovered_bulldozer->set_hovered(false);
        }
        hovered_bulldozer = new_hovered_bulldozer;
        if (hovered_bulldozer) {
            hovered_bulldozer->set_hovered(true);
        }
    }
    
    // Update hovered unit state
    if (new_hovered_unit != hovered_unit) {
        if (hovered_unit) {
            hovered_unit->set_hovered(false);
        }
        hovered_unit = new_hovered_unit;
        if (hovered_unit) {
            hovered_unit->set_hovered(true);
        }
    }
    
    // Update hovered building state
    if (new_hovered_building != hovered_building) {
        if (hovered_building) {
            hovered_building->set_hovered(false);
        }
        hovered_building = new_hovered_building;
        if (hovered_building) {
            hovered_building->set_hovered(true);
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

Building* RTSCamera::raycast_for_building(const Vector2 &screen_pos) {
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
    
    // Create ray query - include layer 4 where buildings are
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(ray_origin, ray_end);
    query->set_collide_with_areas(false);
    query->set_collide_with_bodies(true);
    query->set_collision_mask(4); // Layer 4 is buildings
    
    // Perform raycast
    Dictionary result = space_state->intersect_ray(query);
    
    if (result.is_empty()) return nullptr;
    
    // Check if we hit a Building
    Object *collider = Object::cast_to<Object>(result["collider"]);
    if (!collider) return nullptr;
    
    // Check if the collider is a Building
    Building *building = Object::cast_to<Building>(collider);
    if (building) return building;
    
    // Check parent (in case we hit a collision shape child)
    Node *parent = Object::cast_to<Node>(collider);
    if (parent) {
        parent = parent->get_parent();
        if (parent) {
            building = Object::cast_to<Building>(parent);
            if (building) return building;
        }
    }
    
    return nullptr;
}

void RTSCamera::select_unit(Unit *unit) {
    // Deselect any selected building first
    if (selected_building) {
        selected_building->set_selected(false);
        selected_building = nullptr;
    }
    
    // Deselect any selected bulldozer
    if (selected_bulldozer) {
        selected_bulldozer->set_selected(false);
        selected_bulldozer = nullptr;
    }
    
    // Clear multi-selection lists
    for (Unit *u : selected_units) {
        if (u && u != unit) u->set_selected(false);
    }
    selected_units.clear();
    for (Bulldozer *b : selected_bulldozers) {
        if (b) b->set_selected(false);
    }
    selected_bulldozers.clear();
    
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

void RTSCamera::select_building(Building *building) {
    // Deselect any selected unit first
    if (selected_unit) {
        selected_unit->set_selected(false);
        selected_unit = nullptr;
    }
    
    // Deselect any selected bulldozer
    if (selected_bulldozer) {
        selected_bulldozer->set_selected(false);
        selected_bulldozer = nullptr;
    }
    
    // Clear multi-selection lists
    for (Unit *u : selected_units) {
        if (u) u->set_selected(false);
    }
    selected_units.clear();
    for (Bulldozer *b : selected_bulldozers) {
        if (b) b->set_selected(false);
    }
    selected_bulldozers.clear();
    
    // Deselect previous building
    if (selected_building && selected_building != building) {
        selected_building->set_selected(false);
    }
    
    // Select new building
    selected_building = building;
    if (selected_building) {
        selected_building->set_selected(true);
        UtilityFunctions::print("Selected building: ", selected_building->get_building_name());
    }
}

void RTSCamera::deselect_all() {
    // Deselect single selections
    if (selected_unit) {
        selected_unit->set_selected(false);
        selected_unit = nullptr;
    }
    if (selected_building) {
        selected_building->set_selected(false);
        selected_building = nullptr;
    }
    if (selected_bulldozer) {
        selected_bulldozer->set_selected(false);
        selected_bulldozer = nullptr;
    }
    
    // Deselect all units in multi-selection
    for (Unit *unit : selected_units) {
        if (unit) {
            unit->set_selected(false);
        }
    }
    selected_units.clear();
    
    // Deselect all bulldozers in multi-selection
    for (Bulldozer *bulldozer : selected_bulldozers) {
        if (bulldozer) {
            bulldozer->set_selected(false);
        }
    }
    selected_bulldozers.clear();
}

bool RTSCamera::is_position_in_selection_box(const Vector2 &screen_pos) {
    float left = Math::min(selection_start.x, cursor_position.x);
    float right = Math::max(selection_start.x, cursor_position.x);
    float top = Math::min(selection_start.y, cursor_position.y);
    float bottom = Math::max(selection_start.y, cursor_position.y);
    
    return screen_pos.x >= left && screen_pos.x <= right &&
           screen_pos.y >= top && screen_pos.y <= bottom;
}

void RTSCamera::perform_box_selection() {
    // Clear previous selection
    deselect_all();
    
    // Get the scene tree root to find all units and bulldozers
    Node *root = get_tree()->get_root();
    if (!root) return;
    
    // Find all units and bulldozers recursively
    TypedArray<Node> all_nodes = root->get_children();
    
    // Helper lambda to recursively find all nodes
    std::vector<Node*> nodes_to_check;
    std::function<void(Node*)> collect_nodes = [&](Node *node) {
        if (!node) return;
        nodes_to_check.push_back(node);
        TypedArray<Node> children = node->get_children();
        for (int i = 0; i < children.size(); i++) {
            collect_nodes(Object::cast_to<Node>(children[i]));
        }
    };
    
    for (int i = 0; i < all_nodes.size(); i++) {
        collect_nodes(Object::cast_to<Node>(all_nodes[i]));
    }
    
    // Check each node
    for (Node *node : nodes_to_check) {
        // Check if it's a Unit
        Unit *unit = Object::cast_to<Unit>(node);
        if (unit) {
            // Project unit's world position to screen
            Vector3 world_pos = unit->get_global_position();
            Vector2 screen_pos = unproject_position(world_pos);
            
            // Check if the screen position is within the selection box
            if (is_position_in_selection_box(screen_pos)) {
                unit->set_selected(true);
                selected_units.push_back(unit);
            }
            continue;
        }
        
        // Check if it's a Bulldozer
        Bulldozer *bulldozer = Object::cast_to<Bulldozer>(node);
        if (bulldozer) {
            // Project bulldozer's world position to screen
            Vector3 world_pos = bulldozer->get_global_position();
            Vector2 screen_pos = unproject_position(world_pos);
            
            // Check if the screen position is within the selection box
            if (is_position_in_selection_box(screen_pos)) {
                bulldozer->set_selected(true);
                selected_bulldozers.push_back(bulldozer);
            }
        }
    }
    
    // Set single selection pointers if only one is selected
    if (selected_units.size() == 1) {
        selected_unit = selected_units[0];
    }
    if (selected_bulldozers.size() == 1) {
        selected_bulldozer = selected_bulldozers[0];
    }
    
    int total_selected = selected_units.size() + selected_bulldozers.size();
    if (total_selected > 0) {
        UtilityFunctions::print("Box selected ", total_selected, " units/vehicles");
    }
}

bool RTSCamera::is_cursor_over_panel() const {
    if (!bottom_panel_expanded) return false;
    
    Viewport *viewport = get_viewport();
    if (!viewport) return false;
    
    Vector2 viewport_size = viewport->get_visible_rect().size;
    float panel_top = viewport_size.y * (1.0f - bottom_panel_height_percent);
    
    return cursor_position.y >= panel_top;
}

bool RTSCamera::handle_panel_click() {
    // Returns true if click was handled by panel (should block game interaction)
    if (!is_cursor_over_panel()) return false;
    
    // Get actual mouse position for button detection (not custom cursor)
    Input *input = Input::get_singleton();
    Viewport *viewport = get_viewport();
    Vector2 mouse_pos = viewport ? viewport->get_mouse_position() : cursor_position;
    
    // Check toggle button
    if (bottom_panel_toggle_btn) {
        Rect2 btn_rect = bottom_panel_toggle_btn->get_global_rect();
        if (btn_rect.has_point(mouse_pos) || btn_rect.has_point(cursor_position)) {
            toggle_bottom_panel();
            return true;
        }
    }
    
    // Check build buttons - use both mouse_pos and cursor_position for reliability
    if (build_power_btn && build_power_btn->is_visible()) {
        Rect2 btn_rect = build_power_btn->get_global_rect();
        if (btn_rect.has_point(mouse_pos) || btn_rect.has_point(cursor_position)) {
            on_build_power_pressed();
            return true;
        }
    }
    if (build_barracks_btn && build_barracks_btn->is_visible()) {
        Rect2 btn_rect = build_barracks_btn->get_global_rect();
        if (btn_rect.has_point(mouse_pos) || btn_rect.has_point(cursor_position)) {
            on_build_barracks_pressed();
            return true;
        }
    }
    if (build_bulldozer_btn && build_bulldozer_btn->is_visible()) {
        Rect2 btn_rect = build_bulldozer_btn->get_global_rect();
        if (btn_rect.has_point(mouse_pos) || btn_rect.has_point(cursor_position)) {
            on_build_bulldozer_pressed();
            return true;
        }
    }
    if (train_unit_btn && train_unit_btn->is_visible()) {
        Rect2 btn_rect = train_unit_btn->get_global_rect();
        if (btn_rect.has_point(mouse_pos) || btn_rect.has_point(cursor_position)) {
            on_train_unit_pressed();
            return true;
        }
    }
    
    // Click was on panel but not on any button - still block game interaction
    return true;
}

void RTSCamera::update_unit_info_panel() {
    if (!unit_info_name || !unit_info_health || !unit_info_attack || !unit_info_position) return;
    
    if (selected_bulldozer) {
        // Update labels with selected bulldozer info
        unit_info_name->set_text(String("Vehicle: ") + selected_bulldozer->get_vehicle_name());
        
        String health_text = String("Health: ") + String::num_int64(selected_bulldozer->get_health()) + 
                            String(" / ") + String::num_int64(selected_bulldozer->get_max_health());
        unit_info_health->set_text(health_text);
        
        if (selected_bulldozer->get_is_constructing()) {
            float progress = selected_bulldozer->get_construction_progress() * 100.0f;
            unit_info_attack->set_text(String("Building: ") + String::num(progress, 0) + String("%"));
        } else {
            unit_info_attack->set_text("Ready to build");
        }
        
        Vector3 pos = selected_bulldozer->get_global_position();
        String pos_text = String("Position: (") + String::num(pos.x, 1) + String(", ") + 
                          String::num(pos.y, 1) + String(", ") + String::num(pos.z, 1) + String(")");
        unit_info_position->set_text(pos_text);
    } else if (selected_unit) {
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
    } else if (selected_building) {
        // Update labels with selected building info
        unit_info_name->set_text(String("Building: ") + selected_building->get_building_name());
        
        String health_text = String("Health: ") + String::num_int64(selected_building->get_health()) + 
                            String(" / ") + String::num_int64(selected_building->get_max_health());
        unit_info_health->set_text(health_text);
        
        String armor_text = String("Armor: ") + String::num_int64(selected_building->get_armor());
        unit_info_attack->set_text(armor_text);
        
        Vector3 pos = selected_building->get_global_position();
        String pos_text = String("Position: (") + String::num(pos.x, 1) + String(", ") + 
                          String::num(pos.y, 1) + String(", ") + String::num(pos.z, 1) + String(")");
        unit_info_position->set_text(pos_text);
    } else if (hovered_bulldozer) {
        unit_info_name->set_text(String("[Hover] ") + hovered_bulldozer->get_vehicle_name());
        
        String health_text = String("Health: ") + String::num_int64(hovered_bulldozer->get_health()) + 
                            String(" / ") + String::num_int64(hovered_bulldozer->get_max_health());
        unit_info_health->set_text(health_text);
        
        unit_info_attack->set_text("");
        unit_info_position->set_text("(Click to select)");
    } else if (hovered_unit) {
        // Show hovered unit info in dimmer style (preview)
        unit_info_name->set_text(String("[Hover] ") + hovered_unit->get_unit_name());
        
        String health_text = String("Health: ") + String::num_int64(hovered_unit->get_health()) + 
                            String(" / ") + String::num_int64(hovered_unit->get_max_health());
        unit_info_health->set_text(health_text);
        
        unit_info_attack->set_text("");
        unit_info_position->set_text("(Click to select)");
    } else if (hovered_building) {
        // Show hovered building info in dimmer style (preview)
        unit_info_name->set_text(String("[Hover] ") + hovered_building->get_building_name());
        
        String health_text = String("Health: ") + String::num_int64(hovered_building->get_health()) + 
                            String(" / ") + String::num_int64(hovered_building->get_max_health());
        unit_info_health->set_text(health_text);
        
        unit_info_attack->set_text("");
        unit_info_position->set_text("(Click to select)");
    } else {
        // No unit or building selected or hovered
        unit_info_name->set_text("No selection");
        unit_info_health->set_text("");
        unit_info_attack->set_text("");
        unit_info_position->set_text("");
    }
}

void RTSCamera::update_cursor_mode() {
    if (!cursor_sprite || !cursor_initialized) return;
    
    bool should_use_move_cursor = (selected_unit != nullptr) || 
                                  (selected_bulldozer != nullptr && !is_placing_building);
    
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
    int move_count = 0;
    
    // Move single selected unit
    if (selected_unit) {
        selected_unit->set_move_target(target);
        move_count++;
    }
    
    // Move all multi-selected units
    for (Unit *unit : selected_units) {
        if (unit && unit != selected_unit) {
            unit->set_move_target(target);
            move_count++;
        }
    }
    
    // Move single selected bulldozer
    if (selected_bulldozer) {
        selected_bulldozer->move_to(target);
        move_count++;
    }
    
    // Move all multi-selected bulldozers
    for (Bulldozer *bulldozer : selected_bulldozers) {
        if (bulldozer && bulldozer != selected_bulldozer) {
            bulldozer->move_to(target);
            move_count++;
        }
    }
    
    if (move_count > 0) {
        UtilityFunctions::print("Move order issued to ", move_count, " units at position: (", target.x, ", ", target.y, ", ", target.z, ")");
    }
}

Bulldozer* RTSCamera::raycast_for_bulldozer(const Vector2 &screen_pos) {
    Viewport *viewport = get_viewport();
    if (!viewport) return nullptr;
    
    Ref<World3D> world = get_world_3d();
    if (world.is_null()) return nullptr;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return nullptr;
    
    Vector3 ray_origin = project_ray_origin(screen_pos);
    Vector3 ray_direction = project_ray_normal(screen_pos);
    Vector3 ray_end = ray_origin + ray_direction * 1000.0f;
    
    // Layer 8 is vehicles/bulldozers
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(ray_origin, ray_end);
    query->set_collide_with_areas(false);
    query->set_collide_with_bodies(true);
    query->set_collision_mask(8);
    
    Dictionary result = space_state->intersect_ray(query);
    
    if (result.is_empty()) return nullptr;
    
    Object *collider = Object::cast_to<Object>(result["collider"]);
    if (!collider) return nullptr;
    
    Bulldozer *bulldozer = Object::cast_to<Bulldozer>(collider);
    if (bulldozer) return bulldozer;
    
    Node *parent = Object::cast_to<Node>(collider);
    if (parent) {
        parent = parent->get_parent();
        if (parent) {
            bulldozer = Object::cast_to<Bulldozer>(parent);
            if (bulldozer) return bulldozer;
        }
    }
    
    return nullptr;
}

void RTSCamera::select_bulldozer(Bulldozer *bulldozer) {
    // Deselect any selected unit or building first
    if (selected_unit) {
        selected_unit->set_selected(false);
        selected_unit = nullptr;
    }
    if (selected_building) {
        selected_building->set_selected(false);
        selected_building = nullptr;
    }
    
    // Clear multi-selection lists
    for (Unit *u : selected_units) {
        if (u) u->set_selected(false);
    }
    selected_units.clear();
    for (Bulldozer *b : selected_bulldozers) {
        if (b && b != bulldozer) b->set_selected(false);
    }
    selected_bulldozers.clear();
    
    // Deselect previous bulldozer
    if (selected_bulldozer && selected_bulldozer != bulldozer) {
        selected_bulldozer->set_selected(false);
    }
    
    selected_bulldozer = bulldozer;
    if (selected_bulldozer) {
        selected_bulldozer->set_selected(true);
        UtilityFunctions::print("Selected bulldozer: ", selected_bulldozer->get_vehicle_name());
    }
}

void RTSCamera::setup_build_buttons() {
    if (!bottom_panel_container) return;
    
    float label_start_y = bottom_panel_header_height + 15;
    float btn_width = 120;
    float btn_height = 35;
    float btn_spacing = 10;
    
    // Build Power Plant button
    build_power_btn = memnew(Button);
    build_power_btn->set_text("Power Plant");
    build_power_btn->set_anchor(Side::SIDE_LEFT, 0.35f);
    build_power_btn->set_anchor(Side::SIDE_RIGHT, 0.35f);
    build_power_btn->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    build_power_btn->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    build_power_btn->set_offset(Side::SIDE_LEFT, 0);
    build_power_btn->set_offset(Side::SIDE_RIGHT, btn_width);
    build_power_btn->set_offset(Side::SIDE_TOP, label_start_y);
    build_power_btn->set_offset(Side::SIDE_BOTTOM, label_start_y + btn_height);
    build_power_btn->set_visible(false);
    build_power_btn->connect("pressed", Callable(this, "_on_build_power_pressed"));
    bottom_panel_container->add_child(build_power_btn);
    
    // Build Barracks button
    build_barracks_btn = memnew(Button);
    build_barracks_btn->set_text("Barracks");
    build_barracks_btn->set_anchor(Side::SIDE_LEFT, 0.35f);
    build_barracks_btn->set_anchor(Side::SIDE_RIGHT, 0.35f);
    build_barracks_btn->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    build_barracks_btn->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    build_barracks_btn->set_offset(Side::SIDE_LEFT, btn_width + btn_spacing);
    build_barracks_btn->set_offset(Side::SIDE_RIGHT, btn_width * 2 + btn_spacing);
    build_barracks_btn->set_offset(Side::SIDE_TOP, label_start_y);
    build_barracks_btn->set_offset(Side::SIDE_BOTTOM, label_start_y + btn_height);
    build_barracks_btn->set_visible(false);
    build_barracks_btn->connect("pressed", Callable(this, "_on_build_barracks_pressed"));
    bottom_panel_container->add_child(build_barracks_btn);
    
    // Train Unit button (for barracks)
    train_unit_btn = memnew(Button);
    train_unit_btn->set_text("Train Soldier");
    train_unit_btn->set_anchor(Side::SIDE_LEFT, 0.35f);
    train_unit_btn->set_anchor(Side::SIDE_RIGHT, 0.35f);
    train_unit_btn->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    train_unit_btn->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    train_unit_btn->set_offset(Side::SIDE_LEFT, 0);
    train_unit_btn->set_offset(Side::SIDE_RIGHT, btn_width);
    train_unit_btn->set_offset(Side::SIDE_TOP, label_start_y);
    train_unit_btn->set_offset(Side::SIDE_BOTTOM, label_start_y + btn_height);
    train_unit_btn->set_visible(false);
    train_unit_btn->connect("pressed", Callable(this, "_on_train_unit_pressed"));
    bottom_panel_container->add_child(train_unit_btn);
    
    // Build Bulldozer button (for Command Center)
    build_bulldozer_btn = memnew(Button);
    build_bulldozer_btn->set_text("Build Bulldozer");
    build_bulldozer_btn->set_anchor(Side::SIDE_LEFT, 0.35f);
    build_bulldozer_btn->set_anchor(Side::SIDE_RIGHT, 0.35f);
    build_bulldozer_btn->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    build_bulldozer_btn->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    build_bulldozer_btn->set_offset(Side::SIDE_LEFT, 0);
    build_bulldozer_btn->set_offset(Side::SIDE_RIGHT, btn_width);
    build_bulldozer_btn->set_offset(Side::SIDE_TOP, label_start_y);
    build_bulldozer_btn->set_offset(Side::SIDE_BOTTOM, label_start_y + btn_height);
    build_bulldozer_btn->set_visible(false);
    build_bulldozer_btn->connect("pressed", Callable(this, "_on_build_bulldozer_pressed"));
    bottom_panel_container->add_child(build_bulldozer_btn);
    
    // Construction progress label
    construction_progress_label = memnew(Label);
    construction_progress_label->set_text("");
    construction_progress_label->set_anchor(Side::SIDE_LEFT, 0.35f);
    construction_progress_label->set_anchor(Side::SIDE_RIGHT, 0.65f);
    construction_progress_label->set_anchor(Side::SIDE_TOP, 1.0f - bottom_panel_height_percent);
    construction_progress_label->set_anchor(Side::SIDE_BOTTOM, 1.0f - bottom_panel_height_percent);
    construction_progress_label->set_offset(Side::SIDE_LEFT, 0);
    construction_progress_label->set_offset(Side::SIDE_RIGHT, 0);
    construction_progress_label->set_offset(Side::SIDE_TOP, label_start_y + btn_height + 10);
    construction_progress_label->set_offset(Side::SIDE_BOTTOM, label_start_y + btn_height + 35);
    construction_progress_label->add_theme_color_override("font_color", Color(0.9f, 0.9f, 0.5f, 1.0f));
    construction_progress_label->set_visible(false);
    bottom_panel_container->add_child(construction_progress_label);
}

void RTSCamera::update_build_buttons() {
    if (!build_power_btn || !build_barracks_btn || !train_unit_btn || !build_bulldozer_btn || !construction_progress_label) return;
    
    // Hide all buttons by default
    build_power_btn->set_visible(false);
    build_barracks_btn->set_visible(false);
    build_bulldozer_btn->set_visible(false);
    train_unit_btn->set_visible(false);
    construction_progress_label->set_visible(false);
    
    // Show build buttons if bulldozer is selected
    if (selected_bulldozer && bottom_panel_expanded) {
        if (selected_bulldozer->get_is_constructing()) {
            // Show construction progress
            construction_progress_label->set_visible(true);
            float progress = selected_bulldozer->get_construction_progress() * 100.0f;
            construction_progress_label->set_text(String("Constructing... ") + String::num(progress, 0) + String("%"));
        } else if (!is_placing_building) {
            // Show build buttons
            build_power_btn->set_visible(true);
            build_barracks_btn->set_visible(true);
        } else {
            // Show placement instruction
            construction_progress_label->set_visible(true);
            construction_progress_label->set_text("Click to place building, Right-click to cancel");
        }
    }
    
    // Show buttons based on selected building type
    if (selected_building && bottom_panel_expanded) {
        if (selected_building->get_building_name() == "Barracks") {
            train_unit_btn->set_visible(true);
        } else if (selected_building->get_building_name() == "Command Center") {
            build_bulldozer_btn->set_visible(true);
        }
    }
}

void RTSCamera::on_build_power_pressed() {
    if (!selected_bulldozer) return;
    start_building_placement(0);  // 0 = Power Plant
}

void RTSCamera::on_build_barracks_pressed() {
    if (!selected_bulldozer) return;
    start_building_placement(1);  // 1 = Barracks
}

void RTSCamera::on_build_bulldozer_pressed() {
    if (!selected_building) return;
    if (selected_building->get_building_name() != "Command Center") return;
    
    // Get Command Center position
    Vector3 cc_pos = selected_building->get_global_position();
    Vector3 spawn_pos = cc_pos + Vector3(8, 5.0f, 0); // Spawn to the right of Command Center
    
    // Create a new bulldozer
    Bulldozer *bulldozer = memnew(Bulldozer);
    bulldozer->set_vehicle_name("Construction Dozer");
    bulldozer->set_model_path("res://assets/vehicles/bulldozer/bulldozer.glb");
    bulldozer->set_model_scale(1.5f);
    bulldozer->set_power_plant_model("res://assets/buildings/power/power.glb");
    
    // Add to scene first, then set position
    get_tree()->get_root()->add_child(bulldozer);
    bulldozer->set_global_position(spawn_pos);
    
    // Snap to floor
    FloorSnapConfig config;
    config.floor_collision_mask = 1;
    config.raycast_start_height = 10.0f;
    config.raycast_max_distance = 50.0f;
    config.ground_offset = 0.0f;
    
    FloorSnapResult result = FloorSnapper::snap_to_floor(bulldozer, config);
    if (result.success) {
        UtilityFunctions::print("Bulldozer snapped to floor at Y=", result.final_y);
    }
    
    UtilityFunctions::print("Built new Bulldozer from Command Center");
}

void RTSCamera::on_train_unit_pressed() {
    if (!selected_building) return;
    if (selected_building->get_building_name() != "Barracks") return;
    
    // Get barracks position first
    Vector3 barracks_pos = selected_building->get_global_position();
    Vector3 spawn_pos = barracks_pos + Vector3(4, 5.0f, 4); // Start high, will snap down
    
    // Spawn a unit at the barracks location
    Unit *unit = memnew(Unit);
    unit->set_unit_name("Soldier");
    
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
    mesh_instance->set_position(Vector3(0, 0.5f, 0));
    
    // Set material
    Ref<StandardMaterial3D> material;
    material.instantiate();
    material->set_albedo(Color(0.3f, 0.3f, 0.8f));
    mesh_instance->set_surface_override_material(0, material);
    unit->add_child(mesh_instance);
    
    // Add to scene FIRST, then set position
    get_tree()->get_root()->add_child(unit);
    unit->set_global_position(spawn_pos);
    
    // Set collision layers
    unit->set_collision_layer(2);
    unit->set_collision_mask(1);
    
    // Snap unit to floor
    FloorSnapConfig config;
    config.floor_collision_mask = 1; // Ground layer
    config.raycast_start_height = 10.0f;
    config.raycast_max_distance = 50.0f;
    config.ground_offset = 0.0f;
    
    FloorSnapResult result = FloorSnapper::snap_to_floor(unit, config);
    if (result.success) {
        UtilityFunctions::print("Unit snapped to floor at Y=", result.final_y);
    }
    
    UtilityFunctions::print("Trained new soldier at barracks");
}

void RTSCamera::start_building_placement(int type) {
    if (!selected_bulldozer) return;
    
    // Cancel any existing building placement first
    if (is_placing_building) {
        cancel_building_placement();
    }
    
    is_placing_building = true;
    placing_building_type = type;
    selected_bulldozer->start_placing_building(type);
    
    UtilityFunctions::print("Started building placement, type: ", type);
}

void RTSCamera::cancel_building_placement() {
    if (!is_placing_building) return;
    
    is_placing_building = false;
    placing_building_type = -1;
    
    if (selected_bulldozer) {
        selected_bulldozer->cancel_placing();
    }
    
    UtilityFunctions::print("Cancelled building placement");
}

void RTSCamera::confirm_building_placement() {
    if (!is_placing_building || !selected_bulldozer) return;
    
    Vector3 ground_pos = raycast_ground(cursor_position);
    selected_bulldozer->confirm_build_location(ground_pos);
    
    is_placing_building = false;
    placing_building_type = -1;
    
    UtilityFunctions::print("Confirmed building placement at: ", ground_pos);
}

} // namespace rts
