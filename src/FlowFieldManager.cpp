/**
 * FlowFieldManager.cpp
 * Grid-based pathfinding using Dijkstra flow field algorithm.
 */

#include "FlowFieldManager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void FlowFieldManager::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("initialize_grid"), &FlowFieldManager::initialize_grid);
    ClassDB::bind_method(D_METHOD("compute_flow_field", "target_world_pos"), &FlowFieldManager::compute_flow_field);
    ClassDB::bind_method(D_METHOD("get_flow_direction", "world_pos"), &FlowFieldManager::get_flow_direction);
    ClassDB::bind_method(D_METHOD("is_position_walkable", "world_pos"), &FlowFieldManager::is_position_walkable);
    ClassDB::bind_method(D_METHOD("is_field_valid"), &FlowFieldManager::is_field_valid);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_cell_size", "size"), &FlowFieldManager::set_cell_size);
    ClassDB::bind_method(D_METHOD("get_cell_size"), &FlowFieldManager::get_cell_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.5,10.0,0.5"), "set_cell_size", "get_cell_size");
    
    ClassDB::bind_method(D_METHOD("set_grid_origin", "origin"), &FlowFieldManager::set_grid_origin);
    ClassDB::bind_method(D_METHOD("get_grid_origin"), &FlowFieldManager::get_grid_origin);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "grid_origin"), "set_grid_origin", "get_grid_origin");
    
    ClassDB::bind_method(D_METHOD("set_debug_draw", "enabled"), &FlowFieldManager::set_debug_draw);
    ClassDB::bind_method(D_METHOD("get_debug_draw"), &FlowFieldManager::get_debug_draw);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_draw"), "set_debug_draw", "get_debug_draw");
}

FlowFieldManager::FlowFieldManager() {
}

FlowFieldManager::~FlowFieldManager() {
}

void FlowFieldManager::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Set grid origin to center the grid
    grid_origin = Vector3(
        -grid_width * cell_size / 2.0f,
        0,
        -grid_height * cell_size / 2.0f
    );
    
    initialize_grid();
}

void FlowFieldManager::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    if (debug_draw && field_computed) {
        draw_debug_field();
    }
}

void FlowFieldManager::initialize_grid() {
    grid.clear();
    grid.resize(grid_width);
    
    for (int x = 0; x < grid_width; x++) {
        grid[x].resize(grid_height);
        for (int y = 0; y < grid_height; y++) {
            FlowCell &cell = grid[x][y];
            cell.x = x;
            cell.y = y;
            cell.cost = 1.0f;
            cell.distance = std::numeric_limits<float>::max();
            cell.direction = Vector3(0, 0, 0);
            cell.walkable = true;
        }
    }
    
    update_walkability();
}

void FlowFieldManager::update_walkability() {
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return;
    
    for (int x = 0; x < grid_width; x++) {
        for (int y = 0; y < grid_height; y++) {
            Vector3 world_pos = grid_to_world(x, y);
            
            // Raycast down to check for obstacles
            Vector3 from = world_pos + Vector3(0, terrain_sample_height, 0);
            Vector3 to = world_pos - Vector3(0, terrain_sample_height, 0);
            
            Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(from, to);
            query->set_collision_mask(obstacle_collision_layer);
            
            Dictionary result = space_state->intersect_ray(query);
            
            grid[x][y].walkable = result.is_empty();
        }
    }
}

void FlowFieldManager::compute_flow_field(const Vector3 &target_world_pos) {
    Vector2i target_cell = world_to_grid(target_world_pos);
    
    if (!is_valid_cell(target_cell.x, target_cell.y)) {
        UtilityFunctions::print("FlowFieldManager: Target outside grid bounds");
        return;
    }
    
    current_target = target_world_pos;
    
    // Reset distances
    for (int x = 0; x < grid_width; x++) {
        for (int y = 0; y < grid_height; y++) {
            grid[x][y].distance = std::numeric_limits<float>::max();
            grid[x][y].direction = Vector3(0, 0, 0);
        }
    }
    
    compute_distances(target_cell.x, target_cell.y);
    compute_directions();
    
    field_computed = true;
}

