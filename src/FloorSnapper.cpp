/**
 * FloorSnapper.cpp
 * Implementation of robust snap-to-floor utility.
 */

#include "FloorSnapper.h"

#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

FloorSnapResult FloorSnapper::snap_to_floor(Node3D *node, const FloorSnapConfig &config) {
    FloorSnapResult result;
    
    if (!node) {
        UtilityFunctions::print("FloorSnapper: Node is null");
        return result;
    }
    
    // Get the physics space
    Ref<World3D> world = node->get_world_3d();
    if (world.is_null()) {
        UtilityFunctions::print("FloorSnapper: No World3D found");
        return result;
    }
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) {
        UtilityFunctions::print("FloorSnapper: No physics space state");
        return result;
    }
    
    // Get the node's current position and scale
    Vector3 current_pos = node->get_global_position();
    Vector3 scale = node->get_global_transform().basis.get_scale();
    
    // Step 1: Determine the local bottom Y of the node
    float local_bottom_y = get_local_bottom_y(node, config.use_collider_bounds, scale);
    result.unit_bottom_y = local_bottom_y;
    
    // Step 2: Calculate ray origin (above the node)
    Vector3 ray_origin = current_pos;
    ray_origin.y += config.raycast_start_height;
    
    // Step 3: Perform raycast(s) to find floor
    std::pair<bool, Vector3> raycast_result;
    
    if (config.use_multi_ray) {
        raycast_result = raycast_floor_multi(
            space_state,
            ray_origin,
            config.multi_ray_spread,
            config.raycast_max_distance,
            config.floor_collision_mask
        );
    } else {
        raycast_result = raycast_floor(
            space_state,
            ray_origin,
            config.raycast_max_distance,
            config.floor_collision_mask
        );
    }
    
    // Step 4: Calculate and apply new position
    if (raycast_result.first) {
        result.success = true;
        result.floor_y = raycast_result.second.y;
        result.floor_normal = Vector3(0, 1, 0); // Could be extracted from raycast
        
        // Calculate the world Y for the unit's bottom
        // The unit's bottom in world space is: current_pos.y + (local_bottom_y * scale.y)
        // We want: floor_y = current_pos.y + (local_bottom_y * scale.y)
        // So: current_pos.y = floor_y - (local_bottom_y * scale.y)
        
        // For models where origin is at center: local_bottom_y is negative (e.g., -0.5)
        // For models where origin is at base: local_bottom_y is 0
        // For models where origin is at top: local_bottom_y is negative full height
        
        float world_bottom_offset = local_bottom_y * scale.y;
        result.final_y = result.floor_y - world_bottom_offset + config.ground_offset;
        
        // Apply the new position
        Vector3 new_pos = current_pos;
        new_pos.y = result.final_y;
        node->set_global_position(new_pos);
    } else {
        // Fallback: use default ground level
        result.success = false;
        result.floor_y = config.default_ground_y;
        
        float world_bottom_offset = local_bottom_y * scale.y;
        result.final_y = config.default_ground_y - world_bottom_offset + config.ground_offset;
        
        Vector3 new_pos = current_pos;
        new_pos.y = result.final_y;
        node->set_global_position(new_pos);
    }
    
    return result;
}

float FloorSnapper::get_local_bottom_y(Node3D *node, bool use_collider, const Vector3 &scale) {
    if (!node) return 0.0f;
    
    AABB bounds;
    bool found = false;
    
    // Try collider first if requested
    if (use_collider) {
        bounds = get_collider_bounds(node);
        if (bounds.size.length_squared() > 0.001f) {
            found = true;
        }
    }
    
    // Fall back to mesh bounds
    if (!found) {
        bounds = get_mesh_bounds(node);
        if (bounds.size.length_squared() > 0.001f) {
            found = true;
        }
    }
    
    if (!found) {
        // No bounds found, assume origin is at base
        return 0.0f;
    }
    
    // Return the minimum Y (bottom) of the bounds
    // This is in local space, relative to the node's origin
    return bounds.position.y;
}

std::pair<bool, Vector3> FloorSnapper::raycast_floor(
    PhysicsDirectSpaceState3D *space_state,
    const Vector3 &origin,
    float max_distance,
    uint32_t collision_mask
) {
    Vector3 ray_end = origin - Vector3(0, max_distance, 0);
    
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(origin, ray_end);
    query->set_collide_with_areas(false);
    query->set_collide_with_bodies(true);
    query->set_collision_mask(collision_mask);
    
    Dictionary result = space_state->intersect_ray(query);
    
    if (result.is_empty()) {
        return {false, Vector3()};
    }
    
    Vector3 hit_point = result["position"];
    return {true, hit_point};
}

