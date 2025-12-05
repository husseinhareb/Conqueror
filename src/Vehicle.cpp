/**
 * Vehicle.cpp
 * RTS vehicle implementation - movable units with special actions.
 * Includes collision avoidance for buildings, units, and other vehicles.
 */

#include "Vehicle.h"
#include "FloorSnapper.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Vehicle::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &Vehicle::set_selected);
    ClassDB::bind_method(D_METHOD("get_selected"), &Vehicle::get_selected);
    
    ClassDB::bind_method(D_METHOD("set_hovered", "hovered"), &Vehicle::set_hovered);
    ClassDB::bind_method(D_METHOD("get_hovered"), &Vehicle::get_hovered);
    
    ClassDB::bind_method(D_METHOD("move_to", "position"), &Vehicle::move_to);
    ClassDB::bind_method(D_METHOD("stop_moving"), &Vehicle::stop_moving);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_vehicle_name", "name"), &Vehicle::set_vehicle_name);
    ClassDB::bind_method(D_METHOD("get_vehicle_name"), &Vehicle::get_vehicle_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "vehicle_name"), "set_vehicle_name", "get_vehicle_name");
    
    ClassDB::bind_method(D_METHOD("set_health", "health"), &Vehicle::set_health);
    ClassDB::bind_method(D_METHOD("get_health"), &Vehicle::get_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "health"), "set_health", "get_health");
    
    ClassDB::bind_method(D_METHOD("set_max_health", "max_health"), &Vehicle::set_max_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &Vehicle::get_max_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_health"), "set_max_health", "get_max_health");
    
    ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &Vehicle::set_move_speed);
    ClassDB::bind_method(D_METHOD("get_move_speed"), &Vehicle::get_move_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed", PROPERTY_HINT_RANGE, "1.0,20.0,0.5"), "set_move_speed", "get_move_speed");
    
    ClassDB::bind_method(D_METHOD("set_vehicle_id", "id"), &Vehicle::set_vehicle_id);
    ClassDB::bind_method(D_METHOD("get_vehicle_id"), &Vehicle::get_vehicle_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "vehicle_id"), "set_vehicle_id", "get_vehicle_id");
    
    ClassDB::bind_method(D_METHOD("set_model_path", "path"), &Vehicle::set_model_path);
    ClassDB::bind_method(D_METHOD("get_model_path"), &Vehicle::get_model_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "model_path", PROPERTY_HINT_FILE, "*.glb,*.gltf"), "set_model_path", "get_model_path");
    
    ClassDB::bind_method(D_METHOD("set_model_scale", "scale"), &Vehicle::set_model_scale);
    ClassDB::bind_method(D_METHOD("get_model_scale"), &Vehicle::get_model_scale);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "model_scale", PROPERTY_HINT_RANGE, "0.01,10.0,0.01"), "set_model_scale", "get_model_scale");
    
    // Signals
    ADD_SIGNAL(MethodInfo("vehicle_selected", PropertyInfo(Variant::OBJECT, "vehicle")));
    ADD_SIGNAL(MethodInfo("vehicle_deselected", PropertyInfo(Variant::OBJECT, "vehicle")));
}

Vehicle::Vehicle() {
}

Vehicle::~Vehicle() {
}

void Vehicle::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Set collision layer to 8 (vehicles)
    // Collision layers: 1=Ground, 2=Units, 4=Buildings, 8=Vehicles
    set_collision_layer(8);
    // Collide with ground (1), buildings (4), units (2), and other vehicles (8)
    // Physical collision prevents passing through any object
    set_collision_mask(1 | 2 | 4 | 8);
    
    // Configure CharacterBody3D for proper slope handling
    // Vehicles can climb slopes up to 45 degrees (less than infantry)
    set_floor_max_angle(Math::deg_to_rad(45.0f));
    // Ensure we snap to floor when driving
    set_floor_snap_length(1.5f);
    // Set up direction for floor detection
    set_up_direction(Vector3(0, 1, 0));
    // Allow sliding on walls/slopes
    set_slide_on_ceiling_enabled(false);
    // Don't stop on slopes, allow driving up/down
    set_floor_stop_on_slope_enabled(false);
    set_floor_block_on_wall_enabled(false);
    
    target_position = get_global_position();
    last_position = get_global_position();
    
    // Store base speed for slope calculations
    base_move_speed = move_speed;
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
    
    if (!model_path.is_empty()) {
        load_model();
    } else {
        create_vehicle_mesh();
    }
    
    setup_collision();
    
    // Snap to floor after all setup is complete
    FloorSnapConfig config;
    config.floor_collision_mask = 1; // Ground layer
    config.raycast_start_height = 10.0f;
    config.raycast_max_distance = 50.0f;
    config.ground_offset = 0.0f;
    
    FloorSnapResult result = FloorSnapper::snap_to_floor(this, config);
    if (result.success) {
        UtilityFunctions::print("Vehicle snapped to floor at Y=", result.final_y);
    }
}

