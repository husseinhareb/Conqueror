/**
 * SelectionManager.cpp
 * Handles unit selection via clicks and drag rectangle.
 */

#include "SelectionManager.h"
#include "Unit.h"
#include "FlowFieldManager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void SelectionManager::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("register_unit", "unit"), &SelectionManager::register_unit);
    ClassDB::bind_method(D_METHOD("unregister_unit", "unit"), &SelectionManager::unregister_unit);
    ClassDB::bind_method(D_METHOD("select_unit", "unit", "add_to_selection"), &SelectionManager::select_unit, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("deselect_unit", "unit"), &SelectionManager::deselect_unit);
    ClassDB::bind_method(D_METHOD("deselect_all"), &SelectionManager::deselect_all);
    ClassDB::bind_method(D_METHOD("get_selected_count"), &SelectionManager::get_selected_count);
    
    ClassDB::bind_method(D_METHOD("set_camera", "camera"), &SelectionManager::set_camera);
    ClassDB::bind_method(D_METHOD("set_selection_rect_ui", "rect"), &SelectionManager::set_selection_rect_ui);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "manager"), &SelectionManager::set_flow_field_manager);
    
    // Signals
    ADD_SIGNAL(MethodInfo("selection_changed", PropertyInfo(Variant::ARRAY, "selected_units")));
    ADD_SIGNAL(MethodInfo("move_order_issued", PropertyInfo(Variant::VECTOR3, "target")));
}

SelectionManager::SelectionManager() {
}

SelectionManager::~SelectionManager() {
}

void SelectionManager::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Try to find camera if not set
    if (!camera) {
        camera = Object::cast_to<Camera3D>(get_viewport()->get_camera_3d());
    }
}

void SelectionManager::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    if (is_dragging) {
        update_selection_rect_ui();
    }
}

void SelectionManager::_input(const Ref<InputEvent> &event) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    Ref<InputEventMouseButton> mouse_button = event;
    if (mouse_button.is_valid()) {
        Vector2 pos = mouse_button->get_position();
        
        if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_LEFT) {
            if (mouse_button->is_pressed()) {
                handle_drag_start(pos);
            } else {
                if (is_dragging && (drag_current - drag_start).length() > drag_threshold) {
                    handle_drag_end();
                } else {
                    bool shift = mouse_button->is_shift_pressed();
                    handle_left_click(pos, shift);
                }
                is_dragging = false;
                show_selection_rect(false);
            }
        } else if (mouse_button->get_button_index() == MouseButton::MOUSE_BUTTON_RIGHT) {
            if (mouse_button->is_pressed()) {
                handle_right_click(pos);
            }
        }
    }
    
    Ref<InputEventMouseMotion> mouse_motion = event;
    if (mouse_motion.is_valid() && is_dragging) {
        handle_drag_update(mouse_motion->get_position());
    }
}

void SelectionManager::register_unit(Unit *unit) {
    if (unit && all_units.find(unit) == -1) {
        all_units.push_back(unit);
    }
}

void SelectionManager::unregister_unit(Unit *unit) {
    int idx = all_units.find(unit);
    if (idx != -1) {
        all_units.remove_at(idx);
    }
    
    idx = selected_units.find(unit);
    if (idx != -1) {
        selected_units.remove_at(idx);
    }
}

void SelectionManager::select_unit(Unit *unit, bool add_to_selection) {
    if (!unit) return;
    
    if (!add_to_selection) {
        deselect_all();
    }
    
    if (selected_units.find(unit) == -1) {
        selected_units.push_back(unit);
        unit->set_selected(true);
    }
}

void SelectionManager::deselect_unit(Unit *unit) {
    if (!unit) return;
    
    int idx = selected_units.find(unit);
    if (idx != -1) {
        selected_units.remove_at(idx);
        unit->set_selected(false);
    }
}

void SelectionManager::deselect_all() {
    for (int i = 0; i < selected_units.size(); i++) {
        if (selected_units[i]) {
            selected_units[i]->set_selected(false);
        }
    }
    selected_units.clear();
}

void SelectionManager::select_units_in_rect(const Rect2 &screen_rect) {
    if (!camera) return;
    
    deselect_all();
    
    for (int i = 0; i < all_units.size(); i++) {
        Unit *unit = all_units[i];
        if (!unit) continue;
        
        // Project unit position to screen
        Vector3 world_pos = unit->get_global_position();
        
        if (camera->is_position_behind(world_pos)) {
            continue;
        }
        
        Vector2 screen_pos = camera->unproject_position(world_pos);
        
        if (screen_rect.has_point(screen_pos)) {
            selected_units.push_back(unit);
            unit->set_selected(true);
        }
    }
}

Vector<Unit*> SelectionManager::get_selected_units() const {
    return selected_units;
}

int SelectionManager::get_selected_count() const {
    return selected_units.size();
}

