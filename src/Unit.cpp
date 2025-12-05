/**
 * Unit.cpp
 * RTS unit implementation with movement, selection, and flow field integration.
 * Includes collision avoidance for buildings, other units, and vehicles.
 */

#include "Unit.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Unit::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("set_move_target", "target"), &Unit::set_move_target);
    ClassDB::bind_method(D_METHOD("apply_flow_vector", "vector"), &Unit::apply_flow_vector);
    ClassDB::bind_method(D_METHOD("stop_movement"), &Unit::stop_movement);
    
    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &Unit::set_selected);
    ClassDB::bind_method(D_METHOD("get_selected"), &Unit::get_selected);
    
    ClassDB::bind_method(D_METHOD("set_hovered", "hovered"), &Unit::set_hovered);
    ClassDB::bind_method(D_METHOD("get_hovered"), &Unit::get_hovered);
    
    ClassDB::bind_method(D_METHOD("is_moving"), &Unit::is_moving);
    ClassDB::bind_method(D_METHOD("get_target_position"), &Unit::get_target_position);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &Unit::set_move_speed);
    ClassDB::bind_method(D_METHOD("get_move_speed"), &Unit::get_move_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed", PROPERTY_HINT_RANGE, "1.0,20.0,0.5"), "set_move_speed", "get_move_speed");
    
    ClassDB::bind_method(D_METHOD("set_unit_id", "id"), &Unit::set_unit_id);
    ClassDB::bind_method(D_METHOD("get_unit_id"), &Unit::get_unit_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "unit_id"), "set_unit_id", "get_unit_id");
    
    ClassDB::bind_method(D_METHOD("set_unit_name", "name"), &Unit::set_unit_name);
    ClassDB::bind_method(D_METHOD("get_unit_name"), &Unit::get_unit_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "unit_name"), "set_unit_name", "get_unit_name");
    
    ClassDB::bind_method(D_METHOD("set_health", "health"), &Unit::set_health);
    ClassDB::bind_method(D_METHOD("get_health"), &Unit::get_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "health"), "set_health", "get_health");
    
    ClassDB::bind_method(D_METHOD("set_max_health", "max_health"), &Unit::set_max_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &Unit::get_max_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_health"), "set_max_health", "get_max_health");
    
    ClassDB::bind_method(D_METHOD("set_attack_damage", "damage"), &Unit::set_attack_damage);
    ClassDB::bind_method(D_METHOD("get_attack_damage"), &Unit::get_attack_damage);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "attack_damage"), "set_attack_damage", "get_attack_damage");
    
    ClassDB::bind_method(D_METHOD("set_attack_range", "range"), &Unit::set_attack_range);
    ClassDB::bind_method(D_METHOD("get_attack_range"), &Unit::get_attack_range);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_range"), "set_attack_range", "get_attack_range");
    
    // Signals
    ADD_SIGNAL(MethodInfo("unit_selected", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("unit_deselected", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("unit_arrived", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("unit_hovered", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("unit_unhovered", PropertyInfo(Variant::OBJECT, "unit")));
}

Unit::Unit() {
}

Unit::~Unit() {
}

void Unit::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    target_position = get_global_position();
    current_velocity = Vector3(0, 0, 0);
    flow_vector = Vector3(0, 0, 0);
    last_position = get_global_position();
    
    // Store the base speed and Y offset for walking animation
    base_move_speed = move_speed;
    base_y_offset = 0.0f;
    terrain_height = get_global_position().y;
    last_terrain_height = terrain_height;
    
    // Cache terrain generator reference for performance
    SceneTree *tree = get_tree();
    if (tree) {
        Node *root = tree->get_root();
        if (root) {
            cached_terrain_generator = root->find_child("TerrainGenerator", true, false);
        }
    }
    
    // Initialize cached physics shapes for separation/avoidance (avoid per-frame allocations)
    cached_separation_sphere.instantiate();
    cached_separation_sphere->set_radius(separation_radius);
    
    cached_shape_query.instantiate();
    cached_shape_query->set_shape(cached_separation_sphere);
    cached_shape_query->set_collision_mask(2 | 8); // Units and vehicles
    
    cached_ray_query.instantiate();
    cached_ray_query->set_collision_mask(obstacle_mask);
    
    // Set collision layer to 2 (units)
    set_collision_layer(2);
    // Collide with ground (1) and buildings (4) - physical collision prevents passing through
    // Also collide with other units (2) and vehicles (8) for physical blocking
    set_collision_mask(1 | 2 | 4 | 8);
    
    // Configure CharacterBody3D for proper slope handling
    // Allow walking on slopes up to 60 degrees (mountains can be steep)
    set_floor_max_angle(Math::deg_to_rad(60.0f));
    // Ensure we snap to floor when walking
    set_floor_snap_length(1.0f);
    // Set up direction for floor detection
    set_up_direction(Vector3(0, 1, 0));
    // Allow sliding on walls/slopes
    set_slide_on_ceiling_enabled(false);
    // Maximum slope for floor detection
    set_floor_stop_on_slope_enabled(false);
    set_floor_block_on_wall_enabled(false);
}

void Unit::_physics_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    update_movement(delta);
    update_walk_animation(delta);
}

void Unit::set_move_target(const Vector3 &target) {
    target_position = target;
    target_position.y = get_global_position().y; // Keep same height
    has_move_order = true;
}

void Unit::apply_flow_vector(const Vector3 &vector) {
    flow_vector = vector;
}

void Unit::stop_movement() {
    has_move_order = false;
    flow_vector = Vector3(0, 0, 0);
    current_velocity = Vector3(0, 0, 0);
}

void Unit::update_movement(double delta) {
    if (!has_move_order) {
        // Decelerate to stop
        if (current_velocity.length_squared() > 0.01f) {
            current_velocity = current_velocity.move_toward(Vector3(0, 0, 0), deceleration * delta);
            set_velocity(current_velocity);
            move_and_slide();
        }
        is_avoiding = false;
        return;
    }
    
    Vector3 current_pos = get_global_position();
    Vector3 to_target = target_position - current_pos;
    to_target.y = 0;
    
    float distance = to_target.length();
    
    // Check if arrived
    if (distance < arrival_threshold) {
        has_move_order = false;
        current_velocity = Vector3(0, 0, 0);
        is_avoiding = false;
        emit_signal("unit_arrived", this);
        return;
    }
    
    // Update stuck detection
    update_stuck_detection(delta);
    
    // Calculate base desired direction toward target
    Vector3 desired_direction = to_target.normalized();
    
    // Use flow field if available
    if (use_flow_field && flow_vector.length_squared() > 0.01f) {
        desired_direction = flow_vector.normalized();
    }
    
    // Check if path ahead is blocked
    float forward_distance = raycast_distance(desired_direction, avoidance_radius);
    bool path_blocked = forward_distance < avoidance_radius * 0.8f;
    
    Vector3 move_direction = desired_direction;
    
    if (path_blocked) {
        // Find a clear direction to go around the obstacle
        move_direction = find_clear_direction(desired_direction);
        is_avoiding = true;
    } else {
        // Path is clear, but check if we were avoiding and should continue
        if (is_avoiding) {
            // Check if the direct path to target is now clear
            Vector3 direct_to_target = to_target.normalized();
            float direct_distance = raycast_distance(direct_to_target, avoidance_radius);
            if (direct_distance >= avoidance_radius * 0.9f) {
                is_avoiding = false;
            } else {
                // Continue with avoidance direction
                move_direction = find_clear_direction(desired_direction);
            }
        }
    }
    
    // Add separation from other units
    Vector3 separation = calculate_separation_force();
    move_direction = (move_direction + separation * 0.3f).normalized();
    
    // Calculate desired velocity
    Vector3 desired_velocity = move_direction * move_speed;
    
    // Apply steering
    Vector3 steering = calculate_steering(desired_velocity);
    current_velocity += steering * delta;
    
    // Clamp to max speed
    if (current_velocity.length() > move_speed) {
        current_velocity = current_velocity.normalized() * move_speed;
    }
    
    // Apply movement
    set_velocity(current_velocity);
    move_and_slide();
    
    // Snap to terrain height
    snap_to_terrain();
    
    // Rotate to face movement direction
    if (current_velocity.length_squared() > 0.1f) {
        Vector3 look_dir = current_velocity.normalized();
        float target_angle = Math::atan2(look_dir.x, look_dir.z);
        Vector3 rotation = get_rotation();
        rotation.y = Math::lerp_angle(rotation.y, target_angle, static_cast<float>(steering_strength * delta));
        set_rotation(rotation);
    }
}

void Unit::update_walk_animation(double delta) {
    // Get the mesh instance for animation
    MeshInstance3D *mesh = nullptr;
    for (int i = 0; i < get_child_count(); i++) {
        mesh = Object::cast_to<MeshInstance3D>(get_child(i));
        if (mesh) break;
    }
    
    if (!mesh) return;
    
    float speed = current_velocity.length();
    
    if (speed > 0.5f) {
        // Animate walk - increase time based on speed
        walk_time += delta * walk_bob_speed * (speed / move_speed);
        
        // Vertical bobbing (two steps per cycle)
        float bob_y = Math::sin(walk_time * 2.0f) * walk_bob_amount * (speed / move_speed);
        
        // Side-to-side sway
        float sway_x = Math::sin(walk_time) * walk_sway_amount * (speed / move_speed);
        
        // Slight forward/backward lean based on acceleration
        float lean = 0.0f;
        if (has_move_order) {
            lean = 0.05f; // Slight forward lean when moving
        }
        
        // Apply to mesh position (relative to unit)
        Vector3 mesh_pos = mesh->get_position();
        mesh_pos.y = 0.5f + bob_y; // Base offset + bob (0.5 is the original offset in the scene)
        mesh_pos.x = sway_x;
        mesh->set_position(mesh_pos);
        
        // Apply slight rotation for walking feel
        Vector3 mesh_rot = mesh->get_rotation();
        mesh_rot.x = lean;
        mesh_rot.z = -sway_x * 2.0f; // Tilt based on sway
        mesh->set_rotation(mesh_rot);
    } else {
        // Reset to idle position smoothly
        walk_time = 0.0f;
        
        Vector3 mesh_pos = mesh->get_position();
        mesh_pos.y = Math::lerp(mesh_pos.y, 0.5f, static_cast<float>(delta * 10.0f));
        mesh_pos.x = Math::lerp(mesh_pos.x, 0.0f, static_cast<float>(delta * 10.0f));
        mesh->set_position(mesh_pos);
        
        Vector3 mesh_rot = mesh->get_rotation();
        mesh_rot.x = Math::lerp(mesh_rot.x, 0.0f, static_cast<float>(delta * 10.0f));
        mesh_rot.z = Math::lerp(mesh_rot.z, 0.0f, static_cast<float>(delta * 10.0f));
        mesh->set_rotation(mesh_rot);
    }
}

Vector3 Unit::calculate_steering(const Vector3 &desired_velocity) const {
    Vector3 steering = desired_velocity - current_velocity;
    
    // Limit steering force
    float max_steering = acceleration;
    if (steering.length() > max_steering) {
        steering = steering.normalized() * max_steering;
    }
    
    return steering;
}

void Unit::set_selected(bool selected) {
    if (is_selected == selected) {
        return;
    }
    
    is_selected = selected;
    update_selection_visual();
    
    if (is_selected) {
        emit_signal("unit_selected", this);
    } else {
        emit_signal("unit_deselected", this);
    }
}

bool Unit::get_selected() const {
    return is_selected;
}

void Unit::set_hovered(bool hovered) {
    if (is_hovered == hovered) {
        return;
    }
    
    is_hovered = hovered;
    update_hover_visual();
    
    if (is_hovered) {
        emit_signal("unit_hovered", this);
    } else {
        emit_signal("unit_unhovered", this);
    }
}

bool Unit::get_hovered() const {
    return is_hovered;
}

void Unit::update_selection_visual() {
    // Find mesh instance child and update material
    for (int i = 0; i < get_child_count(); i++) {
        MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(get_child(i));
        if (mesh) {
            Ref<StandardMaterial3D> mat = mesh->get_surface_override_material(0);
            if (mat.is_null()) {
                mat.instantiate();
                mesh->set_surface_override_material(0, mat);
            }
            
            if (is_selected) {
                // Selected: bright green with emission
                mat->set_albedo(Color(0.2f, 0.8f, 0.2f));
                mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                mat->set_emission(Color(0.1f, 0.5f, 0.1f));
                mat->set_emission_energy_multiplier(1.5f);
            } else if (is_hovered) {
                // Keep hover state if not selected
                update_hover_visual();
            } else {
                // Default: blue
                mat->set_albedo(Color(0.3f, 0.3f, 0.8f));
                mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
            }
        }
    }
}

void Unit::update_hover_visual() {
    // Find mesh instance child and update material for hover
    for (int i = 0; i < get_child_count(); i++) {
        MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(get_child(i));
        if (mesh) {
            Ref<StandardMaterial3D> mat = mesh->get_surface_override_material(0);
            if (mat.is_null()) {
                mat.instantiate();
                mesh->set_surface_override_material(0, mat);
            }
            
            // Don't override selection visual
            if (is_selected) {
                return;
            }
            
            if (is_hovered) {
                // Hovered: light green border effect with emission
                mat->set_albedo(Color(0.4f, 0.7f, 0.4f)); // Light green tint
                mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                mat->set_emission(Color(0.3f, 0.8f, 0.3f)); // Light green glow
                mat->set_emission_energy_multiplier(0.8f);
            } else {
                // Default: blue
                mat->set_albedo(Color(0.3f, 0.3f, 0.8f));
                mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
            }
        }
    }
}

void Unit::set_move_speed(float speed) {
    move_speed = speed;
}

float Unit::get_move_speed() const {
    return move_speed;
}

void Unit::set_unit_id(int id) {
    unit_id = id;
}

int Unit::get_unit_id() const {
    return unit_id;
}

bool Unit::is_moving() const {
    return has_move_order || current_velocity.length_squared() > 0.1f;
}

Vector3 Unit::get_target_position() const {
    return target_position;
}

void Unit::set_unit_name(const String &name) {
    unit_name = name;
}

String Unit::get_unit_name() const {
    return unit_name;
}

void Unit::set_health(int hp) {
    health = hp;
}

int Unit::get_health() const {
    return health;
}

void Unit::set_max_health(int hp) {
    max_health = hp;
}

int Unit::get_max_health() const {
    return max_health;
}

void Unit::set_attack_damage(int damage) {
    attack_damage = damage;
}

int Unit::get_attack_damage() const {
    return attack_damage;
}

void Unit::set_attack_range(float range) {
    attack_range = range;
}

float Unit::get_attack_range() const {
    return attack_range;
}

Vector3 Unit::calculate_avoidance_force() {
    // This is now a simpler force-based backup, main avoidance is in find_clear_direction
    Vector3 avoidance_force = Vector3(0, 0, 0);
    
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return avoidance_force;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return avoidance_force;
    
    Vector3 current_pos = get_global_position();
    Vector3 forward = current_velocity.normalized();
    if (forward.length_squared() < 0.01f) {
        forward = -get_transform().basis.get_column(2);
    }
    
    // Simple forward check for immediate obstacles
    float ahead_dist = raycast_distance(forward, wall_follow_distance);
    if (ahead_dist < wall_follow_distance) {
        // Push away from the obstacle
        float strength = (1.0f - ahead_dist / wall_follow_distance) * avoidance_strength;
        avoidance_force = -forward * strength;
    }
    
    return avoidance_force;
}

Vector3 Unit::calculate_separation_force() {
    Vector3 separation_force = Vector3(0, 0, 0);
    
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return separation_force;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return separation_force;
    
    if (cached_shape_query.is_null()) return separation_force;
    
    Vector3 current_pos = get_global_position();
    
    // Use cached shape query (shapes initialized in _ready)
    cached_shape_query->set_transform(Transform3D(Basis(), current_pos + Vector3(0, 0.5f, 0)));
    cached_shape_query->set_exclude(TypedArray<RID>::make(get_rid()));
    
    TypedArray<Dictionary> results = space_state->intersect_shape(cached_shape_query, 10);
    
    for (int i = 0; i < results.size(); i++) {
        Dictionary result = results[i];
        Object *collider_obj = Object::cast_to<Object>(result["collider"]);
        if (!collider_obj) continue;
        
        Node3D *other = Object::cast_to<Node3D>(collider_obj);
        if (!other || other == this) continue;
        
        Vector3 other_pos = other->get_global_position();
        Vector3 away = current_pos - other_pos;
        away.y = 0;
        
        float dist = away.length();
        if (dist > 0.01f && dist < separation_radius) {
            float strength = (1.0f - dist / separation_radius) * separation_strength;
            separation_force += away.normalized() * strength;
        }
    }
    
    return separation_force;
}

Vector3 Unit::find_clear_direction(const Vector3 &preferred_dir) {
    // Context steering: cast rays in multiple directions and find the best open path
    // Reduced from 16 to 8 directions for performance
    const int num_directions = 8;
    float best_score = -1000.0f;
    Vector3 best_direction = preferred_dir;
    
    Vector3 current_pos = get_global_position();
    Vector3 to_target = (target_position - current_pos).normalized();
    
    for (int i = 0; i < num_directions; i++) {
        float angle = (i * 2.0f * Math_PI) / num_directions;
        Vector3 dir = Vector3(Math::sin(angle), 0, Math::cos(angle));
        
        // Raycast in this direction
        float clearance = raycast_distance(dir, avoidance_radius);
        
        // Score this direction
        // Higher clearance is better
        float clearance_score = clearance / avoidance_radius;
        
        // Prefer directions toward the target
        float target_alignment = dir.dot(to_target);
        float target_score = (target_alignment + 1.0f) * 0.5f; // 0 to 1
        
        // Prefer directions aligned with preferred direction (momentum)
        float momentum_alignment = dir.dot(preferred_dir);
        float momentum_score = (momentum_alignment + 1.0f) * 0.25f; // 0 to 0.5
        
        // Combined score - clearance is most important, then target direction
        float score = clearance_score * 2.0f + target_score * 1.5f + momentum_score;
        
        // If path is blocked, heavily penalize
        if (clearance < 1.0f) {
            score -= 5.0f;
        }
        
        if (score > best_score) {
            best_score = score;
            best_direction = dir;
        }
    }
    
    // If we found a clear direction, remember which side we're going around
    if (best_direction.cross(preferred_dir).y > 0) {
        avoid_direction = 1.0f; // Going right
    } else {
        avoid_direction = -1.0f; // Going left
    }
    
    return best_direction;
}

float Unit::raycast_distance(const Vector3 &direction, float max_distance) {
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return max_distance;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return max_distance;
    
    Vector3 current_pos = get_global_position();
    Vector3 from = current_pos + Vector3(0, 0.5f, 0);
    Vector3 to = from + direction.normalized() * max_distance;
    
    // Use cached ray query when possible, but we need to create new for different rays
    // PhysicsRayQueryParameters3D::create is still needed as rays change per-call
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(from, to);
    query->set_collision_mask(obstacle_mask); // Buildings only for pathfinding
    query->set_exclude(TypedArray<RID>::make(get_rid()));
    
    Dictionary result = space_state->intersect_ray(query);
    
    if (result.is_empty()) {
        return max_distance;
    }
    
    Vector3 hit_pos = result["position"];
    return (hit_pos - from).length();
}

bool Unit::check_path_blocked(const Vector3 &direction, float distance) {
    return raycast_distance(direction, distance) < distance;
}

void Unit::update_stuck_detection(double delta) {
    Vector3 current_pos = get_global_position();
    float moved_distance = (current_pos - last_position).length();
    
    if (moved_distance < 0.05f * delta) {
        stuck_timer += delta;
        if (stuck_timer > 1.0f) {
            // We're stuck, try a different avoidance direction
            avoid_direction = -avoid_direction;
            stuck_timer = 0.0f;
        }
    } else {
        stuck_timer = 0.0f;
    }
    
    last_position = current_pos;
}

void Unit::snap_to_terrain() {
    // Use cached terrain generator reference (cached in _ready for performance)
    if (!cached_terrain_generator) return;
    
    Vector3 pos = get_global_position();
    
    // Check if within bounds
    Variant bounds_result = cached_terrain_generator->call("is_within_bounds", pos.x, pos.z);
    if (bounds_result.get_type() == Variant::BOOL && !(bool)bounds_result) {
        // Out of bounds - push back toward center
        float world_size = 0.0f;
        Variant size_result = cached_terrain_generator->call("get_world_size");
        if (size_result.get_type() == Variant::FLOAT) {
            world_size = (float)size_result * 0.5f - 5.0f; // 5 unit buffer from edge
        } else {
            world_size = 100.0f; // Fallback
        }
        
        pos.x = Math::clamp(pos.x, -world_size, world_size);
        pos.z = Math::clamp(pos.z, -world_size, world_size);
    }
    
    // Get terrain height at current position
    Variant height_result = cached_terrain_generator->call("get_height_at", pos.x, pos.z);
    if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
        last_terrain_height = terrain_height;
        terrain_height = (float)height_result;
        pos.y = terrain_height;
        set_global_position(pos);
        
        // Calculate slope based on height change and horizontal movement
        float horizontal_speed = Vector2(current_velocity.x, current_velocity.z).length();
        if (horizontal_speed > 0.1f) {
            float height_diff = terrain_height - last_terrain_height;
            // Normalize slope: positive = going uphill, negative = going downhill
            current_slope = Math::clamp(height_diff / (horizontal_speed * 0.016f), -1.0f, 1.0f);
            
            // Adjust move speed based on slope
            if (current_slope > 0.05f) {
                // Going uphill - slow down
                float slope_factor = 1.0f - (current_slope * (1.0f - uphill_speed_multiplier));
                move_speed = base_move_speed * Math::max(slope_factor, uphill_speed_multiplier);
            } else if (current_slope < -0.05f) {
                // Going downhill - speed up
                float slope_factor = 1.0f + (-current_slope * (downhill_speed_multiplier - 1.0f));
                move_speed = base_move_speed * Math::min(slope_factor, downhill_speed_multiplier);
            } else {
                // Flat terrain - normal speed
                move_speed = base_move_speed;
            }
        }
    }
}