void Vehicle::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
}

void Vehicle::_physics_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    if (!is_moving) {
        is_avoiding = false;
        return;
    }
    
    Vector3 current_pos = get_global_position();
    Vector3 direction = target_position - current_pos;
    direction.y = 0; // Keep on ground plane
    
    float distance = direction.length();
    
    if (distance < 1.0f) {
        is_moving = false;
        is_avoiding = false;
        current_velocity = Vector3();
        return;
    }
    
    // Update stuck detection
    update_stuck_detection(delta);
    
    direction = direction.normalized();
    
    // Check if path ahead is blocked
    float forward_distance = raycast_distance(direction, avoidance_radius);
    bool path_blocked = forward_distance < avoidance_radius * 0.7f;
    
    Vector3 move_direction = direction;
    
    if (path_blocked) {
        move_direction = find_clear_direction(direction);
        is_avoiding = true;
    } else if (is_avoiding) {
        // Check if direct path to target is now clear
        float direct_distance = raycast_distance(direction, avoidance_radius);
        if (direct_distance >= avoidance_radius * 0.9f) {
            is_avoiding = false;
        } else {
            move_direction = find_clear_direction(direction);
        }
    }
    
    // Add separation from other vehicles/units
    Vector3 separation = calculate_separation_force();
    move_direction = (move_direction + separation * 0.2f).normalized();
    
    Vector3 desired_direction = move_direction * move_speed;
    desired_direction.y = 0;
    
    // Calculate target angle from desired direction
    Vector3 move_dir = desired_direction.normalized();
    float target_angle = atan2(move_dir.x, move_dir.z);
    float current_angle = get_rotation().y;
    float angle_diff = target_angle - current_angle;
    
    // Normalize angle
    while (angle_diff > Math_PI) angle_diff -= Math_PI * 2;
    while (angle_diff < -Math_PI) angle_diff += Math_PI * 2;
    
    float rotation_step = turn_speed * delta;
    if (abs(angle_diff) < rotation_step) {
        set_rotation(Vector3(0, target_angle, 0));
    } else {
        set_rotation(Vector3(0, current_angle + (angle_diff > 0 ? rotation_step : -rotation_step), 0));
    }
    
    // Move in the direction we're facing (or desired direction if close enough)
    if (abs(angle_diff) < Math_PI / 4) { // Only move if roughly facing the right direction
        current_velocity = desired_direction;
    } else {
        // Slow down while turning
        current_velocity = desired_direction * 0.3f;
    }
    
    set_velocity(current_velocity);
    move_and_slide();
    
    // Snap to terrain height
    snap_to_terrain();
}

void Vehicle::create_vehicle_mesh() {
    if (!mesh_instance) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
        
        // Create box mesh for vehicle (wider than tall)
        Ref<BoxMesh> box_mesh;
        box_mesh.instantiate();
        box_mesh->set_size(Vector3(1.5f, 0.8f, 2.0f));
        mesh_instance->set_mesh(box_mesh);
        
        mesh_instance->set_position(Vector3(0, 0.4f, 0));
        
        // Yellow color for construction vehicle
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_albedo(Color(0.9f, 0.7f, 0.1f));
        mat->set_roughness(0.6f);
        mesh_instance->set_surface_override_material(0, mat);
    }
}

void Vehicle::load_model() {
    if (model_path.is_empty()) {
        create_vehicle_mesh();
        return;
    }
    
    ResourceLoader *loader = ResourceLoader::get_singleton();
    
    if (!loader->exists(model_path)) {
        UtilityFunctions::print("Vehicle: Model file does not exist: ", model_path);
        create_vehicle_mesh();
        return;
    }
    
    Ref<PackedScene> scene = loader->load(model_path);
    
    if (scene.is_null()) {
        UtilityFunctions::print("Vehicle: Failed to load model: ", model_path);
        create_vehicle_mesh();
        return;
    }
    
    model_instance = Object::cast_to<Node3D>(scene->instantiate());
    if (!model_instance) {
        create_vehicle_mesh();
        return;
    }
    
    add_child(model_instance);
    model_instance->set_scale(Vector3(model_scale, model_scale, model_scale));
    
    // Model is positioned at origin - FloorSnapper will handle ground positioning
    model_instance->set_position(Vector3(0, 0, 0));
    
    // Rotate model 180 degrees so front faces forward (Z+ direction)
    model_instance->set_rotation(Vector3(0, Math_PI, 0));
    
    find_mesh_instances_recursive(model_instance);
    
    UtilityFunctions::print("Vehicle: Loaded model from ", model_path);
}

