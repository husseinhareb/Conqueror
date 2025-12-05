/**
 * Unit.h
 * RTS unit class with movement, selection state, and flow field integration.
 * Supports receiving move orders and smooth steering movement.
 * Includes collision avoidance for buildings, other units, and vehicles.
 */

#ifndef UNIT_H
#define UNIT_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class Unit : public godot::CharacterBody3D {
    GDCLASS(Unit, godot::CharacterBody3D)

private:
    // Movement settings
    float move_speed = 8.0f;
    float base_move_speed = 8.0f;         // Store original speed for slope calculations
    float acceleration = 15.0f;
    float deceleration = 20.0f;
    float steering_strength = 5.0f;
    float arrival_threshold = 0.5f;
    
    // Terrain following
    float uphill_speed_multiplier = 0.5f;   // Speed multiplier when going uphill
    float downhill_speed_multiplier = 1.4f; // Speed multiplier when going downhill
    float current_slope = 0.0f;             // Current terrain slope (-1 to 1, negative = downhill)
    float terrain_height = 0.0f;            // Current terrain height
    float last_terrain_height = 0.0f;       // Previous frame terrain height
    
    // Collision avoidance settings
    float avoidance_radius = 5.0f;        // How far ahead to look for obstacles
    float avoidance_strength = 15.0f;     // How strongly to avoid obstacles
    float separation_radius = 1.2f;       // Minimum distance from other units
    float separation_strength = 10.0f;    // How strongly to separate from other units
    float wall_follow_distance = 2.0f;    // Distance to maintain when following walls
    uint32_t obstacle_mask = 0b0100;      // Layer 4: Buildings only (for steering avoidance)
    
    // Pathfinding state
    bool is_avoiding = false;             // Currently avoiding an obstacle
    float avoid_direction = 0.0f;         // -1 = left, 1 = right
    float stuck_timer = 0.0f;             // Time spent possibly stuck
    godot::Vector3 last_position;         // For stuck detection
    
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
    
    // Cached references for performance
    godot::Node *cached_terrain_generator = nullptr;
    
    // Cached physics shapes (avoid per-frame allocations)
    godot::Ref<godot::SphereShape3D> cached_separation_sphere;
    godot::Ref<godot::PhysicsShapeQueryParameters3D> cached_shape_query;
    godot::Ref<godot::PhysicsRayQueryParameters3D> cached_ray_query;

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
    
    // Collision avoidance
    godot::Vector3 calculate_avoidance_force();
    godot::Vector3 calculate_separation_force();
    godot::Vector3 find_clear_direction(const godot::Vector3 &preferred_dir);
    bool check_path_blocked(const godot::Vector3 &direction, float distance);
    float raycast_distance(const godot::Vector3 &direction, float max_distance);
    void update_stuck_detection(double delta);
    void snap_to_terrain();

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
