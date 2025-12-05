/**
 * Bulldozer.cpp
 * Construction vehicle implementation with placement validation.
 */

#include "Bulldozer.h"
#include "FloorSnapper.h"
#include "Barracks.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/prism_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void Bulldozer::_bind_methods() {
    // Build commands
    ClassDB::bind_method(D_METHOD("start_placing_building", "type"), &Bulldozer::start_placing_building);
    ClassDB::bind_method(D_METHOD("cancel_placing"), &Bulldozer::cancel_placing);
    ClassDB::bind_method(D_METHOD("confirm_build_location", "location"), &Bulldozer::confirm_build_location);
    ClassDB::bind_method(D_METHOD("update_ghost_position", "position"), &Bulldozer::update_ghost_position);
    
    ClassDB::bind_method(D_METHOD("get_is_constructing"), &Bulldozer::get_is_constructing);
    ClassDB::bind_method(D_METHOD("get_construction_progress"), &Bulldozer::get_construction_progress);
    ClassDB::bind_method(D_METHOD("get_is_placing_building"), &Bulldozer::get_is_placing_building);
    ClassDB::bind_method(D_METHOD("get_placing_type"), &Bulldozer::get_placing_type);
    
    ClassDB::bind_method(D_METHOD("get_power_plant_cost"), &Bulldozer::get_power_plant_cost);
    ClassDB::bind_method(D_METHOD("get_barracks_cost"), &Bulldozer::get_barracks_cost);
    
    ClassDB::bind_method(D_METHOD("set_power_plant_model", "path"), &Bulldozer::set_power_plant_model);
    ClassDB::bind_method(D_METHOD("get_power_plant_model"), &Bulldozer::get_power_plant_model);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "power_plant_model", PROPERTY_HINT_FILE, "*.glb,*.gltf"), "set_power_plant_model", "get_power_plant_model");
    
    ClassDB::bind_method(D_METHOD("set_barracks_model", "path"), &Bulldozer::set_barracks_model);
    ClassDB::bind_method(D_METHOD("get_barracks_model"), &Bulldozer::get_barracks_model);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "barracks_model", PROPERTY_HINT_FILE, "*.glb,*.gltf"), "set_barracks_model", "get_barracks_model");
    
    // Signals
    ADD_SIGNAL(MethodInfo("construction_started", PropertyInfo(Variant::INT, "building_type")));
    ADD_SIGNAL(MethodInfo("construction_completed", PropertyInfo(Variant::OBJECT, "building")));
    ADD_SIGNAL(MethodInfo("construction_cancelled"));
    ADD_SIGNAL(MethodInfo("placing_building_started", PropertyInfo(Variant::INT, "building_type")));
    ADD_SIGNAL(MethodInfo("placing_building_cancelled"));
}

Bulldozer::Bulldozer() {
    vehicle_name = "Bulldozer";
    move_speed = 3.0f;  // Slower than regular units
    health = 300;
    max_health = 300;
}

Bulldozer::~Bulldozer() {
}

void Bulldozer::_ready() {
    Vehicle::_ready();
    
    UtilityFunctions::print("Bulldozer: Ready");
}

void Bulldozer::_process(double delta) {
    Vehicle::_process(delta);
    
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Update construction progress
    if (is_constructing) {
        update_construction(delta);
    }
}

void Bulldozer::_physics_process(double delta) {
    // Don't move while constructing
    if (is_constructing) {
        return;
    }
    
    Vehicle::_physics_process(delta);
    
    // Check if we've reached the build location
    if (is_moving && current_build_type != BuildingType::NONE) {
        Vector3 current_pos = get_global_position();
        Vector3 to_target = build_location - current_pos;
        to_target.y = 0;
        
        if (to_target.length() < 2.0f) {
            stop_moving();
            start_construction();
        }
    }
}

// Helper to load texture for bulldozer - loads directly from file
static Ref<ImageTexture> load_bulldozer_texture(const String &path) {
    // Convert res:// path to absolute path for direct file loading
    String abs_path = path;
    if (path.begins_with("res://")) {
        abs_path = path.replace("res://", "");
        // Get the project root
        abs_path = String("res://").path_join(abs_path);
    }
    
    if (!FileAccess::file_exists(path)) {
        UtilityFunctions::print("Bulldozer: Texture not found: ", path);
        return Ref<ImageTexture>();
    }
    
    // First try ResourceLoader (for imported textures)
    ResourceLoader *loader = ResourceLoader::get_singleton();
    if (loader) {
        Ref<Texture2D> tex = loader->load(path);
        if (tex.is_valid()) {
            Ref<ImageTexture> img_tex = tex;
            if (img_tex.is_valid()) {
                UtilityFunctions::print("Bulldozer: Loaded texture via ResourceLoader: ", path);
                return img_tex;
            }
            Ref<Image> image = tex->get_image();
            if (image.is_valid()) {
                image->generate_mipmaps();
                UtilityFunctions::print("Bulldozer: Loaded texture image via ResourceLoader: ", path);
                return ImageTexture::create_from_image(image);
            }
        }
    }
    
    // Fallback: Load image directly from file (works for non-imported files)
    Ref<Image> image = Image::load_from_file(path);
    if (image.is_valid()) {
        image->generate_mipmaps();
        UtilityFunctions::print("Bulldozer: Loaded texture directly from file: ", path);
        return ImageTexture::create_from_image(image);
    }
    
    UtilityFunctions::print("Bulldozer: Failed to load texture: ", path);
    return Ref<ImageTexture>();
}