void Vehicle::find_mesh_instances_recursive(Node *node) {
    if (!node) return;
    
    MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
    if (mesh && !mesh_instance) {
        mesh_instance = mesh;
    }
    
    for (int i = 0; i < node->get_child_count(); i++) {
        find_mesh_instances_recursive(node->get_child(i));
    }
}

void Vehicle::setup_collision() {
    if (!collision_shape) {
        collision_shape = memnew(CollisionShape3D);
        add_child(collision_shape);
        
        Ref<BoxShape3D> box_shape;
        box_shape.instantiate();
        // Scale collision with model
        float col_scale = model_scale > 0 ? model_scale : 1.0f;
        box_shape->set_size(Vector3(1.5f * col_scale, 1.0f * col_scale, 2.0f * col_scale));
        collision_shape->set_shape(box_shape);
        // Position collision shape so bottom is at Y=0
        collision_shape->set_position(Vector3(0, 0.5f * col_scale, 0));
    }
}

void Vehicle::move_to(const Vector3 &position) {
    target_position = position;
    target_position.y = get_global_position().y;
    is_moving = true;
}

void Vehicle::stop_moving() {
    is_moving = false;
    current_velocity = Vector3();
}

bool Vehicle::get_is_moving() const {
    return is_moving;
}

void Vehicle::set_selected(bool selected) {
    if (is_selected == selected) return;
    
    is_selected = selected;
    update_selection_visual();
    
    if (is_selected) {
        emit_signal("vehicle_selected", this);
    } else {
        emit_signal("vehicle_deselected", this);
    }
}

bool Vehicle::get_selected() const {
    return is_selected;
}

void Vehicle::set_hovered(bool hovered) {
    if (is_hovered == hovered) return;
    
    is_hovered = hovered;
    update_hover_visual();
}

bool Vehicle::get_hovered() const {
    return is_hovered;
}

