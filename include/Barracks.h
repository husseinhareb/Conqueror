/**
 * Barracks.h
 * Military barracks building for training infantry units.
 * Features realistic military architecture with doors, windows, and training grounds.
 */

#ifndef BARRACKS_H
#define BARRACKS_H

#include "Building.h"

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <vector>

namespace rts {

class Unit;

class Barracks : public Building {
    GDCLASS(Barracks, Building)

private:
    // Building dimensions - large military barracks complex
    static constexpr float BUILDING_WIDTH = 24.0f;   // Much wider building
    static constexpr float BUILDING_DEPTH = 16.0f;   // Deeper building
    static constexpr float BUILDING_HEIGHT = 6.0f;   // Taller walls
    static constexpr float ROOF_HEIGHT = 2.5f;       // Higher roof
    static constexpr float FOUNDATION_HEIGHT = 0.5f; // Thicker foundation
    
    // Component nodes
    godot::Node3D *building_root = nullptr;
    
    // Main structure parts
    godot::MeshInstance3D *main_building = nullptr;
    godot::MeshInstance3D *roof_section = nullptr;
    godot::MeshInstance3D *foundation = nullptr;
    godot::MeshInstance3D *entrance_section = nullptr;
    godot::MeshInstance3D *porch_roof = nullptr;
    
    // Training area
    godot::MeshInstance3D *training_ground = nullptr;
    godot::MeshInstance3D *flag_pole = nullptr;
    godot::MeshInstance3D *sandbag_pile1 = nullptr;
    godot::MeshInstance3D *sandbag_pile2 = nullptr;
    
    // Door components
    godot::Node3D *door_pivot = nullptr;
    godot::MeshInstance3D *door_mesh = nullptr;
    bool door_is_open = false;
    float door_open_amount = 0.0f;
    float door_animation_speed = 2.0f;
    bool door_animating = false;
    bool door_target_open = false;
    
    // Detail meshes
    std::vector<godot::MeshInstance3D*> window_meshes;
    std::vector<godot::MeshInstance3D*> detail_meshes;
    godot::MeshInstance3D *chimney = nullptr;
    godot::MeshInstance3D *ac_unit = nullptr;
    godot::MeshInstance3D *spotlight = nullptr;
    
    // Materials with textures
    godot::Ref<godot::StandardMaterial3D> wall_material;
    godot::Ref<godot::StandardMaterial3D> brick_material;
    godot::Ref<godot::StandardMaterial3D> concrete_material;
    godot::Ref<godot::StandardMaterial3D> metal_material;
    godot::Ref<godot::StandardMaterial3D> dark_metal_material;
    godot::Ref<godot::StandardMaterial3D> window_material;
    godot::Ref<godot::StandardMaterial3D> door_material;
    godot::Ref<godot::StandardMaterial3D> roof_material;
    godot::Ref<godot::StandardMaterial3D> wood_material;
    godot::Ref<godot::StandardMaterial3D> canvas_material;
    godot::Ref<godot::StandardMaterial3D> ground_material;
    
    // Textures
    godot::Ref<godot::ImageTexture> brick_albedo_tex;
    godot::Ref<godot::ImageTexture> brick_normal_tex;
    godot::Ref<godot::ImageTexture> concrete_albedo_tex;
    godot::Ref<godot::ImageTexture> metal_albedo_tex;
    godot::Ref<godot::ImageTexture> roof_albedo_tex;
    godot::Ref<godot::ImageTexture> wood_albedo_tex;
    
    // Unit spawning
    godot::Vector3 spawn_point;
    godot::Vector3 rally_point;
    
    // Training queue
    int unit_queue = 0;
    float train_progress = 0.0f;
    float unit_train_time = 5.0f;
    bool is_training_unit = false;

protected:
    static void _bind_methods();

public:
    Barracks();
    ~Barracks();

    void _ready() override;
    void _process(double delta) override;
    
    // Building creation
    void create_barracks();
    void create_materials();
    void load_textures();
    
    // Structure components
    void create_foundation();
    void create_main_building();
    void create_roof();
    void create_entrance();
    void create_windows();
    void create_door();
    void create_training_area();
    void create_details();
    void create_flag();
    void create_sandbags();
    void create_chimney();
    void create_support_structures();
    void create_guard_tower();
    void create_antenna();
    void create_vehicle_depot();
    void create_perimeter();
    
    // Mesh generation helpers
    godot::Ref<godot::ArrayMesh> create_box_mesh(float width, float height, float depth);
    godot::Ref<godot::ArrayMesh> create_cylinder_mesh(float radius, float height, int segments = 16);
    godot::Ref<godot::ArrayMesh> create_roof_mesh(float width, float depth, float height, float overhang);
    godot::Ref<godot::ArrayMesh> create_sandbag_mesh();
    godot::Ref<godot::ArrayMesh> create_flag_mesh(float width, float height);
    
    // Door animation
    void open_door();
    void close_door();
    void toggle_door();
    void update_door_animation(double delta);
    bool is_door_open() const;
    
    // Unit training
    void train_unit();
    void queue_unit();
    void cancel_training();
    void update_unit_training(double delta);
    godot::Vector3 get_spawn_point() const;
    godot::Vector3 get_rally_point() const;
    void set_rally_point(const godot::Vector3 &point);
    
    // Getters
    int get_unit_queue() const;
    float get_train_progress() const;
    bool get_is_training() const;
};

} // namespace rts

#endif // BARRACKS_H