void Bulldozer::create_vehicle_mesh() {
    // ========================================================================
    // ULTRA REALISTIC BULLDOZER - CAT D9 STYLE WITH PBR TEXTURES
    // ========================================================================
    
    Node3D *bulldozer_root = memnew(Node3D);
    bulldozer_root->set_name("BulldozerModel");
    add_child(bulldozer_root);
    
    // ========================================================================
    // LOAD PBR TEXTURES
    // ========================================================================
    String tex_path = "res://assets/textures/vehicles/bulldozer/";
    
    // Metal plate textures (for body)
    Ref<ImageTexture> metal_albedo = load_bulldozer_texture(tex_path + "metal_plate_albedo.jpg");
    Ref<ImageTexture> metal_normal = load_bulldozer_texture(tex_path + "metal_plate_normal.jpg");
    Ref<ImageTexture> metal_roughness = load_bulldozer_texture(tex_path + "metal_plate_roughness.jpg");
    Ref<ImageTexture> metal_metallic = load_bulldozer_texture(tex_path + "metal_plate_metallic.jpg");
    
    // Rubber textures (for tracks)
    Ref<ImageTexture> rubber_albedo = load_bulldozer_texture(tex_path + "rubber_albedo.jpg");
    Ref<ImageTexture> rubber_normal = load_bulldozer_texture(tex_path + "rubber_normal.jpg");
    Ref<ImageTexture> rubber_roughness = load_bulldozer_texture(tex_path + "rubber_roughness.jpg");
    
    // Rusty/worn metal textures (for blade)
    Ref<ImageTexture> rusty_albedo = load_bulldozer_texture(tex_path + "rusty_metal_albedo.jpg");
    Ref<ImageTexture> rusty_normal = load_bulldozer_texture(tex_path + "rusty_metal_normal.jpg");
    Ref<ImageTexture> rusty_roughness = load_bulldozer_texture(tex_path + "rusty_metal_roughness.jpg");
    
    // Yellow paint textures (for body)
    Ref<ImageTexture> yellow_albedo = load_bulldozer_texture(tex_path + "yellow_paint_albedo.jpg");
    Ref<ImageTexture> yellow_normal = load_bulldozer_texture(tex_path + "yellow_paint_normal.jpg");
    Ref<ImageTexture> yellow_roughness = load_bulldozer_texture(tex_path + "yellow_paint_roughness.jpg");
    
    // ========================================================================
    // MATERIALS WITH PBR TEXTURES
    // ========================================================================
    
    // Caterpillar Yellow body paint with weathering texture
    Ref<StandardMaterial3D> yellow_mat;
    yellow_mat.instantiate();
    if (yellow_albedo.is_valid()) {
        yellow_mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, yellow_albedo);
        yellow_mat->set_albedo(Color(1.0f, 0.85f, 0.2f)); // Tint to CAT Yellow
    } else {
        yellow_mat->set_albedo(Color(0.92f, 0.72f, 0.08f));
    }
    if (yellow_normal.is_valid()) {
        yellow_mat->set_texture(StandardMaterial3D::TEXTURE_NORMAL, yellow_normal);
        yellow_mat->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        yellow_mat->set_normal_scale(0.8f);
    }
    if (yellow_roughness.is_valid()) {
        yellow_mat->set_texture(StandardMaterial3D::TEXTURE_ROUGHNESS, yellow_roughness);
    } else {
        yellow_mat->set_roughness(0.55f);
    }
    yellow_mat->set_metallic(0.1f);
    yellow_mat->set_specular(0.4f);
    yellow_mat->set_uv1_scale(Vector3(3.0f, 3.0f, 1.0f));
    
    // Darker yellow for panels/accents
    Ref<StandardMaterial3D> dark_yellow_mat;
    dark_yellow_mat.instantiate();
    if (yellow_albedo.is_valid()) {
        dark_yellow_mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, yellow_albedo);
        dark_yellow_mat->set_albedo(Color(0.85f, 0.65f, 0.1f)); // Darker tint
    } else {
        dark_yellow_mat->set_albedo(Color(0.78f, 0.58f, 0.05f));
    }
    if (yellow_normal.is_valid()) {
        dark_yellow_mat->set_texture(StandardMaterial3D::TEXTURE_NORMAL, yellow_normal);
        dark_yellow_mat->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        dark_yellow_mat->set_normal_scale(1.0f);
    }
    dark_yellow_mat->set_roughness(0.65f);
    dark_yellow_mat->set_metallic(0.08f);
    dark_yellow_mat->set_uv1_scale(Vector3(4.0f, 4.0f, 1.0f));
    
    // Rubber track material with texture
    Ref<StandardMaterial3D> rubber_mat;
    rubber_mat.instantiate();
    if (rubber_albedo.is_valid()) {
        rubber_mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, rubber_albedo);
        rubber_mat->set_albedo(Color(0.4f, 0.4f, 0.45f)); // Lighter tint to show texture
    } else {
        rubber_mat->set_albedo(Color(0.12f, 0.12f, 0.14f));
    }
    if (rubber_normal.is_valid()) {
        rubber_mat->set_texture(StandardMaterial3D::TEXTURE_NORMAL, rubber_normal);
        rubber_mat->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        rubber_mat->set_normal_scale(1.5f);
    }
    if (rubber_roughness.is_valid()) {
        rubber_mat->set_texture(StandardMaterial3D::TEXTURE_ROUGHNESS, rubber_roughness);
    } else {
        rubber_mat->set_roughness(0.92f);
    }
    rubber_mat->set_metallic(0.0f);
    rubber_mat->set_uv1_scale(Vector3(4.0f, 1.0f, 1.0f));
    
    // Clean steel/chrome
    Ref<StandardMaterial3D> steel_mat;
    steel_mat.instantiate();
    if (metal_albedo.is_valid()) {
        steel_mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, metal_albedo);
        steel_mat->set_albedo(Color(0.7f, 0.72f, 0.75f)); // Steel tint
    } else {
        steel_mat->set_albedo(Color(0.55f, 0.57f, 0.6f));
    }
    if (metal_normal.is_valid()) {
        steel_mat->set_texture(StandardMaterial3D::TEXTURE_NORMAL, metal_normal);
        steel_mat->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        steel_mat->set_normal_scale(1.0f);
    }
    if (metal_metallic.is_valid()) {
        steel_mat->set_texture(StandardMaterial3D::TEXTURE_METALLIC, metal_metallic);
    }
    steel_mat->set_roughness(0.3f);
    steel_mat->set_metallic(0.9f);
    steel_mat->set_specular(0.6f);
    steel_mat->set_uv1_scale(Vector3(2.0f, 2.0f, 1.0f));
    
    // Dark steel (unpainted metal parts)
    Ref<StandardMaterial3D> dark_steel_mat;
    dark_steel_mat.instantiate();
    if (metal_albedo.is_valid()) {
        dark_steel_mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, metal_albedo);
        dark_steel_mat->set_albedo(Color(0.35f, 0.36f, 0.38f));
    } else {
        dark_steel_mat->set_albedo(Color(0.28f, 0.29f, 0.31f));
    }
    if (metal_normal.is_valid()) {
        dark_steel_mat->set_texture(StandardMaterial3D::TEXTURE_NORMAL, metal_normal);
        dark_steel_mat->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
    }
    dark_steel_mat->set_roughness(0.45f);
    dark_steel_mat->set_metallic(0.85f);
    dark_steel_mat->set_uv1_scale(Vector3(3.0f, 3.0f, 1.0f));
    
    // Worn/rusty blade steel with visible texture
    Ref<StandardMaterial3D> blade_mat;
    blade_mat.instantiate();
    if (rusty_albedo.is_valid()) {
        blade_mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, rusty_albedo);
        blade_mat->set_albedo(Color(0.9f, 0.85f, 0.8f)); // Lighter tint to show texture
    } else {
        blade_mat->set_albedo(Color(0.42f, 0.4f, 0.38f));
    }
    if (rusty_normal.is_valid()) {
        blade_mat->set_texture(StandardMaterial3D::TEXTURE_NORMAL, rusty_normal);
        blade_mat->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        blade_mat->set_normal_scale(2.0f);
    }
    if (rusty_roughness.is_valid()) {
        blade_mat->set_texture(StandardMaterial3D::TEXTURE_ROUGHNESS, rusty_roughness);
    } else {
        blade_mat->set_roughness(0.7f);
    }
    blade_mat->set_metallic(0.65f);
    blade_mat->set_uv1_scale(Vector3(2.0f, 1.5f, 1.0f));
    
    // Exhaust pipe (heat-tinted metal)
    Ref<StandardMaterial3D> exhaust_mat;
    exhaust_mat.instantiate();
    exhaust_mat->set_albedo(Color(0.32f, 0.28f, 0.25f));
    if (metal_normal.is_valid()) {
        exhaust_mat->set_texture(StandardMaterial3D::TEXTURE_NORMAL, metal_normal);
        exhaust_mat->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
    }
    exhaust_mat->set_roughness(0.75f);
    exhaust_mat->set_metallic(0.5f);
    
    // Glass/windows
    Ref<StandardMaterial3D> glass_mat;
    glass_mat.instantiate();
    glass_mat->set_albedo(Color(0.12f, 0.18f, 0.22f, 0.65f));
    glass_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    glass_mat->set_roughness(0.02f);
    glass_mat->set_metallic(0.2f);
    glass_mat->set_specular(0.8f);
    
    // Red safety/lights
    Ref<StandardMaterial3D> red_mat;
    red_mat.instantiate();
    red_mat->set_albedo(Color(0.85f, 0.12f, 0.1f));
    red_mat->set_roughness(0.35f);
    red_mat->set_metallic(0.1f);
    
    // Orange warning
    Ref<StandardMaterial3D> orange_mat;
    orange_mat.instantiate();
    orange_mat->set_albedo(Color(1.0f, 0.55f, 0.0f));
    orange_mat->set_roughness(0.4f);
    
    // Black matte (vents, grilles)
    Ref<StandardMaterial3D> black_mat;
    black_mat.instantiate();
    black_mat->set_albedo(Color(0.05f, 0.05f, 0.06f));
    black_mat->set_roughness(0.85f);
    black_mat->set_metallic(0.1f);
    
    // Headlight emissive
    Ref<StandardMaterial3D> headlight_mat;
    headlight_mat.instantiate();
    headlight_mat->set_albedo(Color(1.0f, 0.98f, 0.92f));
    headlight_mat->set_feature(StandardMaterial3D::FEATURE_EMISSION, true);
    headlight_mat->set_emission(Color(1.0f, 0.95f, 0.85f));
    headlight_mat->set_emission_energy_multiplier(2.0f);
    headlight_mat->set_roughness(0.1f);
    
    // Warning beacon emissive
    Ref<StandardMaterial3D> beacon_mat;
    beacon_mat.instantiate();
    beacon_mat->set_albedo(Color(1.0f, 0.6f, 0.0f));
    beacon_mat->set_feature(StandardMaterial3D::FEATURE_EMISSION, true);
    beacon_mat->set_emission(Color(1.0f, 0.5f, 0.0f));
    beacon_mat->set_emission_energy_multiplier(1.5f);
    
    // ========================================================================
    // TRACK SYSTEM - Left and Right (with detailed treads)
    // ========================================================================
    float track_length = 2.8f;
    float track_width = 0.5f;
    float track_height = 0.55f;
    float track_offset_x = 1.05f;
    
    for (int side = -1; side <= 1; side += 2) {
        float x_offset = side * track_offset_x;
        
        // Main track housing (internal structure)
        MeshInstance3D *track_housing = memnew(MeshInstance3D);
        track_housing->set_name("TrackHousing");
        bulldozer_root->add_child(track_housing);
        
        Ref<BoxMesh> housing_mesh;
        housing_mesh.instantiate();
        housing_mesh->set_size(Vector3(track_width, track_height, track_length));
        track_housing->set_mesh(housing_mesh);
        track_housing->set_position(Vector3(x_offset, track_height / 2.0f, 0));
        track_housing->set_surface_override_material(0, dark_steel_mat);
        
        // Rubber track pad with texture
        MeshInstance3D *track_pad = memnew(MeshInstance3D);
        track_pad->set_name("TrackPad");
        bulldozer_root->add_child(track_pad);
        
        Ref<BoxMesh> pad_mesh;
        pad_mesh.instantiate();
        pad_mesh->set_size(Vector3(track_width + 0.08f, track_height + 0.1f, track_length + 0.05f));
        track_pad->set_mesh(pad_mesh);
        track_pad->set_position(Vector3(x_offset, track_height / 2.0f, 0));
        track_pad->set_surface_override_material(0, rubber_mat);
        
        // Track segments (grouser bars) - steel cleats
        int num_segments = 16;
        for (int i = 0; i < num_segments; i++) {
            MeshInstance3D *segment = memnew(MeshInstance3D);
            segment->set_name("TrackSegment");
            bulldozer_root->add_child(segment);
            
            Ref<BoxMesh> seg_mesh;
            seg_mesh.instantiate();
            seg_mesh->set_size(Vector3(track_width + 0.14f, 0.05f, 0.06f));
            segment->set_mesh(seg_mesh);
            float z = -track_length / 2.0f + 0.1f + i * (track_length / num_segments);
            segment->set_position(Vector3(x_offset, 0.025f, z));
            segment->set_surface_override_material(0, steel_mat);
        }
        
        // Drive sprocket (rear wheel)
        MeshInstance3D *sprocket = memnew(MeshInstance3D);
        sprocket->set_name("DriveSprocket");
        bulldozer_root->add_child(sprocket);
        
        Ref<CylinderMesh> sprocket_mesh;
        sprocket_mesh.instantiate();
        sprocket_mesh->set_top_radius(0.28f);
        sprocket_mesh->set_bottom_radius(0.28f);
        sprocket_mesh->set_height(0.15f);
        sprocket_mesh->set_radial_segments(12);
        sprocket->set_mesh(sprocket_mesh);
        sprocket->set_position(Vector3(x_offset + side * 0.28f, 0.32f, -track_length / 2.0f + 0.25f));
        sprocket->set_rotation_degrees(Vector3(0, 0, 90));
        sprocket->set_surface_override_material(0, yellow_mat);
        
        // Sprocket center hub
        MeshInstance3D *sprocket_hub = memnew(MeshInstance3D);
        sprocket_hub->set_name("SprocketHub");
        bulldozer_root->add_child(sprocket_hub);
        
        Ref<CylinderMesh> hub_mesh;
        hub_mesh.instantiate();
        hub_mesh->set_top_radius(0.1f);
        hub_mesh->set_bottom_radius(0.1f);
        hub_mesh->set_height(0.18f);
        sprocket_hub->set_mesh(hub_mesh);
        sprocket_hub->set_position(Vector3(x_offset + side * 0.32f, 0.32f, -track_length / 2.0f + 0.25f));
        sprocket_hub->set_rotation_degrees(Vector3(0, 0, 90));
        sprocket_hub->set_surface_override_material(0, dark_steel_mat);
        
        // Idler wheel (front)
        MeshInstance3D *idler = memnew(MeshInstance3D);
        idler->set_name("IdlerWheel");
        bulldozer_root->add_child(idler);
        
        Ref<CylinderMesh> idler_mesh;
        idler_mesh.instantiate();
        idler_mesh->set_top_radius(0.22f);
        idler_mesh->set_bottom_radius(0.22f);
        idler_mesh->set_height(0.12f);
        idler->set_mesh(idler_mesh);
        idler->set_position(Vector3(x_offset + side * 0.25f, 0.28f, track_length / 2.0f - 0.2f));
        idler->set_rotation_degrees(Vector3(0, 0, 90));
        idler->set_surface_override_material(0, yellow_mat);
        
        // Road wheels (5 per side)
        for (int w = 0; w < 5; w++) {
            MeshInstance3D *wheel = memnew(MeshInstance3D);
            wheel->set_name("RoadWheel");
            bulldozer_root->add_child(wheel);
            
            Ref<CylinderMesh> wheel_mesh;
            wheel_mesh.instantiate();
            wheel_mesh->set_top_radius(0.15f);
            wheel_mesh->set_bottom_radius(0.15f);
            wheel_mesh->set_height(0.1f);
            wheel->set_mesh(wheel_mesh);
            float wz = -track_length / 2.0f + 0.5f + w * 0.5f;
            wheel->set_position(Vector3(x_offset + side * 0.2f, 0.18f, wz));
            wheel->set_rotation_degrees(Vector3(0, 0, 90));
            wheel->set_surface_override_material(0, steel_mat);
        }
        
        // Track roller (top)
        MeshInstance3D *roller = memnew(MeshInstance3D);
        roller->set_name("TrackRoller");
        bulldozer_root->add_child(roller);
        
        Ref<CylinderMesh> roller_mesh;
        roller_mesh.instantiate();
        roller_mesh->set_top_radius(0.08f);
        roller_mesh->set_bottom_radius(0.08f);
        roller_mesh->set_height(0.08f);
        roller->set_mesh(roller_mesh);
        roller->set_position(Vector3(x_offset + side * 0.22f, track_height + 0.1f, 0));
        roller->set_rotation_degrees(Vector3(0, 0, 90));
        roller->set_surface_override_material(0, yellow_mat);
        
        // Track guard/fender
        MeshInstance3D *guard = memnew(MeshInstance3D);
        guard->set_name("TrackGuard");
        bulldozer_root->add_child(guard);
        
        Ref<BoxMesh> guard_mesh;
        guard_mesh.instantiate();
        guard_mesh->set_size(Vector3(0.1f, 0.15f, track_length - 0.3f));
        guard->set_mesh(guard_mesh);
        guard->set_position(Vector3(x_offset + side * (track_width / 2.0f + 0.1f), track_height + 0.2f, 0));
        guard->set_surface_override_material(0, yellow_mat);
    }
    
    // ========================================================================
    // MAIN BODY / CHASSIS
    // ========================================================================
    
    // Lower hull
    MeshInstance3D *lower_hull = memnew(MeshInstance3D);
    lower_hull->set_name("LowerHull");
    bulldozer_root->add_child(lower_hull);
    
    Ref<BoxMesh> hull_mesh;
    hull_mesh.instantiate();
    hull_mesh->set_size(Vector3(1.8f, 0.4f, 2.4f));
    lower_hull->set_mesh(hull_mesh);
    lower_hull->set_position(Vector3(0, track_height + 0.2f, -0.1f));
    lower_hull->set_surface_override_material(0, yellow_mat);
    
    // Engine compartment (rear)
    MeshInstance3D *engine_comp = memnew(MeshInstance3D);
    engine_comp->set_name("EngineCompartment");
    bulldozer_root->add_child(engine_comp);
    
    Ref<BoxMesh> engine_mesh;
    engine_mesh.instantiate();
    engine_mesh->set_size(Vector3(1.6f, 0.8f, 1.2f));
    engine_comp->set_mesh(engine_mesh);
    engine_comp->set_position(Vector3(0, track_height + 0.6f, -0.9f));
    engine_comp->set_surface_override_material(0, yellow_mat);
    
    // Engine vents (side panels)
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *vent = memnew(MeshInstance3D);
        vent->set_name("EngineVent");
        bulldozer_root->add_child(vent);
        
        Ref<BoxMesh> vent_mesh;
        vent_mesh.instantiate();
        vent_mesh->set_size(Vector3(0.05f, 0.5f, 0.8f));
        vent->set_mesh(vent_mesh);
        vent->set_position(Vector3(side * 0.83f, track_height + 0.55f, -0.9f));
        vent->set_surface_override_material(0, dark_steel_mat);
        
        // Vent slats
        for (int s = 0; s < 6; s++) {
            MeshInstance3D *slat = memnew(MeshInstance3D);
            slat->set_name("VentSlat");
            bulldozer_root->add_child(slat);
            
            Ref<BoxMesh> slat_mesh;
            slat_mesh.instantiate();
            slat_mesh->set_size(Vector3(0.08f, 0.02f, 0.7f));
            slat->set_mesh(slat_mesh);
            float y = track_height + 0.35f + s * 0.08f;
            slat->set_position(Vector3(side * 0.85f, y, -0.9f));
            slat->set_surface_override_material(0, dark_steel_mat);
        }
    }
    
    // Engine hood top
    MeshInstance3D *hood = memnew(MeshInstance3D);
    hood->set_name("EngineHood");
    bulldozer_root->add_child(hood);
    
    Ref<BoxMesh> hood_mesh;
    hood_mesh.instantiate();
    hood_mesh->set_size(Vector3(1.5f, 0.08f, 1.1f));
    hood->set_mesh(hood_mesh);
    hood->set_position(Vector3(0, track_height + 1.04f, -0.9f));
    hood->set_surface_override_material(0, dark_yellow_mat);
    
    // ========================================================================
    // EXHAUST SYSTEM
    // ========================================================================
    
    // Main exhaust stack
    MeshInstance3D *exhaust_stack = memnew(MeshInstance3D);
    exhaust_stack->set_name("ExhaustStack");
    bulldozer_root->add_child(exhaust_stack);
    
    Ref<CylinderMesh> exhaust_mesh;
    exhaust_mesh.instantiate();
    exhaust_mesh->set_top_radius(0.08f);
    exhaust_mesh->set_bottom_radius(0.1f);
    exhaust_mesh->set_height(0.8f);
    exhaust_stack->set_mesh(exhaust_mesh);
    exhaust_stack->set_position(Vector3(-0.5f, track_height + 1.4f, -1.1f));
    exhaust_stack->set_surface_override_material(0, exhaust_mat);
    
    // Exhaust cap
    MeshInstance3D *exhaust_cap = memnew(MeshInstance3D);
    exhaust_cap->set_name("ExhaustCap");
    bulldozer_root->add_child(exhaust_cap);
    
    Ref<CylinderMesh> cap_mesh;
    cap_mesh.instantiate();
    cap_mesh->set_top_radius(0.12f);
    cap_mesh->set_bottom_radius(0.1f);
    cap_mesh->set_height(0.1f);
    exhaust_cap->set_mesh(cap_mesh);
    exhaust_cap->set_position(Vector3(-0.5f, track_height + 1.85f, -1.1f));
    exhaust_cap->set_surface_override_material(0, dark_steel_mat);
    
    // Pre-cleaner (air intake)
    MeshInstance3D *precleaner = memnew(MeshInstance3D);
    precleaner->set_name("PreCleaner");
    bulldozer_root->add_child(precleaner);
    
    Ref<CylinderMesh> precleaner_mesh;
    precleaner_mesh.instantiate();
    precleaner_mesh->set_top_radius(0.12f);
    precleaner_mesh->set_bottom_radius(0.12f);
    precleaner_mesh->set_height(0.5f);
    precleaner->set_mesh(precleaner_mesh);
    precleaner->set_position(Vector3(0.5f, track_height + 1.25f, -1.1f));
    precleaner->set_surface_override_material(0, yellow_mat);
    
    // Pre-cleaner cap
    MeshInstance3D *precleaner_cap = memnew(MeshInstance3D);
    precleaner_cap->set_name("PreCleanerCap");
    bulldozer_root->add_child(precleaner_cap);
    
    Ref<CylinderMesh> pc_cap_mesh;
    pc_cap_mesh.instantiate();
    pc_cap_mesh->set_top_radius(0.05f);
    pc_cap_mesh->set_bottom_radius(0.14f);
    pc_cap_mesh->set_height(0.15f);
    precleaner_cap->set_mesh(pc_cap_mesh);
    precleaner_cap->set_position(Vector3(0.5f, track_height + 1.58f, -1.1f));
    precleaner_cap->set_surface_override_material(0, dark_steel_mat);
    
    // ========================================================================
    // OPERATOR CAB
    // ========================================================================
    
    // Cab base
    MeshInstance3D *cab_base = memnew(MeshInstance3D);
    cab_base->set_name("CabBase");
    bulldozer_root->add_child(cab_base);
    
    Ref<BoxMesh> cab_base_mesh;
    cab_base_mesh.instantiate();
    cab_base_mesh->set_size(Vector3(1.4f, 0.15f, 1.3f));
    cab_base->set_mesh(cab_base_mesh);
    cab_base->set_position(Vector3(0, track_height + 0.47f, 0.4f));
    cab_base->set_surface_override_material(0, dark_yellow_mat);
    
    // Cab frame - main structure
    MeshInstance3D *cab_frame = memnew(MeshInstance3D);
    cab_frame->set_name("CabFrame");
    bulldozer_root->add_child(cab_frame);
    
    Ref<BoxMesh> cab_mesh;
    cab_mesh.instantiate();
    cab_mesh->set_size(Vector3(1.3f, 1.0f, 1.2f));
    cab_frame->set_mesh(cab_mesh);
    cab_frame->set_position(Vector3(0, track_height + 1.05f, 0.4f));
    cab_frame->set_surface_override_material(0, yellow_mat);
    
    // Cab roof
    MeshInstance3D *cab_roof = memnew(MeshInstance3D);
    cab_roof->set_name("CabRoof");
    bulldozer_root->add_child(cab_roof);
    
    Ref<BoxMesh> roof_mesh;
    roof_mesh.instantiate();
    roof_mesh->set_size(Vector3(1.45f, 0.1f, 1.35f));
    cab_roof->set_mesh(roof_mesh);
    cab_roof->set_position(Vector3(0, track_height + 1.6f, 0.4f));
    cab_roof->set_surface_override_material(0, yellow_mat);
    
    // Cab ROPS (Roll-Over Protective Structure) posts
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            MeshInstance3D *post = memnew(MeshInstance3D);
            post->set_name("ROPSPost");
            bulldozer_root->add_child(post);
            
            Ref<BoxMesh> post_mesh;
            post_mesh.instantiate();
            post_mesh->set_size(Vector3(0.08f, 1.0f, 0.08f));
            post->set_mesh(post_mesh);
            post->set_position(Vector3(x * 0.6f, track_height + 1.05f, 0.4f + z * 0.55f));
            post->set_surface_override_material(0, dark_steel_mat);
        }
    }
    
    // Windows
    // Front window
    MeshInstance3D *front_window = memnew(MeshInstance3D);
    front_window->set_name("FrontWindow");
    bulldozer_root->add_child(front_window);
    
    Ref<BoxMesh> fw_mesh;
    fw_mesh.instantiate();
    fw_mesh->set_size(Vector3(1.1f, 0.7f, 0.03f));
    front_window->set_mesh(fw_mesh);
    front_window->set_position(Vector3(0, track_height + 1.15f, 1.02f));
    front_window->set_surface_override_material(0, glass_mat);
    
    // Side windows
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *side_window = memnew(MeshInstance3D);
        side_window->set_name("SideWindow");
        bulldozer_root->add_child(side_window);
        
        Ref<BoxMesh> sw_mesh;
        sw_mesh.instantiate();
        sw_mesh->set_size(Vector3(0.03f, 0.6f, 0.9f));
        side_window->set_mesh(sw_mesh);
        side_window->set_position(Vector3(side * 0.67f, track_height + 1.15f, 0.45f));
        side_window->set_surface_override_material(0, glass_mat);
    }
    
    // Rear window
    MeshInstance3D *rear_window = memnew(MeshInstance3D);
    rear_window->set_name("RearWindow");
    bulldozer_root->add_child(rear_window);
    
    Ref<BoxMesh> rw_mesh;
    rw_mesh.instantiate();
    rw_mesh->set_size(Vector3(1.0f, 0.5f, 0.03f));
    rear_window->set_mesh(rw_mesh);
    rear_window->set_position(Vector3(0, track_height + 1.2f, -0.22f));
    rear_window->set_surface_override_material(0, glass_mat);
    
    // ========================================================================
    // BLADE ASSEMBLY (Front)
    // ========================================================================
    
    float blade_z = track_length / 2.0f + 0.4f;
    
    // Main blade
    MeshInstance3D *blade = memnew(MeshInstance3D);
    blade->set_name("MainBlade");
    bulldozer_root->add_child(blade);
    
    Ref<BoxMesh> blade_mesh;
    blade_mesh.instantiate();
    blade_mesh->set_size(Vector3(3.0f, 0.9f, 0.15f));
    blade->set_mesh(blade_mesh);
    blade->set_position(Vector3(0, 0.5f, blade_z));
    blade->set_surface_override_material(0, blade_mat);
    
    // Blade cutting edge
    MeshInstance3D *cutting_edge = memnew(MeshInstance3D);
    cutting_edge->set_name("CuttingEdge");
    bulldozer_root->add_child(cutting_edge);
    
    Ref<BoxMesh> edge_mesh;
    edge_mesh.instantiate();
    edge_mesh->set_size(Vector3(3.0f, 0.08f, 0.2f));
    cutting_edge->set_mesh(edge_mesh);
    cutting_edge->set_position(Vector3(0, 0.08f, blade_z + 0.05f));
    cutting_edge->set_surface_override_material(0, steel_mat);
    
    // Blade end caps
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *end_cap = memnew(MeshInstance3D);
        end_cap->set_name("BladeEndCap");
        bulldozer_root->add_child(end_cap);
        
        Ref<BoxMesh> ec_mesh;
        ec_mesh.instantiate();
        ec_mesh->set_size(Vector3(0.12f, 0.95f, 0.25f));
        end_cap->set_mesh(ec_mesh);
        end_cap->set_position(Vector3(side * 1.55f, 0.52f, blade_z + 0.05f));
        end_cap->set_surface_override_material(0, yellow_mat);
    }
    
    // Blade reinforcement ribs
    for (int r = -2; r <= 2; r++) {
        MeshInstance3D *rib = memnew(MeshInstance3D);
        rib->set_name("BladeRib");
        bulldozer_root->add_child(rib);
        
        Ref<BoxMesh> rib_mesh;
        rib_mesh.instantiate();
        rib_mesh->set_size(Vector3(0.06f, 0.7f, 0.08f));
        rib->set_mesh(rib_mesh);
        rib->set_position(Vector3(r * 0.6f, 0.45f, blade_z - 0.12f));
        rib->set_surface_override_material(0, dark_steel_mat);
    }
    
    // Push arms (connect blade to body)
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *push_arm = memnew(MeshInstance3D);
        push_arm->set_name("PushArm");
        bulldozer_root->add_child(push_arm);
        
        Ref<BoxMesh> arm_mesh;
        arm_mesh.instantiate();
        arm_mesh->set_size(Vector3(0.15f, 0.2f, 1.0f));
        push_arm->set_mesh(arm_mesh);
        push_arm->set_position(Vector3(side * 0.8f, 0.45f, blade_z - 0.6f));
        push_arm->set_surface_override_material(0, yellow_mat);
        
        // Arm brace
        MeshInstance3D *brace = memnew(MeshInstance3D);
        brace->set_name("PushArmBrace");
        bulldozer_root->add_child(brace);
        
        Ref<BoxMesh> brace_mesh;
        brace_mesh.instantiate();
        brace_mesh->set_size(Vector3(0.1f, 0.4f, 0.1f));
        brace->set_mesh(brace_mesh);
        brace->set_position(Vector3(side * 0.8f, 0.55f, blade_z - 0.2f));
        brace->set_surface_override_material(0, dark_steel_mat);
    }
    
    // Hydraulic cylinders for blade
    for (int side = -1; side <= 1; side += 2) {
        // Cylinder body
        MeshInstance3D *hyd_cyl = memnew(MeshInstance3D);
        hyd_cyl->set_name("HydraulicCylinder");
        bulldozer_root->add_child(hyd_cyl);
        
        Ref<CylinderMesh> cyl_mesh;
        cyl_mesh.instantiate();
        cyl_mesh->set_top_radius(0.06f);
        cyl_mesh->set_bottom_radius(0.06f);
        cyl_mesh->set_height(0.6f);
        hyd_cyl->set_mesh(cyl_mesh);
        hyd_cyl->set_position(Vector3(side * 0.5f, 0.7f, blade_z - 0.5f));
        hyd_cyl->set_rotation_degrees(Vector3(30, 0, 0));
        hyd_cyl->set_surface_override_material(0, steel_mat);
        
        // Cylinder rod
        MeshInstance3D *hyd_rod = memnew(MeshInstance3D);
        hyd_rod->set_name("HydraulicRod");
        bulldozer_root->add_child(hyd_rod);
        
        Ref<CylinderMesh> rod_mesh;
        rod_mesh.instantiate();
        rod_mesh->set_top_radius(0.03f);
        rod_mesh->set_bottom_radius(0.03f);
        rod_mesh->set_height(0.35f);
        hyd_rod->set_mesh(rod_mesh);
        hyd_rod->set_position(Vector3(side * 0.5f, 0.5f, blade_z - 0.25f));
        hyd_rod->set_rotation_degrees(Vector3(30, 0, 0));
        hyd_rod->set_surface_override_material(0, steel_mat);
    }
    
    // ========================================================================
    // RIPPER (Rear attachment)
    // ========================================================================
    
    float ripper_z = -track_length / 2.0f - 0.3f;
    
    // Ripper frame
    MeshInstance3D *ripper_frame = memnew(MeshInstance3D);
    ripper_frame->set_name("RipperFrame");
    bulldozer_root->add_child(ripper_frame);
    
    Ref<BoxMesh> rf_mesh;
    rf_mesh.instantiate();
    rf_mesh->set_size(Vector3(1.4f, 0.2f, 0.15f));
    ripper_frame->set_mesh(rf_mesh);
    ripper_frame->set_position(Vector3(0, 0.5f, ripper_z));
    ripper_frame->set_surface_override_material(0, yellow_mat);
    
    // Ripper shanks (3 teeth)
    for (int t = -1; t <= 1; t++) {
        MeshInstance3D *shank = memnew(MeshInstance3D);
        shank->set_name("RipperShank");
        bulldozer_root->add_child(shank);
        
        Ref<BoxMesh> shank_mesh;
        shank_mesh.instantiate();
        shank_mesh->set_size(Vector3(0.08f, 0.5f, 0.12f));
        shank->set_mesh(shank_mesh);
        shank->set_position(Vector3(t * 0.4f, 0.2f, ripper_z - 0.1f));
        shank->set_rotation_degrees(Vector3(15, 0, 0));
        shank->set_surface_override_material(0, dark_steel_mat);
        
        // Ripper tip
        MeshInstance3D *tip = memnew(MeshInstance3D);
        tip->set_name("RipperTip");
        bulldozer_root->add_child(tip);
        
        Ref<BoxMesh> tip_mesh;
        tip_mesh.instantiate();
        tip_mesh->set_size(Vector3(0.1f, 0.15f, 0.08f));
        tip->set_mesh(tip_mesh);
        tip->set_position(Vector3(t * 0.4f, -0.02f, ripper_z - 0.18f));
        tip->set_surface_override_material(0, blade_mat);
    }
    
    // Ripper arms
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *arm = memnew(MeshInstance3D);
        arm->set_name("RipperArm");
        bulldozer_root->add_child(arm);
        
        Ref<BoxMesh> arm_mesh;
        arm_mesh.instantiate();
        arm_mesh->set_size(Vector3(0.12f, 0.15f, 0.5f));
        arm->set_mesh(arm_mesh);
        arm->set_position(Vector3(side * 0.65f, 0.45f, ripper_z + 0.2f));
        arm->set_surface_override_material(0, yellow_mat);
    }
    
    // ========================================================================
    // LIGHTS AND DETAILS
    // ========================================================================
    
    // Front work lights (on cab roof)
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *work_light = memnew(MeshInstance3D);
        work_light->set_name("WorkLight");
        bulldozer_root->add_child(work_light);
        
        Ref<BoxMesh> wl_mesh;
        wl_mesh.instantiate();
        wl_mesh->set_size(Vector3(0.12f, 0.08f, 0.06f));
        work_light->set_mesh(wl_mesh);
        work_light->set_position(Vector3(side * 0.45f, track_height + 1.68f, 0.9f));
        work_light->set_surface_override_material(0, headlight_mat);
    }
    
    // Rear lights
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *rear_light = memnew(MeshInstance3D);
        rear_light->set_name("RearLight");
        bulldozer_root->add_child(rear_light);
        
        Ref<CylinderMesh> rl_mesh;
        rl_mesh.instantiate();
        rl_mesh->set_top_radius(0.04f);
        rl_mesh->set_bottom_radius(0.04f);
        rl_mesh->set_height(0.03f);
        rear_light->set_mesh(rl_mesh);
        rear_light->set_position(Vector3(side * 0.55f, track_height + 0.8f, -1.48f));
        rear_light->set_rotation_degrees(Vector3(90, 0, 0));
        rear_light->set_surface_override_material(0, red_mat);
    }
    
    // Warning beacon on cab roof
    MeshInstance3D *beacon = memnew(MeshInstance3D);
    beacon->set_name("WarningBeacon");
    bulldozer_root->add_child(beacon);
    
    Ref<CylinderMesh> beacon_mesh;
    beacon_mesh.instantiate();
    beacon_mesh->set_top_radius(0.06f);
    beacon_mesh->set_bottom_radius(0.08f);
    beacon_mesh->set_height(0.1f);
    beacon->set_mesh(beacon_mesh);
    beacon->set_position(Vector3(0, track_height + 1.7f, 0.4f));
    beacon->set_surface_override_material(0, orange_mat);
    
    // Mirrors
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *mirror_arm = memnew(MeshInstance3D);
        mirror_arm->set_name("MirrorArm");
        bulldozer_root->add_child(mirror_arm);
        
        Ref<CylinderMesh> ma_mesh;
        ma_mesh.instantiate();
        ma_mesh->set_top_radius(0.015f);
        ma_mesh->set_bottom_radius(0.015f);
        ma_mesh->set_height(0.25f);
        mirror_arm->set_mesh(ma_mesh);
        mirror_arm->set_position(Vector3(side * 0.8f, track_height + 1.35f, 0.85f));
        mirror_arm->set_rotation_degrees(Vector3(0, 0, side * -30));
        mirror_arm->set_surface_override_material(0, dark_steel_mat);
        
        MeshInstance3D *mirror = memnew(MeshInstance3D);
        mirror->set_name("Mirror");
        bulldozer_root->add_child(mirror);
        
        Ref<BoxMesh> m_mesh;
        m_mesh.instantiate();
        m_mesh->set_size(Vector3(0.15f, 0.1f, 0.02f));
        mirror->set_mesh(m_mesh);
        mirror->set_position(Vector3(side * 0.95f, track_height + 1.45f, 0.88f));
        mirror->set_surface_override_material(0, glass_mat);
    }
    
    // Grab handles
    for (int side = -1; side <= 1; side += 2) {
        MeshInstance3D *handle = memnew(MeshInstance3D);
        handle->set_name("GrabHandle");
        bulldozer_root->add_child(handle);
        
        Ref<CylinderMesh> h_mesh;
        h_mesh.instantiate();
        h_mesh->set_top_radius(0.015f);
        h_mesh->set_bottom_radius(0.015f);
        h_mesh->set_height(0.25f);
        handle->set_mesh(h_mesh);
        handle->set_position(Vector3(side * 0.7f, track_height + 0.9f, 0.95f));
        handle->set_surface_override_material(0, dark_steel_mat);
    }
    
    // Fuel tank
    MeshInstance3D *fuel_tank = memnew(MeshInstance3D);
    fuel_tank->set_name("FuelTank");
    bulldozer_root->add_child(fuel_tank);
    
    Ref<CylinderMesh> tank_mesh;
    tank_mesh.instantiate();
    tank_mesh->set_top_radius(0.2f);
    tank_mesh->set_bottom_radius(0.2f);
    tank_mesh->set_height(0.6f);
    fuel_tank->set_mesh(tank_mesh);
    fuel_tank->set_position(Vector3(-0.85f, track_height + 0.5f, 0.0f));
    fuel_tank->set_rotation_degrees(Vector3(0, 0, 90));
    fuel_tank->set_surface_override_material(0, yellow_mat);
    
    // Fuel cap
    MeshInstance3D *fuel_cap = memnew(MeshInstance3D);
    fuel_cap->set_name("FuelCap");
    bulldozer_root->add_child(fuel_cap);
    
    Ref<CylinderMesh> fc_mesh;
    fc_mesh.instantiate();
    fc_mesh->set_top_radius(0.05f);
    fc_mesh->set_bottom_radius(0.05f);
    fc_mesh->set_height(0.03f);
    fuel_cap->set_mesh(fc_mesh);
    fuel_cap->set_position(Vector3(-0.85f, track_height + 0.72f, 0.0f));
    fuel_cap->set_surface_override_material(0, dark_steel_mat);
    
    // Toolbox
    MeshInstance3D *toolbox = memnew(MeshInstance3D);
    toolbox->set_name("Toolbox");
    bulldozer_root->add_child(toolbox);
    
    Ref<BoxMesh> tb_mesh;
    tb_mesh.instantiate();
    tb_mesh->set_size(Vector3(0.25f, 0.15f, 0.4f));
    toolbox->set_mesh(tb_mesh);
    toolbox->set_position(Vector3(0.75f, track_height + 0.47f, -0.2f));
    toolbox->set_surface_override_material(0, dark_steel_mat);
    
    // Hydraulic tank
    MeshInstance3D *hyd_tank = memnew(MeshInstance3D);
    hyd_tank->set_name("HydraulicTank");
    bulldozer_root->add_child(hyd_tank);
    
    Ref<BoxMesh> ht_mesh;
    ht_mesh.instantiate();
    ht_mesh->set_size(Vector3(0.3f, 0.3f, 0.3f));
    hyd_tank->set_mesh(ht_mesh);
    hyd_tank->set_position(Vector3(0.7f, track_height + 0.55f, 0.3f));
    hyd_tank->set_surface_override_material(0, yellow_mat);
    
    // Decals/stripes (black hazard stripes on blade)
    for (int s = 0; s < 4; s++) {
        MeshInstance3D *stripe = memnew(MeshInstance3D);
        stripe->set_name("HazardStripe");
        bulldozer_root->add_child(stripe);
        
        Ref<BoxMesh> stripe_mesh;
        stripe_mesh.instantiate();
        stripe_mesh->set_size(Vector3(0.15f, 0.88f, 0.01f));
        stripe->set_mesh(stripe_mesh);
        stripe->set_position(Vector3(-1.2f + s * 0.8f, 0.5f, blade_z + 0.09f));
        stripe->set_rotation_degrees(Vector3(0, 0, 20));
        stripe->set_surface_override_material(0, s % 2 == 0 ? black_mat : orange_mat);
    }
    
    // Store reference for selection highlighting
    mesh_instance = memnew(MeshInstance3D);
    mesh_instance->set_name("SelectionHelper");
    add_child(mesh_instance);
    
    Ref<BoxMesh> selection_mesh;
    selection_mesh.instantiate();
    selection_mesh->set_size(Vector3(2.0f, 0.01f, 3.0f));
    mesh_instance->set_mesh(selection_mesh);
    mesh_instance->set_position(Vector3(0, 0.01f, 0));
    mesh_instance->set_visible(false);  // Hidden by default
    
    UtilityFunctions::print("Bulldozer: Created extremely detailed D9 Caterpillar model");
}

