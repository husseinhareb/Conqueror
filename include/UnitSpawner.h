/**
 * UnitSpawner.h
 * Spawns and manages RTS units with MultiMesh optimization for rendering.
 * Maintains both logical Unit nodes and visual MultiMeshInstance3D.
 */

#ifndef UNIT_SPAWNER_H
#define UNIT_SPAWNER_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class Unit;
class SelectionManager;
class FlowFieldManager;

class UnitSpawner : public godot::Node3D {
    GDCLASS(UnitSpawner, godot::Node3D)

private:
    // References
    SelectionManager *selection_manager = nullptr;
    FlowFieldManager *flow_field_manager = nullptr;
    
    // Unit template
    godot::Ref<godot::PackedScene> unit_scene;
    godot::Ref<godot::Mesh> unit_mesh;
    
    // Units container
    godot::Vector<Unit*> units;
    int max_units = 100;
    int next_unit_id = 0;
    
    // MultiMesh for optimized rendering
    godot::MultiMeshInstance3D *multi_mesh_instance = nullptr;
    godot::Ref<godot::MultiMesh> multi_mesh;
    bool use_multi_mesh = true;
    
    // Spawn settings
    float spawn_radius = 20.0f;
    float spawn_height = 0.5f;
    godot::Vector3 spawn_center;
    
    // Auto spawn on ready
    bool auto_spawn = true;
    int auto_spawn_count = 30;

protected:
    static void _bind_methods();

public:
    UnitSpawner();
    ~UnitSpawner();

    void _ready() override;
    void _process(double delta) override;

    // Spawning
    Unit* spawn_unit(const godot::Vector3 &position);
    void spawn_units_in_formation(int count, const godot::Vector3 &center, float radius);
    void spawn_units_grid(int count, const godot::Vector3 &center, float spacing);
    
    void despawn_unit(Unit *unit);
    void despawn_all_units();

    // MultiMesh management
    void setup_multi_mesh();
    void update_multi_mesh_transforms();
    void sync_unit_to_multi_mesh(int index);
    
    // Unit access
    godot::Vector<Unit*> get_all_units() const;
    Unit* get_unit_by_id(int id) const;
    int get_unit_count() const;
    
    // Flow field integration
    void update_units_flow_vectors();

    // Setters
    void set_selection_manager(SelectionManager *manager);
    void set_flow_field_manager(FlowFieldManager *manager);
    void set_unit_scene(const godot::Ref<godot::PackedScene> &scene);
    void set_unit_mesh(const godot::Ref<godot::Mesh> &mesh);
    
    void set_max_units(int count);
    int get_max_units() const;
    
    void set_auto_spawn(bool enabled);
    bool get_auto_spawn() const;
    
    void set_auto_spawn_count(int count);
    int get_auto_spawn_count() const;
    
    // Spawn validation
    bool is_spawn_location_valid(const godot::Vector3 &position);
    godot::Vector3 find_valid_spawn_location(const godot::Vector3 &center, float search_radius);
};

} // namespace rts

#endif // UNIT_SPAWNER_H
