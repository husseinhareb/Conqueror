/**
 * FlowFieldManager.h
 * Grid-based pathfinding using Dijkstra flow field algorithm.
 * Computes direction vectors for each cell toward a target.
 */

#ifndef FLOW_FIELD_MANAGER_H
#define FLOW_FIELD_MANAGER_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <queue>
#include <vector>

namespace rts {

struct FlowCell {
    int x = 0;
    int y = 0;
    float cost = 1.0f;
    float distance = std::numeric_limits<float>::max();
    godot::Vector3 direction = godot::Vector3(0, 0, 0);
    bool walkable = true;
};

class FlowFieldManager : public godot::Node3D {
    GDCLASS(FlowFieldManager, godot::Node3D)

private:
    // Grid settings
    int grid_width = 64;
    int grid_height = 64;
    float cell_size = 2.0f;
    godot::Vector3 grid_origin;
    
    // Flow field data
    std::vector<std::vector<FlowCell>> grid;
    bool field_computed = false;
    godot::Vector3 current_target;
    
    // Terrain sampling
    float terrain_sample_height = 100.0f;
    uint32_t ground_collision_layer = 1;
    uint32_t obstacle_collision_layer = 4;
    
    // Debug
    bool debug_draw = false;

protected:
    static void _bind_methods();

public:
    FlowFieldManager();
    ~FlowFieldManager();

    void _ready() override;
    void _process(double delta) override;

    // Grid management
    void initialize_grid();
    void update_walkability();
    
    // Flow field computation
    void compute_flow_field(const godot::Vector3 &target_world_pos);
    void compute_distances(int target_x, int target_y);
    void compute_directions();
    
    // Query
    godot::Vector3 get_flow_direction(const godot::Vector3 &world_pos) const;
    bool is_position_walkable(const godot::Vector3 &world_pos) const;
    
    // Coordinate conversion
    godot::Vector2i world_to_grid(const godot::Vector3 &world_pos) const;
    godot::Vector3 grid_to_world(int x, int y) const;
    bool is_valid_cell(int x, int y) const;
    
    // Getters/Setters
    void set_grid_size(int width, int height);
    godot::Vector2i get_grid_size() const;
    
    void set_cell_size(float size);
    float get_cell_size() const;
    
    void set_grid_origin(const godot::Vector3 &origin);
    godot::Vector3 get_grid_origin() const;
    
    void set_debug_draw(bool enabled);
    bool get_debug_draw() const;
    
    bool is_field_valid() const;
    
    // Debug visualization
    void draw_debug_field();
};

} // namespace rts

#endif // FLOW_FIELD_MANAGER_H
