/**
 * FloorSnapper.h
 * Robust snap-to-floor utility for RTS units, vehicles, and buildings.
 * Ensures meshes rest exactly on terrain/floor surfaces.
 */

#ifndef FLOOR_SNAPPER_H
#define FLOOR_SNAPPER_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

/**
 * Result of a floor snap operation
 */
struct FloorSnapResult {
    bool success = false;           // Whether a floor was found
    float floor_y = 0.0f;           // World Y of the floor hit point
    float unit_bottom_y = 0.0f;     // Local Y of unit's bottom
    float final_y = 0.0f;           // Final world Y for unit origin
    godot::Vector3 floor_normal;    // Normal of the floor (for slope alignment)
};

/**
 * Configuration for floor snapping
 */
struct FloorSnapConfig {
    float raycast_max_distance = 50.0f;     // Max distance to search for floor
    float raycast_start_height = 10.0f;     // Height above unit to start ray
    float default_ground_y = 0.0f;          // Fallback if no floor found
    bool use_collider_bounds = true;        // Prefer collider over mesh bounds
    bool use_multi_ray = false;             // Use multiple rays for wide bases
    float multi_ray_spread = 1.0f;          // Spread distance for multi-ray
    uint32_t floor_collision_mask = 1;      // Collision mask for floor (layer 1)
    float ground_offset = 0.0f;             // Additional offset from ground
};

/**
 * FloorSnapper - Static utility class for snapping objects to floor
 */
class FloorSnapper {
public:
    /**
     * Snap a Node3D to the floor below it.
     * 
     * @param node The node to snap (Unit, Vehicle, Building, etc.)
     * @param config Configuration for the snap operation
     * @return FloorSnapResult with details about the operation
     * 
     * Algorithm:
     * 1. Determine the local "bottom Y" of the node (from collider or mesh bounds)
     * 2. Cast a ray downward from above the node
     * 3. Calculate the correct Y position so the bottom touches the floor
     * 4. Apply the position
     */
    static FloorSnapResult snap_to_floor(
        godot::Node3D *node,
        const FloorSnapConfig &config = FloorSnapConfig()
    );
    
    /**
     * Get the local bottom Y of a node (lowest point in local space).
     * Checks colliders first (if enabled), then mesh bounds.
     * 
     * @param node The node to analyze
     * @param use_collider Whether to prefer collider bounds
     * @param scale The node's scale (to account for scaled meshes)
     * @return Local Y coordinate of the bottom
     */
    static float get_local_bottom_y(
        godot::Node3D *node,
        bool use_collider = true,
        const godot::Vector3 &scale = godot::Vector3(1, 1, 1)
    );
    
    /**
     * Perform a single downward raycast to find floor.
     * 
     * @param space_state The physics space state
     * @param origin Ray origin point
     * @param max_distance Maximum ray distance
     * @param collision_mask Collision layers to hit
     * @return Pair of (success, hit_point)
     */
    static std::pair<bool, godot::Vector3> raycast_floor(
        godot::PhysicsDirectSpaceState3D *space_state,
        const godot::Vector3 &origin,
        float max_distance,
        uint32_t collision_mask
    );
    
    /**
     * Perform multiple raycasts for wide-base objects.
     * Returns the lowest (minimum Y) hit point.
     * 
     * @param space_state The physics space state
     * @param center Center point of the object
     * @param spread Spread distance from center
     * @param max_distance Maximum ray distance
     * @param collision_mask Collision layers to hit
     * @return Pair of (success, lowest_hit_point)
     */
    static std::pair<bool, godot::Vector3> raycast_floor_multi(
        godot::PhysicsDirectSpaceState3D *space_state,
        const godot::Vector3 &center,
        float spread,
        float max_distance,
        uint32_t collision_mask
    );
    
    /**
     * Get mesh bounds from a node (searches children recursively).
     * 
     * @param node The node to search
     * @return AABB in local space, or empty AABB if no mesh found
     */
    static godot::AABB get_mesh_bounds(godot::Node3D *node);
    
    /**
     * Get collider bounds from a node (searches children).
     * 
     * @param node The node to search
     * @return AABB in local space, or empty AABB if no collider found
     */
    static godot::AABB get_collider_bounds(godot::Node3D *node);

private:
    // Helper to recursively find mesh bounds
    static void find_mesh_bounds_recursive(
        godot::Node *node,
        godot::AABB &bounds,
        bool &found
    );
    
    // Helper to recursively find collider bounds
    static void find_collider_bounds_recursive(
        godot::Node *node,
        godot::AABB &bounds,
        bool &found
    );
};

} // namespace rts

#endif // FLOOR_SNAPPER_H