void Bulldozer::start_placing_building(int type) {
    if (is_constructing) {
        UtilityFunctions::print("Bulldozer: Cannot place building while constructing");
        return;
    }
    
    placing_type = static_cast<BuildingType>(type + 1);  // +1 because NONE is 0
    is_placing_building = true;
    
    create_ghost_building(placing_type);
    
    emit_signal("placing_building_started", type);
    UtilityFunctions::print("Bulldozer: Started placing building type ", type);
}

void Bulldozer::cancel_placing() {
    if (!is_placing_building) return;
    
    is_placing_building = false;
    placing_type = BuildingType::NONE;
    remove_ghost_building();
    
    emit_signal("placing_building_cancelled");
    UtilityFunctions::print("Bulldozer: Cancelled placing");
}

void Bulldozer::confirm_build_location(const Vector3 &location) {
    if (!is_placing_building) return;
    
    // Check if placement is valid
    if (!check_placement_valid(location, current_ghost_size)) {
        UtilityFunctions::print("Bulldozer: Cannot place building here - location blocked!");
        return;
    }
    
    build_location = location;
    current_build_type = placing_type;
    
    is_placing_building = false;
    placing_type = BuildingType::NONE;
    remove_ghost_building();
    
    // Move bulldozer to the location
    move_to(location);
    
    UtilityFunctions::print("Bulldozer: Moving to build location: ", location);
}

