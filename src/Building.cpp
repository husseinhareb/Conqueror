/**
 * Building.cpp
 * RTS building implementation - static structures with placement validation.
 */

#include "Building.h"
#include "FlowFieldManager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Building::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &Building::set_selected);
    ClassDB::bind_method(D_METHOD("get_selected"), &Building::get_selected);
    
    ClassDB::bind_method(D_METHOD("set_hovered", "hovered"), &Building::set_hovered);
    ClassDB::bind_method(D_METHOD("get_hovered"), &Building::get_hovered);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_building_name", "name"), &Building::set_building_name);
    ClassDB::bind_method(D_METHOD("get_building_name"), &Building::get_building_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "building_name"), "set_building_name", "get_building_name");
    
    ClassDB::bind_method(D_METHOD("set_health", "health"), &Building::set_health);
    ClassDB::bind_method(D_METHOD("get_health"), &Building::get_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "health"), "set_health", "get_health");
    
    ClassDB::bind_method(D_METHOD("set_max_health", "max_health"), &Building::set_max_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &Building::get_max_health);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_health"), "set_max_health", "get_max_health");
    
    ClassDB::bind_method(D_METHOD("set_building_size", "size"), &Building::set_building_size);
    ClassDB::bind_method(D_METHOD("get_building_size"), &Building::get_building_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "building_size", PROPERTY_HINT_RANGE, "1.0,20.0,0.5"), "set_building_size", "get_building_size");
    
    ClassDB::bind_method(D_METHOD("set_building_height", "height"), &Building::set_building_height);
    ClassDB::bind_method(D_METHOD("get_building_height"), &Building::get_building_height);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "building_height", PROPERTY_HINT_RANGE, "1.0,10.0,0.5"), "set_building_height", "get_building_height");
    
    ClassDB::bind_method(D_METHOD("set_building_id", "id"), &Building::set_building_id);
    ClassDB::bind_method(D_METHOD("get_building_id"), &Building::get_building_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "building_id"), "set_building_id", "get_building_id");
    
    ClassDB::bind_method(D_METHOD("set_armor", "armor"), &Building::set_armor);
    ClassDB::bind_method(D_METHOD("get_armor"), &Building::get_armor);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "armor"), "set_armor", "get_armor");
    
    ClassDB::bind_method(D_METHOD("set_model_path", "path"), &Building::set_model_path);
    ClassDB::bind_method(D_METHOD("get_model_path"), &Building::get_model_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "model_path", PROPERTY_HINT_FILE, "*.glb,*.gltf"), "set_model_path", "get_model_path");
    
    ClassDB::bind_method(D_METHOD("set_model_scale", "scale"), &Building::set_model_scale);
    ClassDB::bind_method(D_METHOD("get_model_scale"), &Building::get_model_scale);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "model_scale", PROPERTY_HINT_RANGE, "0.01,10.0,0.01"), "set_model_scale", "get_model_scale");
    
    // Signals
    ADD_SIGNAL(MethodInfo("building_selected", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_deselected", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_destroyed", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_hovered", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("building_unhovered", PropertyInfo(Variant::OBJECT, "building")));
}

Building::Building() {
}

Building::~Building() {
}

void Building::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Set collision layer to 4 (buildings) - separate from units (2) and ground (1)
    set_collision_layer(4);
    // Buildings don't need to detect collisions with anything
    set_collision_mask(0);
    
    // Try to load custom model first, fall back to box mesh
    if (!model_path.is_empty()) {
        load_model();
    } else {
        create_building_mesh();
    }
    
    setup_collision();
    
    // Snap building to terrain height
    snap_to_terrain();
}

void Building::snap_to_terrain() {
    Node *terrain_node = get_tree()->get_root()->find_child("TerrainGenerator", true, false);
    if (terrain_node) {
        Vector3 pos = get_global_position();
        Variant height_result = terrain_node->call("get_height_at", pos.x, pos.z);
        if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
            float terrain_y = (float)height_result;
            pos.y = terrain_y;
            set_global_position(pos);
            UtilityFunctions::print("Building snapped to terrain at Y=", terrain_y);
        }
    }
}

void Building::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    // Buildings are static, no update needed
}

