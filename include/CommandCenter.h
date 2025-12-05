/**
 * CommandCenter.h
 * Main player headquarters building with extreme detail.
 * Features an animated garage door, rotating radar, and bulldozer spawning.
 */

#ifndef COMMAND_CENTER_H
#define COMMAND_CENTER_H

#include "Building.h"

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <vector>

namespace rts {

class Bulldozer;

class CommandCenter : public Building {
    GDCLASS(CommandCenter, Building)

private:
    // Building dimensions - MUCH LARGER complex
    static constexpr float BASE_WIDTH = 28.0f;    // Much wider
    static constexpr float BASE_DEPTH = 24.0f;    // Much deeper
    static constexpr float BASE_HEIGHT = 7.0f;    // Taller
    static constexpr float GARAGE_HEIGHT = 5.0f;  // Bigger garage
    static constexpr float RADAR_HEIGHT = 5.0f;   // Taller radar
    static constexpr float TOWER_HEIGHT = 12.0f;  // Command tower
    
    // Component nodes
    godot::Node3D *building_root = nullptr;
    
    // Main structure parts
    godot::MeshInstance3D *main_building = nullptr;
    godot::MeshInstance3D *garage_section = nullptr;
    godot::MeshInstance3D *roof = nullptr;
    godot::MeshInstance3D *control_tower = nullptr;
    
    // Garage door components (for animation)
    godot::Node3D *garage_door_pivot = nullptr;
    godot::MeshInstance3D *garage_door = nullptr;
    std::vector<godot::MeshInstance3D*> door_segments;
    bool door_is_open = false;
    float door_open_amount = 0.0f;  // 0 = closed, 1 = open
    float door_animation_speed = 2.0f;
    bool door_animating = false;
    bool door_target_open = false;
    
    // Radar components
    godot::Node3D *radar_pivot = nullptr;
    godot::MeshInstance3D *radar_base = nullptr;
    godot::MeshInstance3D *radar_dish = nullptr;
    godot::MeshInstance3D *radar_arm = nullptr;
    float radar_rotation = 0.0f;
    float radar_rotation_speed = 30.0f;  // Degrees per second
    
    // Detail meshes
    std::vector<godot::MeshInstance3D*> detail_meshes;
    godot::MeshInstance3D *antenna1 = nullptr;
    godot::MeshInstance3D *antenna2 = nullptr;
    godot::MeshInstance3D *ac_unit = nullptr;
    godot::MeshInstance3D *exhaust_vent = nullptr;
    godot::MeshInstance3D *satellite_dish = nullptr;
    godot::MeshInstance3D *flag_pole = nullptr;
    godot::MeshInstance3D *lights = nullptr;
    godot::MeshInstance3D *helipad = nullptr;
    godot::Node3D *comm_array = nullptr;
    godot::Node3D *power_section = nullptr;
    godot::Node3D *fuel_section = nullptr;
    
    // Materials
    godot::Ref<godot::StandardMaterial3D> main_material;
    godot::Ref<godot::StandardMaterial3D> metal_material;
    godot::Ref<godot::StandardMaterial3D> dark_metal_material;
    godot::Ref<godot::StandardMaterial3D> window_material;
    godot::Ref<godot::StandardMaterial3D> door_material;
    godot::Ref<godot::StandardMaterial3D> accent_material;
    godot::Ref<godot::StandardMaterial3D> radar_material;
    godot::Ref<godot::StandardMaterial3D> light_material;
    godot::Ref<godot::StandardMaterial3D> concrete_material;
    godot::Ref<godot::StandardMaterial3D> caution_material;
    godot::Ref<godot::StandardMaterial3D> red_light_material;
    godot::Ref<godot::StandardMaterial3D> green_light_material;
    godot::Ref<godot::StandardMaterial3D> fuel_tank_material;
    
    // Bulldozer spawning
    godot::Vector3 spawn_point;
    bool has_spawned_initial_bulldozer = false;
    float spawn_delay_timer = 0.0f;
    bool spawn_pending = false;
    
    // Build queue
    int bulldozer_queue = 0;
    float build_progress = 0.0f;
    float bulldozer_build_time = 8.0f;
    bool is_building_bulldozer = false;

protected:
    static void _bind_methods();

public:
    CommandCenter();
    ~CommandCenter();

    void _ready() override;
    void _process(double delta) override;
    
    // Building creation
    void create_command_center();
    void create_materials();
    
    // Structure components
    void create_main_building();
    void create_garage_section();
    void create_control_tower();
    void create_roof();
    void create_garage_door();
    void create_radar_system();
    void create_details();
    void create_windows();
    void create_foundation();
    void create_helipad();
    void create_communications_array();
    void create_power_generators();
    void create_fuel_depot();
    void create_perimeter_walls();
    void create_guard_posts();
    void create_landing_lights();
    void create_vehicle_bay();
    
    // Mesh generation helpers
    godot::Ref<godot::ArrayMesh> create_box_mesh(float width, float height, float depth);
    godot::Ref<godot::ArrayMesh> create_cylinder_mesh(float radius, float height, int segments = 16);
    godot::Ref<godot::ArrayMesh> create_cone_mesh(float radius, float height, int segments = 16);
    godot::Ref<godot::ArrayMesh> create_radar_dish_mesh(float radius, float depth, int segments = 24);
    godot::Ref<godot::ArrayMesh> create_door_segment_mesh(float width, float height, float depth);
    
    // Door animation
    void open_garage_door();
    void close_garage_door();
    void toggle_garage_door();
    void update_door_animation(double delta);
    bool is_door_open() const;
    bool is_door_animating() const;
    
    // Radar animation
    void update_radar_rotation(double delta);
    void set_radar_speed(float speed);
    float get_radar_speed() const;
    
    // Bulldozer spawning
    void spawn_bulldozer();
    void queue_bulldozer();
    void update_bulldozer_production(double delta);
    godot::Vector3 get_spawn_point() const;
    godot::Vector3 get_rally_point() const;
    
    // Getters
    int get_bulldozer_queue() const;
    float get_build_progress() const;
    bool get_is_building() const;
};

} // namespace rts

#endif // COMMAND_CENTER_H