void Vehicle::update_selection_visual() {
    if (model_instance) {
        apply_visual_to_model(model_instance, is_selected, is_hovered);
        return;
    }
    
    if (!mesh_instance) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    if (is_selected) {
        mat->set_albedo(Color(0.9f, 0.8f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.2f, 0.5f, 0.2f));
        mat->set_emission_energy_multiplier(1.5f);
    } else if (is_hovered) {
        update_hover_visual();
    } else {
        mat->set_albedo(Color(0.9f, 0.7f, 0.1f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Vehicle::update_hover_visual() {
    if (model_instance) {
        apply_visual_to_model(model_instance, is_selected, is_hovered);
        return;
    }
    
    if (!mesh_instance) return;
    if (is_selected) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    if (is_hovered) {
        mat->set_albedo(Color(0.95f, 0.85f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.2f, 0.4f, 0.2f));
        mat->set_emission_energy_multiplier(0.5f);
    } else {
        mat->set_albedo(Color(0.9f, 0.7f, 0.1f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Vehicle::apply_visual_to_model(Node *node, bool selected, bool hovered) {
    if (!node) return;
    
    MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
    if (mesh) {
        Ref<Mesh> mesh_res = mesh->get_mesh();
        if (mesh_res.is_valid()) {
            int surface_count = mesh_res->get_surface_count();
            for (int s = 0; s < surface_count; s++) {
                Ref<StandardMaterial3D> mat = mesh->get_surface_override_material(s);
                if (mat.is_null()) {
                    Ref<Material> original_mat = mesh_res->surface_get_material(s);
                    Ref<StandardMaterial3D> original_std = original_mat;
                    
                    if (original_std.is_valid()) {
                        mat = original_std->duplicate();
                    } else {
                        mat.instantiate();
                    }
                    mesh->set_surface_override_material(s, mat);
                }
                
                if (selected) {
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                    mat->set_emission(Color(0.1f, 0.5f, 0.1f));
                    mat->set_emission_energy_multiplier(1.5f);
                } else if (hovered) {
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                    mat->set_emission(Color(0.2f, 0.5f, 0.2f));
                    mat->set_emission_energy_multiplier(0.6f);
                } else {
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
                }
            }
        }
    }
    
    for (int i = 0; i < node->get_child_count(); i++) {
        apply_visual_to_model(node->get_child(i), selected, hovered);
    }
}

void Vehicle::set_vehicle_name(const String &name) {
    vehicle_name = name;
}

String Vehicle::get_vehicle_name() const {
    return vehicle_name;
}

void Vehicle::set_health(int hp) {
    health = hp;
}

int Vehicle::get_health() const {
    return health;
}

void Vehicle::set_max_health(int hp) {
    max_health = hp;
}

int Vehicle::get_max_health() const {
    return max_health;
}

void Vehicle::set_move_speed(float speed) {
    move_speed = speed;
}

float Vehicle::get_move_speed() const {
    return move_speed;
}

void Vehicle::set_vehicle_id(int id) {
    vehicle_id = id;
}

int Vehicle::get_vehicle_id() const {
    return vehicle_id;
}

void Vehicle::set_model_path(const String &path) {
    model_path = path;
}

String Vehicle::get_model_path() const {
    return model_path;
}

void Vehicle::set_model_scale(float scale) {
    model_scale = scale;
}

float Vehicle::get_model_scale() const {
    return model_scale;
}

Vector3 Vehicle::calculate_avoidance_force() {
    // Simple backup force-based avoidance
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
    
    float ahead_dist = raycast_distance(forward, wall_follow_distance);
    if (ahead_dist < wall_follow_distance) {
        float strength = (1.0f - ahead_dist / wall_follow_distance) * avoidance_strength;
        avoidance_force = -forward * strength;
    }
    
    return avoidance_force;
}

Vector3 Vehicle::calculate_separation_force() {
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

Vector3 Vehicle::find_clear_direction(const Vector3 &preferred_dir) {
    // Reduced from 16 to 8 directions for performance
    const int num_directions = 8;
    float best_score = -1000.0f;
    Vector3 best_direction = preferred_dir;
    
    Vector3 current_pos = get_global_position();
    Vector3 to_target = (target_position - current_pos).normalized();
    
    for (int i = 0; i < num_directions; i++) {
        float angle = (i * 2.0f * Math_PI) / num_directions;
        Vector3 dir = Vector3(Math::sin(angle), 0, Math::cos(angle));
        
        float clearance = raycast_distance(dir, avoidance_radius);
        
        float clearance_score = clearance / avoidance_radius;
        float target_alignment = dir.dot(to_target);
        float target_score = (target_alignment + 1.0f) * 0.5f;
        float momentum_alignment = dir.dot(preferred_dir);
        float momentum_score = (momentum_alignment + 1.0f) * 0.25f;
        
        float score = clearance_score * 2.0f + target_score * 1.5f + momentum_score;
        
        if (clearance < 2.0f) {
            score -= 5.0f;
        }
        
        if (score > best_score) {
            best_score = score;
            best_direction = dir;
        }
    }
    
    if (best_direction.cross(preferred_dir).y > 0) {
        avoid_direction = 1.0f;
    } else {
        avoid_direction = -1.0f;
    }
    
    return best_direction;
}

float Vehicle::raycast_distance(const Vector3 &direction, float max_distance) {
    Ref<World3D> world = get_viewport()->get_world_3d();
    if (world.is_null()) return max_distance;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return max_distance;
    
    Vector3 current_pos = get_global_position();
    Vector3 from = current_pos + Vector3(0, 0.5f, 0);
    Vector3 to = from + direction.normalized() * max_distance;
    
    Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(from, to);
    query->set_collision_mask(obstacle_mask);
    query->set_exclude(TypedArray<RID>::make(get_rid()));
    
    Dictionary result = space_state->intersect_ray(query);
    
    if (result.is_empty()) {
        return max_distance;
    }
    
    Vector3 hit_pos = result["position"];
    return (hit_pos - from).length();
}

bool Vehicle::check_path_blocked(const Vector3 &direction, float distance) {
    return raycast_distance(direction, distance) < distance;
}

void Vehicle::update_stuck_detection(double delta) {
    Vector3 current_pos = get_global_position();
    float moved_distance = (current_pos - last_position).length();
    
    if (moved_distance < 0.03f * delta) {
        stuck_timer += delta;
        if (stuck_timer > 1.5f) {
            avoid_direction = -avoid_direction;
            stuck_timer = 0.0f;
        }
    } else {
        stuck_timer = 0.0f;
    }
    
    last_position = current_pos;
}

void Vehicle::snap_to_terrain() {
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
            world_size = (float)size_result * 0.5f - 5.0f;
        } else {
            world_size = 100.0f;
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
                // Going uphill - slow down (vehicles are heavier, slower on hills)
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

float Vehicle::get_slope_ahead(const Vector3 &direction, float check_distance) {
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

bool Vehicle::can_traverse_slope(const Vector3 &direction) {
    float slope = get_slope_ahead(direction, 3.0f);
    // Check if slope is within traversable range
    // slope is rise/run, so 1.0 = 45 degrees, 0.7 = ~35 degrees
    return Math::abs(slope) <= max_traversable_slope;
}

} // namespace rts