void Bulldozer::update_ghost_position(const Vector3 &position) {
    if (ghost_building) {
        // Snap ghost to terrain height - use cached terrain generator from Vehicle base class
        Vector3 snapped_pos = position;
        if (cached_terrain_generator) {
            Variant height_result = cached_terrain_generator->call("get_height_at", position.x, position.z);
            if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
                snapped_pos.y = (float)height_result;
            }
        }
        ghost_building->set_global_position(snapped_pos);
        update_ghost_validity();
    }
}

void Bulldozer::start_construction() {
    if (current_build_type == BuildingType::NONE) return;
    
    is_constructing = true;
    construction_progress = 0.0f;
    
    emit_signal("construction_started", static_cast<int>(current_build_type) - 1);
    UtilityFunctions::print("Bulldozer: Started construction");
}

void Bulldozer::update_construction(double delta) {
    construction_progress += delta / construction_time;
    
    if (construction_progress >= 1.0f) {
        complete_construction();
    }
}

void Bulldozer::complete_construction() {
    Building *building = spawn_building(current_build_type, build_location);
    
    is_constructing = false;
    construction_progress = 0.0f;
    
    BuildingType completed_type = current_build_type;
    current_build_type = BuildingType::NONE;
    
    if (building) {
        emit_signal("construction_completed", building);
        UtilityFunctions::print("Bulldozer: Completed construction of ", building->get_building_name());
    }
    
    // Move away from the building
    Vector3 offset = get_global_position() - build_location;
    offset.y = 0;
    if (offset.length() < 0.1f) {
        offset = Vector3(3, 0, 0);
    } else {
        offset = offset.normalized() * 3.0f;
    }
    move_to(build_location + offset);
}

