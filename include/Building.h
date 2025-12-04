/**
 * Building.h
 * RTS building class - static structures that can be selected.
 * Buildings are large square structures with collision detection for placement validation.
 */

#ifndef BUILDING_H
#define BUILDING_H

#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class Building : public godot::StaticBody3D {
    GDCLASS(Building, godot::StaticBody3D)

private:
    // Building properties
    godot::String building_name = "Building";
    godot::String model_path = "";  // Path to .glb model (empty = use default box)
    float model_scale = 1.0f;        // Scale for the loaded model
    int health = 500;
    int max_health = 500;
    float building_size = 4.0f;  // Size of the square base
    float building_height = 3.0f;
    
    // Placement validation
    bool is_placement_valid = true;
    bool is_preview_mode = false;
    uint32_t placement_check_mask = 0b1110; // Units(2), Buildings(4), Vehicles(8)
    
    // State
    bool is_selected = false;
    bool is_hovered = false;
    bool is_constructed = true;  // For future construction system
    
    // Visual feedback
    int building_id = -1;
    
    // Building stats
    int armor = 5;
    
    // Child nodes (created at runtime)
    godot::MeshInstance3D *mesh_instance = nullptr;
    godot::CollisionShape3D *collision_shape = nullptr;
    godot::Node3D *model_instance = nullptr;  // For loaded 3D models

protected:
    static void _bind_methods();

public:
    Building();
    ~Building();

    void _ready() override;
    void _process(double delta) override;
    
    // Setup
    void create_building_mesh();
    void load_model();
    void setup_collision();
    void snap_to_terrain();
    void find_mesh_instances_recursive(godot::Node *node);
    void apply_selection_to_model(godot::Node *node, bool selected, bool hovered);
    void apply_placement_color_to_model(godot::Node *node, bool valid);

    // Selection
    void set_selected(bool selected);
    bool get_selected() const;
    
    // Hover
    void set_hovered(bool hovered);
    bool get_hovered() const;
    
    void update_selection_visual();
    void update_hover_visual();

    // Getters/Setters
    void set_building_name(const godot::String &name);
    godot::String get_building_name() const;
    
    void set_health(int hp);
    int get_health() const;
    
    void set_max_health(int hp);
    int get_max_health() const;
    
    void set_building_size(float size);
    float get_building_size() const;
    
    void set_building_height(float height);
    float get_building_height() const;
    
    void set_building_id(int id);
    int get_building_id() const;
    
    void set_armor(int value);
    int get_armor() const;
    
    void set_model_path(const godot::String &path);
    godot::String get_model_path() const;
    
    void set_model_scale(float scale);
    float get_model_scale() const;
    
    // Placement validation
    bool can_place_at(const godot::Vector3 &position);
    bool check_placement_valid();
    void set_preview_mode(bool preview);
    bool get_preview_mode() const;
    void update_preview_visual();
    void notify_flow_field_of_placement();
    static bool is_position_valid_for_building(godot::Node *context, const godot::Vector3 &position, float size, uint32_t check_mask);

    // Signals
    // signal building_selected(building: Building)
    // signal building_deselected(building: Building)
    // signal building_destroyed(building: Building)
};

} // namespace rts

#endif // BUILDING_H