float Unit::get_slope_ahead(const Vector3 &direction, float check_distance) {
    // Check the terrain slope in the given direction
    if (!cached_terrain_generator) return 0.0f;
    
    Vector3 current_pos = get_global_position();
    Vector3 ahead_pos = current_pos + direction.normalized() * check_distance;
    
    // Get heights at both positions
    Variant current_height = cached_terrain_generator->call("get_height_at", current_pos.x, current_pos.z);
    Variant ahead_height = cached_terrain_generator->call("get_height_at", ahead_pos.x, ahead_pos.z);
    
    if ((current_height.get_type() == Variant::FLOAT || current_height.get_type() == Variant::INT) &&
        (ahead_height.get_type() == Variant::FLOAT || ahead_height.get_type() == Variant::INT)) {
        float h1 = (float)current_height;
        float h2 = (float)ahead_height;
        float height_diff = h2 - h1;
        
        // Calculate slope as rise/run (tangent of angle)
        // Positive = uphill, negative = downhill
        return height_diff / check_distance;
    }
    
    return 0.0f;
}

bool Unit::can_traverse_slope(const Vector3 &direction) {
    float slope = get_slope_ahead(direction, 2.0f);
    // Check if slope is within traversable range
    // slope is rise/run, so 1.0 = 45 degrees, 0.7 = ~35 degrees
    return Math::abs(slope) <= max_traversable_slope;
}

} // namespace rts