void Bulldozer::cancel_construction() {
    if (!is_constructing) return;
    
    is_constructing = false;
    construction_progress = 0.0f;
    current_build_type = BuildingType::NONE;
    
    emit_signal("construction_cancelled");
    UtilityFunctions::print("Bulldozer: Construction cancelled");
}

bool Bulldozer::get_is_constructing() const {
    return is_constructing;
}

float Bulldozer::get_construction_progress() const {
    return construction_progress;
}

bool Bulldozer::get_is_placing_building() const {
    return is_placing_building;
}

int Bulldozer::get_placing_type() const {
    return static_cast<int>(placing_type) - 1;  // -1 because NONE is 0
}

int Bulldozer::get_power_plant_cost() const {
    return power_plant_cost;
}

int Bulldozer::get_barracks_cost() const {
    return barracks_cost;
}

void Bulldozer::set_power_plant_model(const String &path) {
    power_plant_model = path;
}

String Bulldozer::get_power_plant_model() const {
    return power_plant_model;
}

void Bulldozer::set_barracks_model(const String &path) {
    barracks_model = path;
}

String Bulldozer::get_barracks_model() const {
    return barracks_model;
}

void Bulldozer::create_ghost_building(BuildingType type) {
    remove_ghost_building();
    
    ghost_building = memnew(Node3D);
    get_tree()->get_root()->add_child(ghost_building);
    
    ghost_mesh = memnew(MeshInstance3D);
    ghost_building->add_child(ghost_mesh);
    
    Ref<BoxMesh> mesh;
    mesh.instantiate();
    
    float width = 4.0f;
    float depth = 4.0f;
    float height = 3.0f;
    
    if (type == BuildingType::POWER_PLANT) {
        width = 8.0f;
        depth = 8.0f;
        height = 6.0f;
    } else if (type == BuildingType::BARRACKS) {
        // Match actual barracks dimensions: 24 wide, 16 deep, plus perimeter fence area
        width = 44.0f;   // BUILDING_WIDTH (24) + perimeter (20)
        depth = 41.0f;   // BUILDING_DEPTH (16) + perimeter (25)
        height = 8.5f;   // BUILDING_HEIGHT (6) + ROOF_HEIGHT (2.5)
    }
    
    current_ghost_size = Math::max(width, depth);
    
    mesh->set_size(Vector3(width, height, depth));
    ghost_mesh->set_mesh(mesh);
    ghost_mesh->set_position(Vector3(0, height / 2.0f, 0));
    
    // Semi-transparent green material (valid placement)
    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_albedo(Color(0.2f, 0.8f, 0.2f, 0.4f));
    mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
    ghost_mesh->set_surface_override_material(0, mat);
    
    is_placement_valid = true;
}