std::pair<bool, Vector3> FloorSnapper::raycast_floor_multi(
    PhysicsDirectSpaceState3D *space_state,
    const Vector3 &center,
    float spread,
    float max_distance,
    uint32_t collision_mask
) {
    // Cast rays from center and 4 corners
    Vector3 offsets[] = {
        Vector3(0, 0, 0),           // Center
        Vector3(spread, 0, 0),      // Right
        Vector3(-spread, 0, 0),     // Left
        Vector3(0, 0, spread),      // Front
        Vector3(0, 0, -spread)      // Back
    };
    
    bool any_hit = false;
    float min_y = 1e10f;
    Vector3 lowest_point;
    
    for (const Vector3 &offset : offsets) {
        Vector3 ray_origin = center + offset;
        auto [hit, point] = raycast_floor(space_state, ray_origin, max_distance, collision_mask);
        
        if (hit) {
            any_hit = true;
            if (point.y < min_y) {
                min_y = point.y;
                lowest_point = point;
            }
        }
    }
    
    return {any_hit, lowest_point};
}

AABB FloorSnapper::get_mesh_bounds(Node3D *node) {
    AABB bounds;
    bool found = false;
    find_mesh_bounds_recursive(node, bounds, found);
    return bounds;
}

AABB FloorSnapper::get_collider_bounds(Node3D *node) {
    AABB bounds;
    bool found = false;
    find_collider_bounds_recursive(node, bounds, found);
    return bounds;
}

void FloorSnapper::find_mesh_bounds_recursive(Node *node, AABB &bounds, bool &found) {
    if (!node) return;
    
    // Check if this node is a MeshInstance3D
    MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(node);
    if (mesh_instance) {
        Ref<Mesh> mesh = mesh_instance->get_mesh();
        if (mesh.is_valid()) {
            AABB mesh_aabb = mesh->get_aabb();
            
            // Transform by the mesh instance's local transform
            Transform3D local_transform = mesh_instance->get_transform();
            mesh_aabb = local_transform.xform(mesh_aabb);
            
            if (!found) {
                bounds = mesh_aabb;
                found = true;
            } else {
                bounds = bounds.merge(mesh_aabb);
            }
        }
    }
    
    // Recursively check children
    for (int i = 0; i < node->get_child_count(); i++) {
        find_mesh_bounds_recursive(node->get_child(i), bounds, found);
    }
}

void FloorSnapper::find_collider_bounds_recursive(Node *node, AABB &bounds, bool &found) {
    if (!node) return;
    
    // Check if this node is a CollisionShape3D
    CollisionShape3D *collision = Object::cast_to<CollisionShape3D>(node);
    if (collision) {
        Ref<Shape3D> shape = collision->get_shape();
        if (shape.is_valid()) {
            AABB shape_aabb;
            bool got_aabb = false;
            
            // Handle different shape types
            Ref<BoxShape3D> box = shape;
            if (box.is_valid()) {
                Vector3 size = box->get_size();
                shape_aabb = AABB(-size / 2.0f, size);
                got_aabb = true;
            }
            
            Ref<CapsuleShape3D> capsule = shape;
            if (!got_aabb && capsule.is_valid()) {
                float radius = capsule->get_radius();
                float height = capsule->get_height();
                shape_aabb = AABB(
                    Vector3(-radius, -height / 2.0f, -radius),
                    Vector3(radius * 2, height, radius * 2)
                );
                got_aabb = true;
            }
            
            Ref<SphereShape3D> sphere = shape;
            if (!got_aabb && sphere.is_valid()) {
                float radius = sphere->get_radius();
                shape_aabb = AABB(
                    Vector3(-radius, -radius, -radius),
                    Vector3(radius * 2, radius * 2, radius * 2)
                );
                got_aabb = true;
            }
            
            Ref<CylinderShape3D> cylinder = shape;
            if (!got_aabb && cylinder.is_valid()) {
                float radius = cylinder->get_radius();
                float height = cylinder->get_height();
                shape_aabb = AABB(
                    Vector3(-radius, -height / 2.0f, -radius),
                    Vector3(radius * 2, height, radius * 2)
                );
                got_aabb = true;
            }
            
            if (got_aabb) {
                // Transform by the collision shape's local transform
                Transform3D local_transform = collision->get_transform();
                shape_aabb = local_transform.xform(shape_aabb);
                
                if (!found) {
                    bounds = shape_aabb;
                    found = true;
                } else {
                    bounds = bounds.merge(shape_aabb);
                }
            }
        }
    }
    
    // Recursively check children
    for (int i = 0; i < node->get_child_count(); i++) {
        find_collider_bounds_recursive(node->get_child(i), bounds, found);
    }
}

} // namespace rts
