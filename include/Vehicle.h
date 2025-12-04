/**
 * Vehicle.h
 * RTS vehicle class - movable units that can perform special actions.
 * Base class for Bulldozer, tanks, etc.
 */

#ifndef VEHICLE_H
#define VEHICLE_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class Vehicle : public godot::CharacterBody3D {
    GDCLASS(Vehicle, godot::CharacterBody3D)

protected:
    // Vehicle properties
    godot::String vehicle_name = "Vehicle";
    int health = 200;
    int max_health = 200;
    float move_speed = 4.0f;
    float turn_speed = 3.0f;
    
    // State
    bool is_selected = false;
    bool is_hovered = false;
    bool is_moving = false;
    
    // Movement
    godot::Vector3 target_position;
    godot::Vector3 current_velocity;
    
    // Visual
    int vehicle_id = -1;
    godot::MeshInstance3D *mesh_instance = nullptr;
    godot::CollisionShape3D *collision_shape = nullptr;
    godot::Node3D *model_instance = nullptr;
    
    // Model
    godot::String model_path = "";
    float model_scale = 1.0f;

    static void _bind_methods();

public:
    Vehicle();
    ~Vehicle();

    void _ready() override;
    void _process(double delta) override;
    void _physics_process(double delta) override;
    
    // Setup
    virtual void create_vehicle_mesh();
    void load_model();
    void setup_collision();
    void find_mesh_instances_recursive(godot::Node *node);

    // Movement
    void move_to(const godot::Vector3 &position);
    void stop_moving();
    bool get_is_moving() const;
    
    // Selection
    void set_selected(bool selected);
    bool get_selected() const;
    
    // Hover
    void set_hovered(bool hovered);
    bool get_hovered() const;
    
    void update_selection_visual();
    void update_hover_visual();
    void apply_visual_to_model(godot::Node *node, bool selected, bool hovered);

    // Getters/Setters
    void set_vehicle_name(const godot::String &name);
    godot::String get_vehicle_name() const;
    
    void set_health(int hp);
    int get_health() const;
    
    void set_max_health(int hp);
    int get_max_health() const;
    
    void set_move_speed(float speed);
    float get_move_speed() const;
    
    void set_vehicle_id(int id);
    int get_vehicle_id() const;
    
    void set_model_path(const godot::String &path);
    godot::String get_model_path() const;
    
    void set_model_scale(float scale);
    float get_model_scale() const;
};

} // namespace rts

#endif // VEHICLE_H