void Bulldozer::remove_ghost_building() {
    if (ghost_building) {
        ghost_building->queue_free();
        ghost_building = nullptr;
        ghost_mesh = nullptr;
    }
}

Building* Bulldozer::spawn_building(BuildingType type, const Vector3 &position) {
    Building *building = nullptr;
    
    if (type == BuildingType::POWER_PLANT) {
        building = memnew(Building);
        building->set_building_name("Power Plant");
        building->set_building_size(8.0f);
        building->set_building_height(6.0f);
        building->set_max_health(1000);
        building->set_health(1000);
        if (!power_plant_model.is_empty()) {
            building->set_model_path(power_plant_model);
            building->set_model_scale(0.05f);
        }
    } else if (type == BuildingType::BARRACKS) {
        // Use Barracks class which creates its own detailed 3D model
        Barracks *barracks = memnew(Barracks);
        building = barracks;
        // Barracks constructor already sets name, size, health, etc.
    } else {
        // Default fallback
        building = memnew(Building);
    }
    
    // Add to scene
    get_tree()->get_root()->add_child(building);
    
    // Try to get terrain height directly from cached TerrainGenerator
    float terrain_y = position.y;
    if (cached_terrain_generator) {
        UtilityFunctions::print("Building: Found TerrainGenerator node");
        // Call get_height_at on the terrain generator
        Variant height_result = cached_terrain_generator->call("get_height_at", position.x, position.z);
        UtilityFunctions::print("Building: get_height_at returned type ", height_result.get_type(), " value ", height_result);
        if (height_result.get_type() == Variant::FLOAT || height_result.get_type() == Variant::INT) {
            terrain_y = (float)height_result;
            UtilityFunctions::print("Building: Got terrain height ", terrain_y, " at (", position.x, ", ", position.z, ")");
        }
    } else {
        UtilityFunctions::print("Building: TerrainGenerator node NOT FOUND!");
    }
    
    // Set position on terrain
    Vector3 spawn_pos = position;
    spawn_pos.y = terrain_y;
    building->set_global_position(spawn_pos);
    
    UtilityFunctions::print("Building spawned at Y=", spawn_pos.y);
    
    // Notify the FlowFieldManager that a building was placed
    building->notify_flow_field_of_placement();
    
    return building;
}