void FlowFieldManager::compute_distances(int target_x, int target_y) {
    // Priority queue: (distance, x, y)
    auto cmp = [](const std::tuple<float, int, int> &a, const std::tuple<float, int, int> &b) {
        return std::get<0>(a) > std::get<0>(b);
    };
    std::priority_queue<std::tuple<float, int, int>, std::vector<std::tuple<float, int, int>>, decltype(cmp)> open_set(cmp);
    
    grid[target_x][target_y].distance = 0;
    open_set.push(std::make_tuple(0.0f, target_x, target_y));
    
    // 8-directional neighbors (including diagonals)
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const float costs[] = {1.414f, 1.0f, 1.414f, 1.0f, 1.0f, 1.414f, 1.0f, 1.414f};
    
    while (!open_set.empty()) {
        auto [dist, x, y] = open_set.top();
        open_set.pop();
        
        // Skip if we've found a better path
        if (dist > grid[x][y].distance) {
            continue;
        }
        
        // Check all neighbors
        for (int i = 0; i < 8; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if (!is_valid_cell(nx, ny)) continue;
            if (!grid[nx][ny].walkable) continue;
            
            float new_dist = grid[x][y].distance + costs[i] * grid[nx][ny].cost;
            
            if (new_dist < grid[nx][ny].distance) {
                grid[nx][ny].distance = new_dist;
                open_set.push(std::make_tuple(new_dist, nx, ny));
            }
        }
    }
}

void FlowFieldManager::compute_directions() {
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    for (int x = 0; x < grid_width; x++) {
        for (int y = 0; y < grid_height; y++) {
            if (!grid[x][y].walkable) continue;
            if (grid[x][y].distance == std::numeric_limits<float>::max()) continue;
            
            float min_dist = grid[x][y].distance;
            int best_dx = 0;
            int best_dy = 0;
            
            for (int i = 0; i < 8; i++) {
                int nx = x + dx[i];
                int ny = y + dy[i];
                
                if (!is_valid_cell(nx, ny)) continue;
                if (!grid[nx][ny].walkable) continue;
                
                if (grid[nx][ny].distance < min_dist) {
                    min_dist = grid[nx][ny].distance;
                    best_dx = dx[i];
                    best_dy = dy[i];
                }
            }
            
            // Convert grid direction to world direction
            grid[x][y].direction = Vector3(best_dx * cell_size, 0, best_dy * cell_size).normalized();
        }
    }
}

Vector3 FlowFieldManager::get_flow_direction(const Vector3 &world_pos) const {
    if (!field_computed) {
        return Vector3(0, 0, 0);
    }
    
    Vector2i cell = world_to_grid(world_pos);
    
    if (!is_valid_cell(cell.x, cell.y)) {
        return Vector3(0, 0, 0);
    }
    
    return grid[cell.x][cell.y].direction;
}

bool FlowFieldManager::is_position_walkable(const Vector3 &world_pos) const {
    Vector2i cell = world_to_grid(world_pos);
    
    if (!is_valid_cell(cell.x, cell.y)) {
        return false;
    }
    
    return grid[cell.x][cell.y].walkable;
}

Vector2i FlowFieldManager::world_to_grid(const Vector3 &world_pos) const {
    int x = static_cast<int>((world_pos.x - grid_origin.x) / cell_size);
    int y = static_cast<int>((world_pos.z - grid_origin.z) / cell_size);
    return Vector2i(x, y);
}

Vector3 FlowFieldManager::grid_to_world(int x, int y) const {
    return Vector3(
        grid_origin.x + (x + 0.5f) * cell_size,
        grid_origin.y,
        grid_origin.z + (y + 0.5f) * cell_size
    );
}

bool FlowFieldManager::is_valid_cell(int x, int y) const {
    return x >= 0 && x < grid_width && y >= 0 && y < grid_height;
}

void FlowFieldManager::set_grid_size(int width, int height) {
    grid_width = width;
    grid_height = height;
}

Vector2i FlowFieldManager::get_grid_size() const {
    return Vector2i(grid_width, grid_height);
}

void FlowFieldManager::set_cell_size(float size) {
    cell_size = size;
}

float FlowFieldManager::get_cell_size() const {
    return cell_size;
}

void FlowFieldManager::set_grid_origin(const Vector3 &origin) {
    grid_origin = origin;
}

Vector3 FlowFieldManager::get_grid_origin() const {
    return grid_origin;
}

void FlowFieldManager::set_debug_draw(bool enabled) {
    debug_draw = enabled;
}

bool FlowFieldManager::get_debug_draw() const {
    return debug_draw;
}

bool FlowFieldManager::is_field_valid() const {
    return field_computed;
}

void FlowFieldManager::draw_debug_field() {
    // Debug visualization would require ImmediateMesh
    // This is a placeholder - full implementation would draw arrows
    // showing flow directions for each cell
}

} // namespace rts