void SelectionManager::handle_left_click(const Vector2 &position, bool shift_held) {
    Unit *clicked_unit = raycast_for_unit(position);
    
    if (clicked_unit) {
        select_unit(clicked_unit, shift_held);
    } else if (!shift_held) {
        deselect_all();
    }
}

void SelectionManager::handle_right_click(const Vector2 &position) {
    if (selected_units.size() == 0) {
        return;
    }
    
    Vector3 target = raycast_for_ground(position);
    
    if (target != Vector3(0, -1000, 0)) { // Check for valid hit
        issue_move_order(target);
    }
}

void SelectionManager::handle_drag_start(const Vector2 &position) {
    is_dragging = true;
    drag_start = position;
    drag_current = position;
    show_selection_rect(true);
}

void SelectionManager::handle_drag_update(const Vector2 &position) {
    drag_current = position;
    update_selection_rect_ui();
}

void SelectionManager::handle_drag_end() {
    // Create selection rectangle
    Vector2 top_left = Vector2(
        Math::min(drag_start.x, drag_current.x),
        Math::min(drag_start.y, drag_current.y)
    );
    Vector2 size = Vector2(
        Math::abs(drag_current.x - drag_start.x),
        Math::abs(drag_current.y - drag_start.y)
    );
    
    Rect2 selection_rect = Rect2(top_left, size);
    select_units_in_rect(selection_rect);
    
    show_selection_rect(false);
}

Unit* SelectionManager::raycast_for_unit(const Vector2 &screen_pos) {
    if (!camera) return nullptr;
    
    Vector3 from = camera->project_ray_origin(screen_pos);
    Vector3 to = from + camera->project_ray_normal(screen_pos) * 1000.0f;
    
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return nullptr;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return nullptr;
    
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(from, to);
    query->set_collision_mask(unit_collision_layer);
    
    Dictionary result = space_state->intersect_ray(query);
    
    if (!result.is_empty()) {
        Object *collider = Object::cast_to<Object>(result["collider"]);
        if (collider) {
            // Check if collider is a Unit or parent is a Unit
            Unit *unit = Object::cast_to<Unit>(collider);
            if (unit) return unit;
            
            Node *parent = Object::cast_to<Node>(collider);
            while (parent) {
                unit = Object::cast_to<Unit>(parent);
                if (unit) return unit;
                parent = parent->get_parent();
            }
        }
    }
    
    return nullptr;
}

Vector3 SelectionManager::raycast_for_ground(const Vector2 &screen_pos) {
    if (!camera) return Vector3(0, -1000, 0);
    
    Vector3 from = camera->project_ray_origin(screen_pos);
    Vector3 to = from + camera->project_ray_normal(screen_pos) * 1000.0f;
    
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return Vector3(0, -1000, 0);
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return Vector3(0, -1000, 0);
    
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(from, to);
    query->set_collision_mask(ground_collision_layer);
    
    Dictionary result = space_state->intersect_ray(query);
    
    if (!result.is_empty()) {
        return result["position"];
    }
    
    // Fallback: intersect with Y=0 plane
    Vector3 ray_dir = camera->project_ray_normal(screen_pos);
    if (Math::abs(ray_dir.y) > 0.001f) {
        float t = -from.y / ray_dir.y;
        if (t > 0) {
            return from + ray_dir * t;
        }
    }
    
    return Vector3(0, -1000, 0);
}

void SelectionManager::update_selection_rect_ui() {
    if (!selection_rect_ui) return;
    
    ColorRect *rect = Object::cast_to<ColorRect>(selection_rect_ui);
    if (!rect) return;
    
    Vector2 top_left = Vector2(
        Math::min(drag_start.x, drag_current.x),
        Math::min(drag_start.y, drag_current.y)
    );
    Vector2 size = Vector2(
        Math::abs(drag_current.x - drag_start.x),
        Math::abs(drag_current.y - drag_start.y)
    );
    
    rect->set_position(top_left);
    rect->set_size(size);
}

void SelectionManager::show_selection_rect(bool visible) {
    if (selection_rect_ui) {
        selection_rect_ui->set_visible(visible);
    }
}

void SelectionManager::issue_move_order(const Vector3 &target) {
    // Compute flow field if manager is available
    if (flow_field_manager) {
        flow_field_manager->compute_flow_field(target);
    }
    
    // Issue move orders to all selected units
    for (int i = 0; i < selected_units.size(); i++) {
        Unit *unit = selected_units[i];
        if (unit) {
            unit->set_move_target(target);
            
            // Apply flow vector if available
            if (flow_field_manager) {
                Vector3 flow = flow_field_manager->get_flow_direction(unit->get_global_position());
                unit->apply_flow_vector(flow);
            }
        }
    }
    
    emit_signal("move_order_issued", target);
}

void SelectionManager::set_camera(Camera3D *cam) {
    camera = cam;
}

void SelectionManager::set_selection_rect_ui(Control *rect) {
    selection_rect_ui = rect;
}

void SelectionManager::set_flow_field_manager(FlowFieldManager *manager) {
    flow_field_manager = manager;
}

} // namespace rts