void Bulldozer::update_ghost_validity() {
    if (!ghost_building || !ghost_mesh) return;
    
    Vector3 pos = ghost_building->get_global_position();
    bool valid = check_placement_valid(pos, current_ghost_size);
    
    if (valid != is_placement_valid) {
        is_placement_valid = valid;
        
        Ref<StandardMaterial3D> mat = ghost_mesh->get_surface_override_material(0);
        if (mat.is_null()) {
            mat.instantiate();
            ghost_mesh->set_surface_override_material(0, mat);
        }
        
        mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
        
        if (valid) {
            // Green for valid placement
            mat->set_albedo(Color(0.2f, 0.8f, 0.2f, 0.5f));
            mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
            mat->set_emission(Color(0.1f, 0.4f, 0.1f));
            mat->set_emission_energy_multiplier(0.3f);
        } else {
            // Red for invalid placement
            mat->set_albedo(Color(0.8f, 0.2f, 0.2f, 0.5f));
            mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
            mat->set_emission(Color(0.4f, 0.1f, 0.1f));
            mat->set_emission_energy_multiplier(0.6f);
        }
    }
}

bool Bulldozer::check_placement_valid(const Vector3 &position, float size) {
    // Use the static method from Building class for consistency
    uint32_t check_mask = 0b1110; // Units(2), Buildings(4), Vehicles(8)
    return Building::is_position_valid_for_building(this, position, size, check_mask);
}

} // namespace rts