void Building::create_building_mesh() {
    // Check if mesh already exists as child
    for (int i = 0; i < get_child_count(); i++) {
        mesh_instance = Object::cast_to<MeshInstance3D>(get_child(i));
        if (mesh_instance) break;
    }
    
    // Create mesh if it doesn't exist
    if (!mesh_instance) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
        
        // Create box mesh for building
        Ref<BoxMesh> box_mesh;
        box_mesh.instantiate();
        box_mesh->set_size(Vector3(building_size, building_height, building_size));
        mesh_instance->set_mesh(box_mesh);
        
        // Position mesh so bottom is at ground level
        mesh_instance->set_position(Vector3(0, building_height / 2.0f, 0));
        
        // Create material
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_albedo(Color(0.5f, 0.4f, 0.3f)); // Brown/tan color for building
        mat->set_roughness(0.8f);
        mesh_instance->set_surface_override_material(0, mat);
    }
}

void Building::load_model() {
    if (model_path.is_empty()) {
        UtilityFunctions::print("Building: No model path specified, using default box");
        create_building_mesh();
        return;
    }
    
    UtilityFunctions::print("Building: Attempting to load model from: ", model_path);
    
    // Load the 3D model (GLB/GLTF)
    ResourceLoader *loader = ResourceLoader::get_singleton();
    
    // Check if file exists first
    if (!loader->exists(model_path)) {
        UtilityFunctions::print("Building: Model file does not exist: ", model_path);
        create_building_mesh();
        return;
    }
    
    Ref<PackedScene> scene = loader->load(model_path);
    
    if (scene.is_null()) {
        UtilityFunctions::print("Building: Failed to load model from ", model_path, ", using default box");
        create_building_mesh();
        return;
    }
    
    UtilityFunctions::print("Building: PackedScene loaded successfully");
    
    // Instance the loaded model
    model_instance = Object::cast_to<Node3D>(scene->instantiate());
    if (!model_instance) {
        UtilityFunctions::print("Building: Failed to instantiate model, using default box");
        create_building_mesh();
        return;
    }
    
    add_child(model_instance);
    
    // Apply scale
    model_instance->set_scale(Vector3(model_scale, model_scale, model_scale));
    
    UtilityFunctions::print("Building: Model instanced with scale: ", model_scale);
    UtilityFunctions::print("Building: Model child count: ", model_instance->get_child_count());
    
    // Find mesh instances recursively
    find_mesh_instances_recursive(model_instance);
    
    if (mesh_instance) {
        UtilityFunctions::print("Building: Found mesh instance in model");
    } else {
        UtilityFunctions::print("Building: No mesh instance found in model hierarchy");
    }
    
    UtilityFunctions::print("Building: Successfully loaded model from ", model_path);
}

void Building::find_mesh_instances_recursive(Node *node) {
    if (!node) return;
    
    // Check if this node is a MeshInstance3D
    MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
    if (mesh && !mesh_instance) {
        mesh_instance = mesh;
        UtilityFunctions::print("Building: Found MeshInstance3D: ", mesh->get_name());
    }
    
    // Recursively check children
    for (int i = 0; i < node->get_child_count(); i++) {
        find_mesh_instances_recursive(node->get_child(i));
    }
}

void Building::setup_collision() {
    // Check if collision shape already exists
    for (int i = 0; i < get_child_count(); i++) {
        collision_shape = Object::cast_to<CollisionShape3D>(get_child(i));
        if (collision_shape) break;
    }
    
    // Create collision shape if it doesn't exist
    if (!collision_shape) {
        collision_shape = memnew(CollisionShape3D);
        add_child(collision_shape);
        
        // Create box shape matching the building size
        Ref<BoxShape3D> box_shape;
        box_shape.instantiate();
        box_shape->set_size(Vector3(building_size, building_height, building_size));
        collision_shape->set_shape(box_shape);
        
        // Position collision shape so bottom is at ground level
        collision_shape->set_position(Vector3(0, building_height / 2.0f, 0));
    }
}

void Building::set_selected(bool selected) {
    if (is_selected == selected) {
        return;
    }
    
    is_selected = selected;
    update_selection_visual();
    
    if (is_selected) {
        emit_signal("building_selected", this);
    } else {
        emit_signal("building_deselected", this);
    }
}

bool Building::get_selected() const {
    return is_selected;
}

void Building::set_hovered(bool hovered) {
    if (is_hovered == hovered) {
        return;
    }
    
    is_hovered = hovered;
    update_hover_visual();
    
    if (is_hovered) {
        emit_signal("building_hovered", this);
    } else {
        emit_signal("building_unhovered", this);
    }
}

bool Building::get_hovered() const {
    return is_hovered;
}

