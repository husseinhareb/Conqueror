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
    
    // Walking animation
    float walk_bob_amount = 0.08f;  // Vertical bobbing
    float walk_bob_speed = 12.0f;   // Bob frequency
    float walk_sway_amount = 0.02f; // Side-to-side sway
    float walk_time = 0.0f;         // Animation timer
    float base_y_offset = 0.0f;     // Original Y offset
    
    // State
    bool is_selected = false;
    bool is_hovered = false;
    bool has_move_order = false;
    godot::Vector3 target_position;
    godot::Vector3 current_velocity;
    
    // Flow field movement
    godot::Vector3 flow_vector;
    bool use_flow_field = true;
    
    // Visual feedback
    int unit_id = -1;
    
    // Unit stats (for display)
    godot::String unit_name = "Soldier";
    int health = 100;
    int max_health = 100;
    int attack_damage = 10;
    float attack_range = 5.0f;

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
    void update_walk_animation(double delta);
    godot::Vector3 calculate_steering(const godot::Vector3 &desired_velocity) const;

    // Selection
    void set_selected(bool selected);
    bool get_selected() const;
    
    // Hover
    void set_hovered(bool hovered);
    bool get_hovered() const;
    
    void update_selection_visual();
    void update_hover_visual();

    // Getters/Setters
    void set_move_speed(float speed);
    float get_move_speed() const;
    
    void set_unit_id(int id);
    int get_unit_id() const;
    
    void set_unit_name(const godot::String &name);
    godot::String get_unit_name() const;
    
    void set_health(int hp);
    int get_health() const;
    
    void set_max_health(int hp);
    int get_max_health() const;
    
    void set_attack_damage(int damage);
    int get_attack_damage() const;
    
    void set_attack_range(float range);
    float get_attack_range() const;
    
    bool is_moving() const;
    godot::Vector3 get_target_position() const;

    // Signals
    // signal unit_selected(unit: Unit)
    // signal unit_deselected(unit: Unit)
    // signal unit_arrived(unit: Unit)
};

} // namespace rts

#endif // UNIT_H
