/**
 * SelectionManager.h
 * Handles unit selection via clicks and drag rectangle.
 * Left click = single select, drag = multi-select, right click = move order.
 */

#ifndef SELECTION_MANAGER_H
#define SELECTION_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class Unit;
class FlowFieldManager;

class SelectionManager : public godot::Node {
    GDCLASS(SelectionManager, godot::Node)

private:
    // References
    godot::Camera3D *camera = nullptr;
    godot::Control *selection_rect_ui = nullptr;
    FlowFieldManager *flow_field_manager = nullptr;
    
    // Selection state
    godot::Vector<Unit*> selected_units;
    godot::Vector<Unit*> all_units;
    
    // Drag selection
    bool is_dragging = false;
    godot::Vector2 drag_start;
    godot::Vector2 drag_current;
    float drag_threshold = 5.0f;
    
    // Raycast settings
    uint32_t unit_collision_layer = 2;
    uint32_t ground_collision_layer = 1;

protected:
    static void _bind_methods();

public:
    SelectionManager();
    ~SelectionManager();

    void _ready() override;
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

    // Unit registration
    void register_unit(Unit *unit);
    void unregister_unit(Unit *unit);
    
    // Selection
    void select_unit(Unit *unit, bool add_to_selection = false);
    void deselect_unit(Unit *unit);
    void deselect_all();
    void select_units_in_rect(const godot::Rect2 &screen_rect);
    
    godot::Vector<Unit*> get_selected_units() const;
    int get_selected_count() const;

    // Input handling
    void handle_left_click(const godot::Vector2 &position, bool shift_held);
    void handle_right_click(const godot::Vector2 &position);
    void handle_drag_start(const godot::Vector2 &position);
    void handle_drag_update(const godot::Vector2 &position);
    void handle_drag_end();
    
    // Raycasting
    Unit* raycast_for_unit(const godot::Vector2 &screen_pos);
    godot::Vector3 raycast_for_ground(const godot::Vector2 &screen_pos);
    
    // UI
    void update_selection_rect_ui();
    void show_selection_rect(bool visible);
    
    // Move orders
    void issue_move_order(const godot::Vector3 &target);

    // Setters
    void set_camera(godot::Camera3D *cam);
    void set_selection_rect_ui(godot::Control *rect);
    void set_flow_field_manager(FlowFieldManager *manager);
};

} // namespace rts

#endif // SELECTION_MANAGER_H