void Building::update_selection_visual() {
    // If we have a loaded model, apply to all meshes recursively
    if (model_instance) {
        apply_selection_to_model(model_instance, is_selected, is_hovered);
        return;
    }
    
    // Fallback for simple box mesh
    if (!mesh_instance) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    if (is_selected) {
        // Selected: bright green with emission
        mat->set_albedo(Color(0.3f, 0.7f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.1f, 0.4f, 0.1f));
        mat->set_emission_energy_multiplier(1.5f);
    } else if (is_hovered) {
        update_hover_visual();
    } else {
        // Default: brown/tan
        mat->set_albedo(Color(0.5f, 0.4f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Building::update_hover_visual() {
    // If we have a loaded model, apply to all meshes recursively
    if (model_instance) {
        apply_selection_to_model(model_instance, is_selected, is_hovered);
        return;
    }
    
    // Fallback for simple box mesh
    if (!mesh_instance) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    // Don't override selection visual
    if (is_selected) {
        return;
    }
    
    if (is_hovered) {
        // Hovered: light green tint with emission
        mat->set_albedo(Color(0.5f, 0.6f, 0.4f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.2f, 0.5f, 0.2f));
        mat->set_emission_energy_multiplier(0.6f);
    } else {
        // Default: brown/tan
        mat->set_albedo(Color(0.5f, 0.4f, 0.3f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
    }
}

void Building::apply_selection_to_model(Node *node, bool selected, bool hovered) {
    if (!node) return;
    
    // Check if this node is a MeshInstance3D
    MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
    if (mesh) {
        // Get the mesh to find surface count
        Ref<Mesh> mesh_res = mesh->get_mesh();
        if (mesh_res.is_valid()) {
            int surface_count = mesh_res->get_surface_count();
            for (int s = 0; s < surface_count; s++) {
                // Create or get override material
                Ref<StandardMaterial3D> mat = mesh->get_surface_override_material(s);
                if (mat.is_null()) {
                    // Try to get the original material
                    Ref<Material> original_mat = mesh_res->surface_get_material(s);
                    Ref<StandardMaterial3D> original_std = original_mat;
                    
                    if (original_std.is_valid()) {
                        // Duplicate the original material so we can modify it
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
    
    // Recursively process children
    for (int i = 0; i < node->get_child_count(); i++) {
        apply_selection_to_model(node->get_child(i), selected, hovered);
    }
}

void Building::set_building_name(const String &name) {
    building_name = name;
}

String Building::get_building_name() const {
    return building_name;
}

void Building::set_health(int hp) {
    health = hp;
}

int Building::get_health() const {
    return health;
}

void Building::set_max_health(int hp) {
    max_health = hp;
}

int Building::get_max_health() const {
    return max_health;
}

void Building::set_building_size(float size) {
    building_size = size;
}

float Building::get_building_size() const {
    return building_size;
}

void Building::set_building_height(float height) {
    building_height = height;
}

float Building::get_building_height() const {
    return building_height;
}

void Building::set_building_id(int id) {
    building_id = id;
}

int Building::get_building_id() const {
    return building_id;
}

void Building::set_armor(int value) {
    armor = value;
}

int Building::get_armor() const {
    return armor;
}

void Building::set_model_path(const String &path) {
    model_path = path;
}

String Building::get_model_path() const {
    return model_path;
}

void Building::set_model_scale(float scale) {
    model_scale = scale;
}

float Building::get_model_scale() const {
    return model_scale;
}

bool Building::can_place_at(const Vector3 &position) {
    return is_position_valid_for_building(this, position, building_size, placement_check_mask);
}

bool Building::check_placement_valid() {
    is_placement_valid = can_place_at(get_global_position());
    if (is_preview_mode) {
        update_preview_visual();
    }
    return is_placement_valid;
}

void Building::set_preview_mode(bool preview) {
    is_preview_mode = preview;
    if (preview) {
        // In preview mode, disable collision so it doesn't interfere with placement checks
        set_collision_layer(0);
        set_collision_mask(0);
        update_preview_visual();
    } else {
        // Restore normal collision
        set_collision_layer(4); // Buildings layer
        set_collision_mask(0);
    }
}

bool Building::get_preview_mode() const {
    return is_preview_mode;
}

void Building::update_preview_visual() {
    // Update visual based on placement validity
    if (model_instance) {
        apply_selection_to_model(model_instance, false, false);
        // Override with placement color
        apply_placement_color_to_model(model_instance, is_placement_valid);
        return;
    }
    
    if (!mesh_instance) return;
    
    Ref<StandardMaterial3D> mat = mesh_instance->get_surface_override_material(0);
    if (mat.is_null()) {
        mat.instantiate();
        mesh_instance->set_surface_override_material(0, mat);
    }
    
    mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    
    if (is_placement_valid) {
        // Green for valid placement
        mat->set_albedo(Color(0.2f, 0.8f, 0.2f, 0.6f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.1f, 0.4f, 0.1f));
        mat->set_emission_energy_multiplier(0.5f);
    } else {
        // Red for invalid placement
        mat->set_albedo(Color(0.8f, 0.2f, 0.2f, 0.6f));
        mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
        mat->set_emission(Color(0.4f, 0.1f, 0.1f));
        mat->set_emission_energy_multiplier(0.8f);
    }
}

void Building::apply_placement_color_to_model(Node *node, bool valid) {
    if (!node) return;
    
    MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node);
    if (mesh) {
        Ref<Mesh> mesh_res = mesh->get_mesh();
        if (mesh_res.is_valid()) {
            int surface_count = mesh_res->get_surface_count();
            for (int s = 0; s < surface_count; s++) {
                Ref<StandardMaterial3D> mat = mesh->get_surface_override_material(s);
                if (mat.is_null()) {
                    mat.instantiate();
                    mesh->set_surface_override_material(s, mat);
                }
                
                mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                
                if (valid) {
                    mat->set_albedo(Color(0.2f, 0.8f, 0.2f, 0.6f));
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                    mat->set_emission(Color(0.1f, 0.4f, 0.1f));
                    mat->set_emission_energy_multiplier(0.5f);
                } else {
                    mat->set_albedo(Color(0.8f, 0.2f, 0.2f, 0.6f));
                    mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
                    mat->set_emission(Color(0.4f, 0.1f, 0.1f));
                    mat->set_emission_energy_multiplier(0.8f);
                }
            }
        }
    }
    
    for (int i = 0; i < node->get_child_count(); i++) {
        apply_placement_color_to_model(node->get_child(i), valid);
    }
}

bool Building::is_position_valid_for_building(Node *context, const Vector3 &position, float size, uint32_t check_mask) {
    if (!context) return false;
    
    // First check: Is the position on valid terrain?
    Node *terrain_node = context->get_tree()->get_root()->find_child("TerrainGenerator", true, false);
    if (terrain_node) {
        // Check if within map bounds
        Variant bounds_result = terrain_node->call("is_within_bounds", position.x, position.z);
        if (bounds_result.get_type() == Variant::BOOL && !(bool)bounds_result) {
            return false; // Outside terrain bounds
        }
        
        // Check buildability at all corners of the building
        float half_size = size * 0.5f;
        float corners[4][2] = {
            {position.x - half_size, position.z - half_size},
            {position.x + half_size, position.z - half_size},
            {position.x - half_size, position.z + half_size},
            {position.x + half_size, position.z + half_size}
        };
        
        for (int i = 0; i < 4; i++) {
            Variant buildable_result = terrain_node->call("is_buildable_at", corners[i][0], corners[i][1]);
            if (buildable_result.get_type() == Variant::BOOL && !(bool)buildable_result) {
                return false; // Can't build here (water, steep slope, etc.)
            }
        }
        
        // Also check center
        Variant center_buildable = terrain_node->call("is_buildable_at", position.x, position.z);
        if (center_buildable.get_type() == Variant::BOOL && !(bool)center_buildable) {
            return false;
        }
    }
    
    // Second check: Overlapping with other objects
    Viewport *viewport = context->get_viewport();
    if (!viewport) return false;
    
    Ref<World3D> world = viewport->get_world_3d();
    if (world.is_null()) return false;
    
    PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
    if (!space_state) return false;
    
    // Use a box shape to check for overlapping objects
    Ref<BoxShape3D> check_shape;
    check_shape.instantiate();
    // Slightly smaller than the building to allow tight placement
    check_shape->set_size(Vector3(size * 0.9f, 2.0f, size * 0.9f));
    
    Ref<PhysicsShapeQueryParameters3D> shape_query;
    shape_query.instantiate();
    shape_query->set_shape(check_shape);
    shape_query->set_transform(Transform3D(Basis(), position + Vector3(0, 1.0f, 0)));
    shape_query->set_collision_mask(check_mask);
    
    TypedArray<Dictionary> results = space_state->intersect_shape(shape_query, 1);
    
    // If any objects found, placement is invalid
    return results.size() == 0;
}

void Building::notify_flow_field_of_placement() {
    // Find the FlowFieldManager and update walkability
    Node *flow_field = get_tree()->get_root()->find_child("FlowFieldManager", true, false);
    if (flow_field) {
        // Mark this building's area as unwalkable
        flow_field->call("mark_building_area", get_global_position(), building_size, false);
        UtilityFunctions::print("Building: Notified FlowFieldManager of placement");
    }
}

} // namespace rts
