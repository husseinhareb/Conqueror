/**
 * Unit.h
 * RTS unit class with movement, selection state, and flow field integration.
 * Supports receiving move orders and smooth steering movement.
 */

#ifndef UNIT_H
#define UNIT_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class Unit : public godot::CharacterBody3D {
    GDCLASS(Unit, godot::CharacterBody3D)

private:
    // Movement settings
    float move_speed = 8.0f;
    float acceleration = 15.0f;
    float deceleration = 20.0f;
    float steering_strength = 5.0f;
    float arrival_threshold = 0.5f;
    
    // State
    bool is_selected = false;
    bool has_move_order = false;
    godot::Vector3 target_position;
    godot::Vector3 current_velocity;
    
    // Flow field movement
    godot::Vector3 flow_vector;
    bool use_flow_field = true;
    
    // Visual feedback
    int unit_id = -1;

protected:
    static void _bind_methods();

public:
    Unit();
    ~Unit();

    void _ready() override;
    void _physics_process(double delta) override;

    // Movement
    void set_move_target(const godot::Vector3 &target);
    void apply_flow_vector(const godot::Vector3 &vector);
    void stop_movement();
    
    void update_movement(double delta);
    godot::Vector3 calculate_steering(const godot::Vector3 &desired_velocity) const;

    // Selection
    void set_selected(bool selected);
    bool get_selected() const;
    
    void update_selection_visual();

    // Getters/Setters
    void set_move_speed(float speed);
    float get_move_speed() const;
    
    void set_unit_id(int id);
    int get_unit_id() const;
    
    bool is_moving() const;
    godot::Vector3 get_target_position() const;

    // Signals
    // signal unit_selected(unit: Unit)
    // signal unit_deselected(unit: Unit)
    // signal unit_arrived(unit: Unit)
};

} // namespace rts

#endif // UNIT_H
