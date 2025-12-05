/**
 * Barracks.cpp
 * Military barracks building implementation with detailed 3D model.
 * Features realistic military architecture for training infantry.
 */

#include "Barracks.h"
#include "Unit.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/prism_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;

namespace rts {

void Barracks::_bind_methods() {
    // Unit training methods
    ClassDB::bind_method(D_METHOD("train_unit"), &Barracks::train_unit);
    ClassDB::bind_method(D_METHOD("queue_unit"), &Barracks::queue_unit);
    ClassDB::bind_method(D_METHOD("cancel_training"), &Barracks::cancel_training);
    ClassDB::bind_method(D_METHOD("get_unit_queue"), &Barracks::get_unit_queue);
    ClassDB::bind_method(D_METHOD("get_train_progress"), &Barracks::get_train_progress);
    ClassDB::bind_method(D_METHOD("get_is_training"), &Barracks::get_is_training);
    
    ClassDB::bind_method(D_METHOD("get_spawn_point"), &Barracks::get_spawn_point);
    ClassDB::bind_method(D_METHOD("get_rally_point"), &Barracks::get_rally_point);
    ClassDB::bind_method(D_METHOD("set_rally_point", "point"), &Barracks::set_rally_point);
    
    // Door animation
    ClassDB::bind_method(D_METHOD("open_door"), &Barracks::open_door);
    ClassDB::bind_method(D_METHOD("close_door"), &Barracks::close_door);
    ClassDB::bind_method(D_METHOD("toggle_door"), &Barracks::toggle_door);
    
    // Signals
    ADD_SIGNAL(MethodInfo("unit_trained", PropertyInfo(Variant::OBJECT, "unit")));
    ADD_SIGNAL(MethodInfo("training_started"));
    ADD_SIGNAL(MethodInfo("training_complete"));
}

Barracks::Barracks() {
    // Set default building properties
    set_building_name("Barracks");
    set_building_size(BUILDING_WIDTH);
    set_building_height(BUILDING_HEIGHT + ROOF_HEIGHT);
    set_max_health(1200);
    set_health(1200);
    set_armor(4);
    
    // Skip default box mesh - we create our own detailed geometry
    skip_default_mesh = true;
}

Barracks::~Barracks() {
}

void Barracks::_ready() {
    Building::_ready();
    
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Create the detailed barracks model
    create_barracks();
    
    // Calculate spawn point (in front of entrance)
    spawn_point = get_global_position() + Vector3(0, 0, BUILDING_DEPTH / 2.0f + 3.0f);
    rally_point = spawn_point + Vector3(0, 0, 5.0f);
    
    UtilityFunctions::print("Barracks: Ready - detailed military barracks");
}

void Barracks::_process(double delta) {
    Building::_process(delta);
    
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Update door animation
    update_door_animation(delta);
    
    // Update unit training
    update_unit_training(delta);
}

void Barracks::create_barracks() {
    // Load textures first
    load_textures();
    
    // Create materials
    create_materials();
    
    // Create root node for all building parts
    building_root = memnew(Node3D);
    building_root->set_name("BarracksRoot");
    add_child(building_root);
    
    // Build all components in order
    create_foundation();
    create_main_building();
    create_roof();
    create_entrance();
    create_windows();
    create_door();
    create_training_area();
    create_details();
    create_flag();
    create_sandbags();
    create_chimney();
    create_support_structures();
    create_guard_tower();
    create_antenna();
    create_vehicle_depot();
    create_perimeter();
    
    UtilityFunctions::print("Barracks: Created all detailed components");
}

void Barracks::load_textures() {
    // We'll use procedural materials with realistic colors
    // In a full implementation, you'd load actual texture files
    UtilityFunctions::print("Barracks: Setting up materials...");
}

void Barracks::create_materials() {
    // Main wall material - olive drab military color
    wall_material.instantiate();
    wall_material->set_albedo(Color(0.35f, 0.38f, 0.28f));  // Military olive drab
    wall_material->set_roughness(0.85f);
    wall_material->set_metallic(0.0f);
    
    // Brick material - for accents
    brick_material.instantiate();
    brick_material->set_albedo(Color(0.55f, 0.35f, 0.25f));  // Warm brick red
    brick_material->set_roughness(0.9f);
    brick_material->set_metallic(0.0f);
    
    // Concrete material - foundation and details
    concrete_material.instantiate();
    concrete_material->set_albedo(Color(0.5f, 0.48f, 0.45f));  // Gray concrete
    concrete_material->set_roughness(0.95f);
    concrete_material->set_metallic(0.0f);
    
    // Metal material - pipes, railings
    metal_material.instantiate();
    metal_material->set_albedo(Color(0.6f, 0.62f, 0.64f));  // Light steel
    metal_material->set_roughness(0.4f);
    metal_material->set_metallic(0.75f);
    
    // Dark metal - structural elements
    dark_metal_material.instantiate();
    dark_metal_material->set_albedo(Color(0.2f, 0.22f, 0.25f));  // Dark steel
    dark_metal_material->set_roughness(0.45f);
    dark_metal_material->set_metallic(0.7f);
    
    // Window material - tinted glass
    window_material.instantiate();
    window_material->set_albedo(Color(0.2f, 0.25f, 0.3f, 0.8f));
    window_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    window_material->set_roughness(0.1f);
    window_material->set_metallic(0.2f);
    
    // Door material - reinforced wood/metal
    door_material.instantiate();
    door_material->set_albedo(Color(0.3f, 0.25f, 0.2f));  // Dark brown
    door_material->set_roughness(0.7f);
    door_material->set_metallic(0.15f);
    
    // Roof material - corrugated metal
    roof_material.instantiate();
    roof_material->set_albedo(Color(0.35f, 0.32f, 0.28f));  // Weathered metal
    roof_material->set_roughness(0.6f);
    roof_material->set_metallic(0.5f);
    
    // Wood material - beams and trim
    wood_material.instantiate();
    wood_material->set_albedo(Color(0.45f, 0.35f, 0.25f));  // Natural wood
    wood_material->set_roughness(0.8f);
    wood_material->set_metallic(0.0f);
    
    // Canvas material - for tarps, flags
    canvas_material.instantiate();
    canvas_material->set_albedo(Color(0.4f, 0.42f, 0.35f));  // Canvas tan
    canvas_material->set_roughness(0.9f);
    canvas_material->set_metallic(0.0f);
    
    // Ground material - dirt/gravel
    ground_material.instantiate();
    ground_material->set_albedo(Color(0.4f, 0.35f, 0.28f));  // Brown dirt
    ground_material->set_roughness(1.0f);
    ground_material->set_metallic(0.0f);
}

void Barracks::create_foundation() {
    // Concrete foundation platform
    foundation = memnew(MeshInstance3D);
    foundation->set_name("Foundation");
    building_root->add_child(foundation);
    
    Ref<BoxMesh> fnd_mesh;
    fnd_mesh.instantiate();
    fnd_mesh->set_size(Vector3(BUILDING_WIDTH + 1.0f, FOUNDATION_HEIGHT, BUILDING_DEPTH + 1.0f));
    foundation->set_mesh(fnd_mesh);
    foundation->set_position(Vector3(0, FOUNDATION_HEIGHT / 2.0f, 0));
    foundation->set_surface_override_material(0, concrete_material);
    
    // Foundation edge/step
    MeshInstance3D *step = memnew(MeshInstance3D);
    step->set_name("FoundationStep");
    building_root->add_child(step);
    
    Ref<BoxMesh> step_mesh;
    step_mesh.instantiate();
    step_mesh->set_size(Vector3(BUILDING_WIDTH + 1.5f, 0.1f, BUILDING_DEPTH + 1.5f));
    step->set_mesh(step_mesh);
    step->set_position(Vector3(0, 0.05f, 0));
    step->set_surface_override_material(0, concrete_material);
    
    detail_meshes.push_back(foundation);
    detail_meshes.push_back(step);
}

void Barracks::create_main_building() {
    // Main barracks building - long rectangular structure
    main_building = memnew(MeshInstance3D);
    main_building->set_name("MainBuilding");
    building_root->add_child(main_building);
    
    Ref<BoxMesh> main_mesh;
    main_mesh.instantiate();
    main_mesh->set_size(Vector3(BUILDING_WIDTH, BUILDING_HEIGHT, BUILDING_DEPTH));
    main_building->set_mesh(main_mesh);
    main_building->set_position(Vector3(0, FOUNDATION_HEIGHT + BUILDING_HEIGHT / 2.0f, 0));
    main_building->set_surface_override_material(0, wall_material);
    
    // Bottom trim band - brick accent
    MeshInstance3D *bottom_trim = memnew(MeshInstance3D);
    bottom_trim->set_name("BottomTrim");
    building_root->add_child(bottom_trim);
    
    Ref<BoxMesh> trim_mesh;
    trim_mesh.instantiate();
    trim_mesh->set_size(Vector3(BUILDING_WIDTH + 0.1f, 0.6f, BUILDING_DEPTH + 0.1f));
    bottom_trim->set_mesh(trim_mesh);
    bottom_trim->set_position(Vector3(0, FOUNDATION_HEIGHT + 0.3f, 0));
    bottom_trim->set_surface_override_material(0, brick_material);
    
    // Top trim band
    MeshInstance3D *top_trim = memnew(MeshInstance3D);
    top_trim->set_name("TopTrim");
    building_root->add_child(top_trim);
    
    Ref<BoxMesh> top_trim_mesh;
    top_trim_mesh.instantiate();
    top_trim_mesh->set_size(Vector3(BUILDING_WIDTH + 0.15f, 0.25f, BUILDING_DEPTH + 0.15f));
    top_trim->set_mesh(top_trim_mesh);
    top_trim->set_position(Vector3(0, FOUNDATION_HEIGHT + BUILDING_HEIGHT - 0.125f, 0));
    top_trim->set_surface_override_material(0, dark_metal_material);
    
    // Corner pillars for reinforced look
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            MeshInstance3D *pillar = memnew(MeshInstance3D);
            pillar->set_name("CornerPillar");
            building_root->add_child(pillar);
            
            Ref<BoxMesh> pillar_mesh;
            pillar_mesh.instantiate();
            pillar_mesh->set_size(Vector3(0.5f, BUILDING_HEIGHT, 0.5f));
            pillar->set_mesh(pillar_mesh);
            
            float px = x * (BUILDING_WIDTH / 2.0f - 0.25f);
            float pz = z * (BUILDING_DEPTH / 2.0f - 0.25f);
            pillar->set_position(Vector3(px, FOUNDATION_HEIGHT + BUILDING_HEIGHT / 2.0f, pz));
            pillar->set_surface_override_material(0, brick_material);
            detail_meshes.push_back(pillar);
        }
    }
    
    detail_meshes.push_back(bottom_trim);
    detail_meshes.push_back(top_trim);
}

void Barracks::create_roof() {
    // Gabled roof using a prism for realistic look
    roof_section = memnew(MeshInstance3D);
    roof_section->set_name("Roof");
    building_root->add_child(roof_section);
    
    // Create the main roof - pitched
    Ref<PrismMesh> roof_prism;
    roof_prism.instantiate();
    roof_prism->set_size(Vector3(BUILDING_WIDTH + 0.8f, ROOF_HEIGHT, BUILDING_DEPTH + 0.5f));
    roof_prism->set_left_to_right(0.5f);  // Centered peak
    roof_section->set_mesh(roof_prism);
    roof_section->set_position(Vector3(0, FOUNDATION_HEIGHT + BUILDING_HEIGHT + ROOF_HEIGHT / 2.0f, 0));
    roof_section->set_rotation_degrees(Vector3(0, 90, 0));  // Rotate for length direction
    roof_section->set_surface_override_material(0, roof_material);
    
    // Roof overhang beams
    for (int i = 0; i < 4; i++) {
        MeshInstance3D *beam = memnew(MeshInstance3D);
        beam->set_name(String("RoofBeam") + String::num_int64(i));
        building_root->add_child(beam);
        
        Ref<BoxMesh> beam_mesh;
        beam_mesh.instantiate();
        beam_mesh->set_size(Vector3(0.15f, 0.2f, BUILDING_DEPTH + 1.0f));
        beam->set_mesh(beam_mesh);
        
        float bx = -BUILDING_WIDTH / 2.0f + 0.5f + i * (BUILDING_WIDTH / 3.0f);
        beam->set_position(Vector3(bx, FOUNDATION_HEIGHT + BUILDING_HEIGHT - 0.1f, 0));
        beam->set_surface_override_material(0, wood_material);
        detail_meshes.push_back(beam);
    }
    
    // Gable end triangles (fill in the ends)
    for (int z = -1; z <= 1; z += 2) {
        MeshInstance3D *gable = memnew(MeshInstance3D);
        gable->set_name("GableEnd");
        building_root->add_child(gable);
        
        // Simplified gable as a box (would be triangle in full implementation)
        Ref<BoxMesh> gable_mesh;
        gable_mesh.instantiate();
        gable_mesh->set_size(Vector3(BUILDING_WIDTH, ROOF_HEIGHT * 0.8f, 0.2f));
        gable->set_mesh(gable_mesh);
        gable->set_position(Vector3(0, FOUNDATION_HEIGHT + BUILDING_HEIGHT + ROOF_HEIGHT * 0.3f, z * (BUILDING_DEPTH / 2.0f + 0.1f)));
        gable->set_surface_override_material(0, wall_material);
        detail_meshes.push_back(gable);
    }
}

void Barracks::create_entrance() {
    // Entrance porch/awning at the front
    entrance_section = memnew(MeshInstance3D);
    entrance_section->set_name("EntrancePorch");
    building_root->add_child(entrance_section);
    
    // Porch platform
    Ref<BoxMesh> porch_mesh;
    porch_mesh.instantiate();
    porch_mesh->set_size(Vector3(4.0f, 0.15f, 2.0f));
    entrance_section->set_mesh(porch_mesh);
    entrance_section->set_position(Vector3(0, FOUNDATION_HEIGHT + 0.075f, BUILDING_DEPTH / 2.0f + 1.0f));
    entrance_section->set_surface_override_material(0, concrete_material);
    
    // Porch support columns
    for (int x = -1; x <= 1; x += 2) {
        MeshInstance3D *column = memnew(MeshInstance3D);
        column->set_name("PorchColumn");
        building_root->add_child(column);
        
        Ref<CylinderMesh> col_mesh;
        col_mesh.instantiate();
        col_mesh->set_top_radius(0.15f);
        col_mesh->set_bottom_radius(0.18f);
        col_mesh->set_height(2.5f);
        column->set_mesh(col_mesh);
        column->set_position(Vector3(x * 1.7f, FOUNDATION_HEIGHT + 1.25f, BUILDING_DEPTH / 2.0f + 1.8f));
        column->set_surface_override_material(0, wood_material);
        detail_meshes.push_back(column);
    }
    
    // Porch roof
    porch_roof = memnew(MeshInstance3D);
    porch_roof->set_name("PorchRoof");
    building_root->add_child(porch_roof);
    
    Ref<BoxMesh> prch_roof_mesh;
    prch_roof_mesh.instantiate();
    prch_roof_mesh->set_size(Vector3(5.0f, 0.15f, 2.5f));
    porch_roof->set_mesh(prch_roof_mesh);
    porch_roof->set_position(Vector3(0, FOUNDATION_HEIGHT + 2.6f, BUILDING_DEPTH / 2.0f + 1.0f));
    porch_roof->set_rotation_degrees(Vector3(-10, 0, 0));  // Slight angle for drainage
    porch_roof->set_surface_override_material(0, roof_material);
    
    // Steps leading up
    for (int i = 0; i < 2; i++) {
        MeshInstance3D *step = memnew(MeshInstance3D);
        step->set_name(String("Step") + String::num_int64(i));
        building_root->add_child(step);
        
        Ref<BoxMesh> step_mesh;
        step_mesh.instantiate();
        step_mesh->set_size(Vector3(2.5f, 0.15f, 0.4f));
        step->set_mesh(step_mesh);
        step->set_position(Vector3(0, 0.075f + i * 0.15f, BUILDING_DEPTH / 2.0f + 1.8f + i * 0.4f));
        step->set_surface_override_material(0, concrete_material);
        detail_meshes.push_back(step);
    }
    
    detail_meshes.push_back(porch_roof);
}

void Barracks::create_windows() {
    // Windows on each long side
    int windows_per_side = 5;
    float window_spacing = BUILDING_WIDTH / (windows_per_side + 1);
    
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < windows_per_side; i++) {
            // Window frame
            MeshInstance3D *window_frame = memnew(MeshInstance3D);
            window_frame->set_name(String("WindowFrame") + String::num_int64(side * 10 + i));
            building_root->add_child(window_frame);
            
            Ref<BoxMesh> frame_mesh;
            frame_mesh.instantiate();
            frame_mesh->set_size(Vector3(1.0f, 1.2f, 0.15f));
            window_frame->set_mesh(frame_mesh);
            
            float wx = -BUILDING_WIDTH / 2.0f + window_spacing * (i + 1);
            float wy = FOUNDATION_HEIGHT + BUILDING_HEIGHT * 0.55f;
            float wz = side * (BUILDING_DEPTH / 2.0f + 0.05f);
            window_frame->set_position(Vector3(wx, wy, wz));
            window_frame->set_surface_override_material(0, dark_metal_material);
            window_meshes.push_back(window_frame);
            
            // Window glass
            MeshInstance3D *window_glass = memnew(MeshInstance3D);
            window_glass->set_name(String("WindowGlass") + String::num_int64(side * 10 + i));
            building_root->add_child(window_glass);
            
            Ref<BoxMesh> glass_mesh;
            glass_mesh.instantiate();
            glass_mesh->set_size(Vector3(0.85f, 1.0f, 0.05f));
            window_glass->set_mesh(glass_mesh);
            window_glass->set_position(Vector3(wx, wy, wz + side * 0.03f));
            window_glass->set_surface_override_material(0, window_material);
            window_meshes.push_back(window_glass);
            
            // Window sill
            MeshInstance3D *sill = memnew(MeshInstance3D);
            sill->set_name("WindowSill");
            building_root->add_child(sill);
            
            Ref<BoxMesh> sill_mesh;
            sill_mesh.instantiate();
            sill_mesh->set_size(Vector3(1.1f, 0.08f, 0.2f));
            sill->set_mesh(sill_mesh);
            sill->set_position(Vector3(wx, wy - 0.65f, wz + side * 0.1f));
            sill->set_surface_override_material(0, concrete_material);
            window_meshes.push_back(sill);
        }
    }
    
    // Windows on gable ends
    for (int z = -1; z <= 1; z += 2) {
        MeshInstance3D *gable_window = memnew(MeshInstance3D);
        gable_window->set_name("GableWindow");
        building_root->add_child(gable_window);
        
        Ref<BoxMesh> gw_mesh;
        gw_mesh.instantiate();
        gw_mesh->set_size(Vector3(0.8f, 0.8f, 0.1f));
        gable_window->set_mesh(gw_mesh);
        gable_window->set_position(Vector3(0, FOUNDATION_HEIGHT + BUILDING_HEIGHT + ROOF_HEIGHT * 0.3f, z * (BUILDING_DEPTH / 2.0f + 0.15f)));
        gable_window->set_surface_override_material(0, window_material);
        window_meshes.push_back(gable_window);
    }
}

void Barracks::create_door() {
    // Main entrance door
    door_pivot = memnew(Node3D);
    door_pivot->set_name("DoorPivot");
    building_root->add_child(door_pivot);
    door_pivot->set_position(Vector3(-0.9f, FOUNDATION_HEIGHT, BUILDING_DEPTH / 2.0f));
    
    door_mesh = memnew(MeshInstance3D);
    door_mesh->set_name("Door");
    door_pivot->add_child(door_mesh);
    
    Ref<BoxMesh> d_mesh;
    d_mesh.instantiate();
    d_mesh->set_size(Vector3(1.8f, 2.4f, 0.1f));
    door_mesh->set_mesh(d_mesh);
    door_mesh->set_position(Vector3(0.9f, 1.2f, 0.05f));  // Offset from pivot
    door_mesh->set_surface_override_material(0, door_material);
    
    // Door frame
    MeshInstance3D *door_frame = memnew(MeshInstance3D);
    door_frame->set_name("DoorFrame");
    building_root->add_child(door_frame);
    
    Ref<BoxMesh> df_mesh;
    df_mesh.instantiate();
    df_mesh->set_size(Vector3(2.1f, 2.6f, 0.15f));
    door_frame->set_mesh(df_mesh);
    door_frame->set_position(Vector3(0, FOUNDATION_HEIGHT + 1.3f, BUILDING_DEPTH / 2.0f + 0.05f));
    door_frame->set_surface_override_material(0, dark_metal_material);
    detail_meshes.push_back(door_frame);
    
    // Door sign/placard
    MeshInstance3D *sign = memnew(MeshInstance3D);
    sign->set_name("DoorSign");
    building_root->add_child(sign);
    
    Ref<BoxMesh> sign_mesh;
    sign_mesh.instantiate();
    sign_mesh->set_size(Vector3(1.5f, 0.4f, 0.05f));
    sign->set_mesh(sign_mesh);
    sign->set_position(Vector3(0, FOUNDATION_HEIGHT + 2.8f, BUILDING_DEPTH / 2.0f + 0.15f));
    
    // Sign material with text color
    Ref<StandardMaterial3D> sign_mat;
    sign_mat.instantiate();
    sign_mat->set_albedo(Color(0.15f, 0.2f, 0.15f));  // Dark green
    sign->set_surface_override_material(0, sign_mat);
    detail_meshes.push_back(sign);
}

void Barracks::create_training_area() {
    // Training ground pad in front
    training_ground = memnew(MeshInstance3D);
    training_ground->set_name("TrainingGround");
    building_root->add_child(training_ground);
    
    Ref<BoxMesh> tg_mesh;
    tg_mesh.instantiate();
    tg_mesh->set_size(Vector3(10.0f, 0.05f, 6.0f));
    training_ground->set_mesh(tg_mesh);
    training_ground->set_position(Vector3(0, 0.025f, BUILDING_DEPTH / 2.0f + 6.0f));
    training_ground->set_surface_override_material(0, ground_material);
    
    // Training area border
    MeshInstance3D *border = memnew(MeshInstance3D);
    border->set_name("TrainingBorder");
    building_root->add_child(border);
    
    Ref<BoxMesh> border_mesh;
    border_mesh.instantiate();
    border_mesh->set_size(Vector3(10.5f, 0.1f, 6.5f));
    border->set_mesh(border_mesh);
    border->set_position(Vector3(0, 0.05f, BUILDING_DEPTH / 2.0f + 6.0f));
    border->set_surface_override_material(0, concrete_material);
    detail_meshes.push_back(border);
}

void Barracks::create_flag() {
    // Flag pole
    flag_pole = memnew(MeshInstance3D);
    flag_pole->set_name("FlagPole");
    building_root->add_child(flag_pole);
    
    Ref<CylinderMesh> pole_mesh;
    pole_mesh.instantiate();
    pole_mesh->set_top_radius(0.05f);
    pole_mesh->set_bottom_radius(0.08f);
    pole_mesh->set_height(6.0f);
    flag_pole->set_mesh(pole_mesh);
    flag_pole->set_position(Vector3(BUILDING_WIDTH / 2.0f + 1.5f, 3.0f, BUILDING_DEPTH / 2.0f + 2.0f));
    flag_pole->set_surface_override_material(0, metal_material);
    
    // Flag
    MeshInstance3D *flag = memnew(MeshInstance3D);
    flag->set_name("Flag");
    building_root->add_child(flag);
    
    Ref<BoxMesh> flag_mesh;
    flag_mesh.instantiate();
    flag_mesh->set_size(Vector3(1.5f, 1.0f, 0.02f));
    flag->set_mesh(flag_mesh);
    flag->set_position(Vector3(BUILDING_WIDTH / 2.0f + 2.3f, 5.5f, BUILDING_DEPTH / 2.0f + 2.0f));
    
    // Flag material - military green/tan
    Ref<StandardMaterial3D> flag_mat;
    flag_mat.instantiate();
    flag_mat->set_albedo(Color(0.3f, 0.35f, 0.25f));
    flag_mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);  // Double-sided
    flag->set_surface_override_material(0, flag_mat);
    
    // Flag pole base
    MeshInstance3D *base = memnew(MeshInstance3D);
    base->set_name("FlagPoleBase");
    building_root->add_child(base);
    
    Ref<BoxMesh> base_mesh;
    base_mesh.instantiate();
    base_mesh->set_size(Vector3(0.6f, 0.3f, 0.6f));
    base->set_mesh(base_mesh);
    base->set_position(Vector3(BUILDING_WIDTH / 2.0f + 1.5f, 0.15f, BUILDING_DEPTH / 2.0f + 2.0f));
    base->set_surface_override_material(0, concrete_material);
    
    detail_meshes.push_back(flag);
    detail_meshes.push_back(base);
}

void Barracks::create_sandbags() {
    // Sandbag pile at corner for defensive look
    auto create_sandbag_pile = [this](Vector3 position, int layers, int bags_per_layer) {
        Node3D *pile = memnew(Node3D);
        pile->set_name("SandbagPile");
        building_root->add_child(pile);
        pile->set_position(position);
        
        float bag_width = 0.5f;
        float bag_height = 0.2f;
        float bag_depth = 0.3f;
        
        for (int layer = 0; layer < layers; layer++) {
            int bags = bags_per_layer - layer;
            float offset = layer * 0.15f;
            
            for (int i = 0; i < bags; i++) {
                MeshInstance3D *bag = memnew(MeshInstance3D);
                bag->set_name("Sandbag");
                pile->add_child(bag);
                
                Ref<BoxMesh> bag_mesh;
                bag_mesh.instantiate();
                bag_mesh->set_size(Vector3(bag_width, bag_height, bag_depth));
                bag->set_mesh(bag_mesh);
                
                float bx = -bags * bag_width / 2.0f + i * bag_width + bag_width / 2.0f + offset * 0.5f;
                float by = layer * bag_height + bag_height / 2.0f;
                bag->set_position(Vector3(bx, by, 0));
                bag->set_surface_override_material(0, canvas_material);
                detail_meshes.push_back(bag);
            }
        }
    };
    
    create_sandbag_pile(Vector3(-BUILDING_WIDTH / 2.0f - 1.0f, 0, BUILDING_DEPTH / 2.0f - 1.0f), 3, 4);
    create_sandbag_pile(Vector3(BUILDING_WIDTH / 2.0f + 1.0f, 0, BUILDING_DEPTH / 2.0f - 1.0f), 2, 3);
}

void Barracks::create_chimney() {
    // Chimney/exhaust pipe
    chimney = memnew(MeshInstance3D);
    chimney->set_name("Chimney");
    building_root->add_child(chimney);
    
    Ref<CylinderMesh> chim_mesh;
    chim_mesh.instantiate();
    chim_mesh->set_top_radius(0.2f);
    chim_mesh->set_bottom_radius(0.25f);
    chim_mesh->set_height(1.5f);
    chimney->set_mesh(chim_mesh);
    chimney->set_position(Vector3(BUILDING_WIDTH / 2.0f - 1.5f, FOUNDATION_HEIGHT + BUILDING_HEIGHT + ROOF_HEIGHT + 0.5f, 0));
    chimney->set_surface_override_material(0, dark_metal_material);
    
    // Chimney cap
    MeshInstance3D *cap = memnew(MeshInstance3D);
    cap->set_name("ChimneyCap");
    building_root->add_child(cap);
    
    Ref<CylinderMesh> cap_mesh;
    cap_mesh.instantiate();
    cap_mesh->set_top_radius(0.35f);
    cap_mesh->set_bottom_radius(0.35f);
    cap_mesh->set_height(0.1f);
    cap->set_mesh(cap_mesh);
    cap->set_position(Vector3(BUILDING_WIDTH / 2.0f - 1.5f, FOUNDATION_HEIGHT + BUILDING_HEIGHT + ROOF_HEIGHT + 1.3f, 0));
    cap->set_surface_override_material(0, metal_material);
    
    detail_meshes.push_back(cap);
}

void Barracks::create_details() {
    // AC unit on roof
    ac_unit = memnew(MeshInstance3D);
    ac_unit->set_name("ACUnit");
    building_root->add_child(ac_unit);
    
    Ref<BoxMesh> ac_mesh;
    ac_mesh.instantiate();
    ac_mesh->set_size(Vector3(1.2f, 0.8f, 0.8f));
    ac_unit->set_mesh(ac_mesh);
    ac_unit->set_position(Vector3(-BUILDING_WIDTH / 2.0f + 2.0f, FOUNDATION_HEIGHT + BUILDING_HEIGHT + 0.4f, 0));
    ac_unit->set_surface_override_material(0, metal_material);
    
    // AC unit fan grill
    MeshInstance3D *grill = memnew(MeshInstance3D);
    grill->set_name("ACGrill");
    building_root->add_child(grill);
    
    Ref<CylinderMesh> grill_mesh;
    grill_mesh.instantiate();
    grill_mesh->set_top_radius(0.3f);
    grill_mesh->set_bottom_radius(0.3f);
    grill_mesh->set_height(0.05f);
    grill->set_mesh(grill_mesh);
    grill->set_position(Vector3(-BUILDING_WIDTH / 2.0f + 2.0f, FOUNDATION_HEIGHT + BUILDING_HEIGHT + 0.82f, 0));
    grill->set_surface_override_material(0, dark_metal_material);
    
    // Spotlight
    spotlight = memnew(MeshInstance3D);
    spotlight->set_name("Spotlight");
    building_root->add_child(spotlight);
    
    Ref<CylinderMesh> spot_mesh;
    spot_mesh.instantiate();
    spot_mesh->set_top_radius(0.15f);
    spot_mesh->set_bottom_radius(0.2f);
    spot_mesh->set_height(0.4f);
    spotlight->set_mesh(spot_mesh);
    spotlight->set_position(Vector3(0, FOUNDATION_HEIGHT + 2.8f, BUILDING_DEPTH / 2.0f + 0.4f));
    spotlight->set_rotation_degrees(Vector3(45, 0, 0));
    
    // Spotlight material with emission
    Ref<StandardMaterial3D> spot_mat;
    spot_mat.instantiate();
    spot_mat->set_albedo(Color(0.9f, 0.9f, 0.8f));
    spot_mat->set_feature(StandardMaterial3D::FEATURE_EMISSION, true);
    spot_mat->set_emission(Color(1.0f, 0.95f, 0.8f));
    spot_mat->set_emission_energy_multiplier(1.5f);
    spotlight->set_surface_override_material(0, spot_mat);
    
    // Utility box on side
    MeshInstance3D *util_box = memnew(MeshInstance3D);
    util_box->set_name("UtilityBox");
    building_root->add_child(util_box);
    
    Ref<BoxMesh> util_mesh;
    util_mesh.instantiate();
    util_mesh->set_size(Vector3(0.8f, 1.2f, 0.4f));
    util_box->set_mesh(util_mesh);
    util_box->set_position(Vector3(-BUILDING_WIDTH / 2.0f - 0.3f, FOUNDATION_HEIGHT + 0.6f, -BUILDING_DEPTH / 4.0f));
    util_box->set_surface_override_material(0, metal_material);
    
    // Pipes along wall
    for (int i = 0; i < 2; i++) {
        MeshInstance3D *pipe = memnew(MeshInstance3D);
        pipe->set_name(String("Pipe") + String::num_int64(i));
        building_root->add_child(pipe);
        
        Ref<CylinderMesh> pipe_mesh;
        pipe_mesh.instantiate();
        pipe_mesh->set_top_radius(0.06f);
        pipe_mesh->set_bottom_radius(0.06f);
        pipe_mesh->set_height(BUILDING_HEIGHT);
        pipe->set_mesh(pipe_mesh);
        pipe->set_position(Vector3(-BUILDING_WIDTH / 2.0f - 0.1f, FOUNDATION_HEIGHT + BUILDING_HEIGHT / 2.0f, BUILDING_DEPTH / 4.0f - i * 1.5f));
        pipe->set_surface_override_material(0, dark_metal_material);
        detail_meshes.push_back(pipe);
    }
    
    detail_meshes.push_back(grill);
    detail_meshes.push_back(util_box);
}

void Barracks::create_support_structures() {
    // Storage crates near building
    for (int i = 0; i < 3; i++) {
        MeshInstance3D *crate = memnew(MeshInstance3D);
        crate->set_name(String("Crate") + String::num_int64(i));
        building_root->add_child(crate);
        
        float size = 0.6f + (i % 2) * 0.2f;
        Ref<BoxMesh> crate_mesh;
        crate_mesh.instantiate();
        crate_mesh->set_size(Vector3(size, size * 0.8f, size));
        crate->set_mesh(crate_mesh);
        crate->set_position(Vector3(-BUILDING_WIDTH / 2.0f - 1.5f - i * 0.8f, size * 0.4f, -BUILDING_DEPTH / 4.0f + i * 0.3f));
        crate->set_rotation_degrees(Vector3(0, i * 15.0f, 0));
        crate->set_surface_override_material(0, wood_material);
        detail_meshes.push_back(crate);
    }
    
    // Barrel
    MeshInstance3D *barrel = memnew(MeshInstance3D);
    barrel->set_name("Barrel");
    building_root->add_child(barrel);
    
    Ref<CylinderMesh> barrel_mesh;
    barrel_mesh.instantiate();
    barrel_mesh->set_top_radius(0.3f);
    barrel_mesh->set_bottom_radius(0.35f);
    barrel_mesh->set_height(0.9f);
    barrel->set_mesh(barrel_mesh);
    barrel->set_position(Vector3(-BUILDING_WIDTH / 2.0f - 1.2f, 0.45f, BUILDING_DEPTH / 4.0f));
    barrel->set_surface_override_material(0, dark_metal_material);
    detail_meshes.push_back(barrel);
    
    // Bench near training area
    MeshInstance3D *bench_seat = memnew(MeshInstance3D);
    bench_seat->set_name("BenchSeat");
    building_root->add_child(bench_seat);
    
    Ref<BoxMesh> bench_mesh;
    bench_mesh.instantiate();
    bench_mesh->set_size(Vector3(2.0f, 0.1f, 0.4f));
    bench_seat->set_mesh(bench_mesh);
    bench_seat->set_position(Vector3(BUILDING_WIDTH / 2.0f + 1.5f, 0.45f, BUILDING_DEPTH / 2.0f + 4.0f));
    bench_seat->set_surface_override_material(0, wood_material);
    
    // Bench legs
    for (int i = 0; i < 2; i++) {
        MeshInstance3D *leg = memnew(MeshInstance3D);
        leg->set_name(String("BenchLeg") + String::num_int64(i));
        building_root->add_child(leg);
        
        Ref<BoxMesh> leg_mesh;
        leg_mesh.instantiate();
        leg_mesh->set_size(Vector3(0.1f, 0.45f, 0.35f));
        leg->set_mesh(leg_mesh);
        leg->set_position(Vector3(BUILDING_WIDTH / 2.0f + 1.5f + (i * 2 - 1) * 0.85f, 0.225f, BUILDING_DEPTH / 2.0f + 4.0f));
        leg->set_surface_override_material(0, metal_material);
        detail_meshes.push_back(leg);
    }
    
    detail_meshes.push_back(bench_seat);
}

// Door animation methods
void Barracks::open_door() {
    if (!door_is_open && !door_animating) {
        door_animating = true;
        door_target_open = true;
        UtilityFunctions::print("Barracks: Opening door");
    }
}

void Barracks::close_door() {
    if (door_is_open && !door_animating) {
        door_animating = true;
        door_target_open = false;
        UtilityFunctions::print("Barracks: Closing door");
    }
}

void Barracks::toggle_door() {
    if (door_is_open) {
        close_door();
    } else {
        open_door();
    }
}

void Barracks::update_door_animation(double delta) {
    if (!door_animating || !door_pivot) return;
    
    float target_rotation = door_target_open ? -110.0f : 0.0f;
    float current_rotation = door_pivot->get_rotation_degrees().y;
    
    float new_rotation = Math::move_toward(current_rotation, target_rotation, static_cast<float>(door_animation_speed * 60.0f * delta));
    door_pivot->set_rotation_degrees(Vector3(0, new_rotation, 0));
    
    if (Math::abs(new_rotation - target_rotation) < 0.1f) {
        door_animating = false;
        door_is_open = door_target_open;
        door_open_amount = door_is_open ? 1.0f : 0.0f;
    } else {
        door_open_amount = Math::abs(new_rotation) / 110.0f;
    }
}

bool Barracks::is_door_open() const {
    return door_is_open;
}

// Unit training methods
void Barracks::train_unit() {
    if (!is_training_unit) {
        is_training_unit = true;
        train_progress = 0.0f;
        open_door();
        emit_signal("training_started");
        UtilityFunctions::print("Barracks: Started training unit");
    }
}

void Barracks::queue_unit() {
    unit_queue++;
    if (!is_training_unit) {
        train_unit();
    }
}

void Barracks::cancel_training() {
    if (is_training_unit) {
        is_training_unit = false;
        train_progress = 0.0f;
        unit_queue = 0;
        close_door();
        UtilityFunctions::print("Barracks: Training cancelled");
    }
}

void Barracks::update_unit_training(double delta) {
    if (!is_training_unit) return;
    
    train_progress += delta / unit_train_time;
    
    if (train_progress >= 1.0f) {
        // Training complete - spawn unit
        is_training_unit = false;
        train_progress = 0.0f;
        unit_queue = Math::max(0, unit_queue - 1);
        
        emit_signal("training_complete");
        UtilityFunctions::print("Barracks: Unit training complete!");
        
        // If more units in queue, start next
        if (unit_queue > 0) {
            train_unit();
        } else {
            close_door();
        }
    }
}

Vector3 Barracks::get_spawn_point() const {
    return spawn_point;
}

Vector3 Barracks::get_rally_point() const {
    return rally_point;
}

void Barracks::set_rally_point(const Vector3 &point) {
    rally_point = point;
}

int Barracks::get_unit_queue() const {
    return unit_queue;
}

float Barracks::get_train_progress() const {
    return train_progress;
}

bool Barracks::get_is_training() const {
    return is_training_unit;
}

// Mesh generation helpers (placeholder implementations)
Ref<ArrayMesh> Barracks::create_box_mesh(float width, float height, float depth) {
    Ref<BoxMesh> box;
    box.instantiate();
    box->set_size(Vector3(width, height, depth));
    return box;
}

Ref<ArrayMesh> Barracks::create_cylinder_mesh(float radius, float height, int segments) {
    Ref<CylinderMesh> cyl;
    cyl.instantiate();
    cyl->set_top_radius(radius);
    cyl->set_bottom_radius(radius);
    cyl->set_height(height);
    cyl->set_radial_segments(segments);
    return cyl;
}

Ref<ArrayMesh> Barracks::create_roof_mesh(float width, float depth, float height, float overhang) {
    // For now, return a simple prism
    Ref<PrismMesh> prism;
    prism.instantiate();
    prism->set_size(Vector3(width + overhang * 2, height, depth + overhang * 2));
    return prism;
}

Ref<ArrayMesh> Barracks::create_sandbag_mesh() {
    Ref<BoxMesh> box;
    box.instantiate();
    box->set_size(Vector3(0.5f, 0.2f, 0.3f));
    return box;
}

Ref<ArrayMesh> Barracks::create_flag_mesh(float width, float height) {
    Ref<BoxMesh> box;
    box.instantiate();
    box->set_size(Vector3(width, height, 0.02f));
    return box;
}

void Barracks::create_guard_tower() {
    // Guard tower at corner of the building
    Node3D *tower = memnew(Node3D);
    tower->set_name("GuardTower");
    building_root->add_child(tower);
    tower->set_position(Vector3(-BUILDING_WIDTH / 2.0f - 3.0f, 0, -BUILDING_DEPTH / 2.0f - 3.0f));
    
    // Tower base platform
    MeshInstance3D *base = memnew(MeshInstance3D);
    base->set_name("TowerBase");
    tower->add_child(base);
    Ref<BoxMesh> base_mesh;
    base_mesh.instantiate();
    base_mesh->set_size(Vector3(4.0f, 0.4f, 4.0f));
    base->set_mesh(base_mesh);
    base->set_position(Vector3(0, 0.2f, 0));
    base->set_surface_override_material(0, concrete_material);
    detail_meshes.push_back(base);
    
    // Tower legs (4 corners)
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            MeshInstance3D *leg = memnew(MeshInstance3D);
            leg->set_name("TowerLeg");
            tower->add_child(leg);
            Ref<BoxMesh> leg_mesh;
            leg_mesh.instantiate();
            leg_mesh->set_size(Vector3(0.4f, 8.0f, 0.4f));
            leg->set_mesh(leg_mesh);
            leg->set_position(Vector3(x * 1.5f, 4.4f, z * 1.5f));
            leg->set_surface_override_material(0, wood_material);
            detail_meshes.push_back(leg);
        }
    }
    
    // Cross braces
    for (int level = 0; level < 3; level++) {
        for (int side = 0; side < 4; side++) {
            MeshInstance3D *brace = memnew(MeshInstance3D);
            brace->set_name("TowerBrace");
            tower->add_child(brace);
            Ref<BoxMesh> brace_mesh;
            brace_mesh.instantiate();
            brace_mesh->set_size(Vector3(3.0f, 0.15f, 0.1f));
            brace->set_mesh(brace_mesh);
            
            float by = 1.5f + level * 2.5f;
            float rotation = side * 90.0f;
            brace->set_position(Vector3(0, by, 0));
            brace->set_rotation_degrees(Vector3(0, rotation, 0));
            brace->set_surface_override_material(0, wood_material);
            detail_meshes.push_back(brace);
        }
    }
    
    // Tower platform/floor
    MeshInstance3D *platform = memnew(MeshInstance3D);
    platform->set_name("TowerPlatform");
    tower->add_child(platform);
    Ref<BoxMesh> plat_mesh;
    plat_mesh.instantiate();
    plat_mesh->set_size(Vector3(4.5f, 0.2f, 4.5f));
    platform->set_mesh(plat_mesh);
    platform->set_position(Vector3(0, 8.5f, 0));
    platform->set_surface_override_material(0, wood_material);
    detail_meshes.push_back(platform);
    
    // Tower walls/railing
    for (int side = 0; side < 4; side++) {
        MeshInstance3D *wall = memnew(MeshInstance3D);
        wall->set_name("TowerWall");
        tower->add_child(wall);
        Ref<BoxMesh> wall_mesh;
        wall_mesh.instantiate();
        wall_mesh->set_size(Vector3(4.5f, 1.2f, 0.15f));
        wall->set_mesh(wall_mesh);
        
        float wx = 0, wz = 0;
        float rotation = side * 90.0f;
        if (side == 0) wz = 2.1f;
        else if (side == 1) { wx = 2.1f; wz = 0; }
        else if (side == 2) wz = -2.1f;
        else { wx = -2.1f; wz = 0; }
        
        wall->set_position(Vector3(wx, 9.2f, wz));
        wall->set_rotation_degrees(Vector3(0, rotation, 0));
        wall->set_surface_override_material(0, wood_material);
        detail_meshes.push_back(wall);
    }
    
    // Tower roof
    MeshInstance3D *roof = memnew(MeshInstance3D);
    roof->set_name("TowerRoof");
    tower->add_child(roof);
    Ref<PrismMesh> roof_mesh;
    roof_mesh.instantiate();
    roof_mesh->set_size(Vector3(5.0f, 1.5f, 5.0f));
    roof_mesh->set_left_to_right(0.5f);
    roof->set_mesh(roof_mesh);
    roof->set_position(Vector3(0, 10.55f, 0));
    roof->set_surface_override_material(0, roof_material);
    detail_meshes.push_back(roof);
    
    // Searchlight on tower
    MeshInstance3D *searchlight = memnew(MeshInstance3D);
    searchlight->set_name("Searchlight");
    tower->add_child(searchlight);
    Ref<CylinderMesh> light_mesh;
    light_mesh.instantiate();
    light_mesh->set_top_radius(0.2f);
    light_mesh->set_bottom_radius(0.35f);
    light_mesh->set_height(0.6f);
    searchlight->set_mesh(light_mesh);
    searchlight->set_position(Vector3(1.8f, 9.5f, 1.8f));
    searchlight->set_rotation_degrees(Vector3(45, -45, 0));
    
    Ref<StandardMaterial3D> light_mat;
    light_mat.instantiate();
    light_mat->set_albedo(Color(0.9f, 0.9f, 0.85f));
    light_mat->set_feature(StandardMaterial3D::FEATURE_EMISSION, true);
    light_mat->set_emission(Color(1.0f, 0.98f, 0.9f));
    light_mat->set_emission_energy_multiplier(2.0f);
    searchlight->set_surface_override_material(0, light_mat);
    detail_meshes.push_back(searchlight);
}

void Barracks::create_antenna() {
    // Radio antenna on roof
    Node3D *antenna = memnew(Node3D);
    antenna->set_name("Antenna");
    building_root->add_child(antenna);
    antenna->set_position(Vector3(BUILDING_WIDTH / 2.0f - 3.0f, FOUNDATION_HEIGHT + BUILDING_HEIGHT + ROOF_HEIGHT, -BUILDING_DEPTH / 4.0f));
    
    // Main antenna mast
    MeshInstance3D *mast = memnew(MeshInstance3D);
    mast->set_name("AntennaMast");
    antenna->add_child(mast);
    Ref<CylinderMesh> mast_mesh;
    mast_mesh.instantiate();
    mast_mesh->set_top_radius(0.03f);
    mast_mesh->set_bottom_radius(0.08f);
    mast_mesh->set_height(5.0f);
    mast->set_mesh(mast_mesh);
    mast->set_position(Vector3(0, 2.5f, 0));
    mast->set_surface_override_material(0, metal_material);
    detail_meshes.push_back(mast);
    
    // Antenna crossbars
    for (int i = 0; i < 4; i++) {
        MeshInstance3D *crossbar = memnew(MeshInstance3D);
        crossbar->set_name("AntennaCrossbar");
        antenna->add_child(crossbar);
        Ref<CylinderMesh> cb_mesh;
        cb_mesh.instantiate();
        cb_mesh->set_top_radius(0.015f);
        cb_mesh->set_bottom_radius(0.015f);
        cb_mesh->set_height(1.2f - i * 0.2f);
        crossbar->set_mesh(cb_mesh);
        crossbar->set_position(Vector3(0, 1.5f + i * 1.0f, 0));
        crossbar->set_rotation_degrees(Vector3(90, i * 45.0f, 0));
        crossbar->set_surface_override_material(0, metal_material);
        detail_meshes.push_back(crossbar);
    }
    
    // Satellite dish
    MeshInstance3D *dish = memnew(MeshInstance3D);
    dish->set_name("SatelliteDish");
    antenna->add_child(dish);
    Ref<SphereMesh> dish_mesh;
    dish_mesh.instantiate();
    dish_mesh->set_radius(0.6f);
    dish_mesh->set_height(0.3f);
    dish->set_mesh(dish_mesh);
    dish->set_position(Vector3(1.5f, 0.5f, 0));
    dish->set_rotation_degrees(Vector3(-30, 45, 0));
    dish->set_surface_override_material(0, metal_material);
    detail_meshes.push_back(dish);
    
    // Dish arm
    MeshInstance3D *arm = memnew(MeshInstance3D);
    arm->set_name("DishArm");
    antenna->add_child(arm);
    Ref<CylinderMesh> arm_mesh;
    arm_mesh.instantiate();
    arm_mesh->set_top_radius(0.05f);
    arm_mesh->set_bottom_radius(0.05f);
    arm_mesh->set_height(1.0f);
    arm->set_mesh(arm_mesh);
    arm->set_position(Vector3(0.8f, 0.3f, 0));
    arm->set_rotation_degrees(Vector3(0, 0, 60));
    arm->set_surface_override_material(0, dark_metal_material);
    detail_meshes.push_back(arm);
}

void Barracks::create_vehicle_depot() {
    // Vehicle parking area on side of building
    Node3D *depot = memnew(Node3D);
    depot->set_name("VehicleDepot");
    building_root->add_child(depot);
    depot->set_position(Vector3(BUILDING_WIDTH / 2.0f + 5.0f, 0, 0));
    
    // Parking pad
    MeshInstance3D *pad = memnew(MeshInstance3D);
    pad->set_name("ParkingPad");
    depot->add_child(pad);
    Ref<BoxMesh> pad_mesh;
    pad_mesh.instantiate();
    pad_mesh->set_size(Vector3(8.0f, 0.1f, 10.0f));
    pad->set_mesh(pad_mesh);
    pad->set_position(Vector3(0, 0.05f, 0));
    pad->set_surface_override_material(0, concrete_material);
    detail_meshes.push_back(pad);
    
    // Carport/canopy structure
    // Support posts
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            MeshInstance3D *post = memnew(MeshInstance3D);
            post->set_name("CarportPost");
            depot->add_child(post);
            Ref<CylinderMesh> post_mesh;
            post_mesh.instantiate();
            post_mesh->set_top_radius(0.1f);
            post_mesh->set_bottom_radius(0.12f);
            post_mesh->set_height(3.5f);
            post->set_mesh(post_mesh);
            post->set_position(Vector3(x * 3.5f, 1.75f, z * 4.0f));
            post->set_surface_override_material(0, metal_material);
            detail_meshes.push_back(post);
        }
    }
    
    // Canopy roof
    MeshInstance3D *canopy = memnew(MeshInstance3D);
    canopy->set_name("Canopy");
    depot->add_child(canopy);
    Ref<BoxMesh> canopy_mesh;
    canopy_mesh.instantiate();
    canopy_mesh->set_size(Vector3(8.0f, 0.15f, 9.0f));
    canopy->set_mesh(canopy_mesh);
    canopy->set_position(Vector3(0, 3.6f, 0));
    canopy->set_rotation_degrees(Vector3(3, 0, 0));  // Slight tilt for drainage
    canopy->set_surface_override_material(0, roof_material);
    detail_meshes.push_back(canopy);
    
    // Parked military jeep (simplified)
    Node3D *jeep = memnew(Node3D);
    jeep->set_name("ParkedJeep");
    depot->add_child(jeep);
    jeep->set_position(Vector3(0, 0.1f, -2.0f));
    
    // Jeep body
    MeshInstance3D *jeep_body = memnew(MeshInstance3D);
    jeep_body->set_name("JeepBody");
    jeep->add_child(jeep_body);
    Ref<BoxMesh> jb_mesh;
    jb_mesh.instantiate();
    jb_mesh->set_size(Vector3(2.2f, 1.0f, 4.0f));
    jeep_body->set_mesh(jb_mesh);
    jeep_body->set_position(Vector3(0, 0.8f, 0));
    
    Ref<StandardMaterial3D> jeep_mat;
    jeep_mat.instantiate();
    jeep_mat->set_albedo(Color(0.25f, 0.3f, 0.2f));  // Military green
    jeep_mat->set_roughness(0.7f);
    jeep_body->set_surface_override_material(0, jeep_mat);
    detail_meshes.push_back(jeep_body);
    
    // Jeep cab
    MeshInstance3D *cab = memnew(MeshInstance3D);
    cab->set_name("JeepCab");
    jeep->add_child(cab);
    Ref<BoxMesh> cab_mesh;
    cab_mesh.instantiate();
    cab_mesh->set_size(Vector3(2.0f, 0.8f, 1.8f));
    cab->set_mesh(cab_mesh);
    cab->set_position(Vector3(0, 1.7f, -0.5f));
    cab->set_surface_override_material(0, jeep_mat);
    detail_meshes.push_back(cab);
    
    // Jeep wheels
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            MeshInstance3D *wheel = memnew(MeshInstance3D);
            wheel->set_name("JeepWheel");
            jeep->add_child(wheel);
            Ref<CylinderMesh> w_mesh;
            w_mesh.instantiate();
            w_mesh->set_top_radius(0.4f);
            w_mesh->set_bottom_radius(0.4f);
            w_mesh->set_height(0.3f);
            wheel->set_mesh(w_mesh);
            wheel->set_position(Vector3(x * 1.1f, 0.4f, z * 1.3f));
            wheel->set_rotation_degrees(Vector3(0, 0, 90));
            wheel->set_surface_override_material(0, dark_metal_material);
            detail_meshes.push_back(wheel);
        }
    }
    
    // Fuel drums
    for (int i = 0; i < 3; i++) {
        MeshInstance3D *drum = memnew(MeshInstance3D);
        drum->set_name("FuelDrum");
        depot->add_child(drum);
        Ref<CylinderMesh> drum_mesh;
        drum_mesh.instantiate();
        drum_mesh->set_top_radius(0.35f);
        drum_mesh->set_bottom_radius(0.35f);
        drum_mesh->set_height(1.0f);
        drum->set_mesh(drum_mesh);
        drum->set_position(Vector3(3.2f, 0.5f, 3.0f - i * 0.8f));
        
        Ref<StandardMaterial3D> drum_mat;
        drum_mat.instantiate();
        drum_mat->set_albedo(Color(0.15f, 0.4f, 0.15f));  // Green fuel drum
        drum_mat->set_roughness(0.5f);
        drum_mat->set_metallic(0.6f);
        drum->set_surface_override_material(0, drum_mat);
        detail_meshes.push_back(drum);
    }
}

void Barracks::create_perimeter() {
    // Perimeter fence around the barracks complex
    float fence_height = 2.5f;
    float post_spacing = 4.0f;
    float perimeter_w = BUILDING_WIDTH + 20.0f;
    float perimeter_d = BUILDING_DEPTH + 25.0f;
    
    // Fence posts and chain-link sections
    auto create_fence_section = [this, fence_height](Vector3 start, Vector3 end) {
        Vector3 dir = (end - start).normalized();
        float length = (end - start).length();
        int num_posts = (int)(length / 4.0f) + 1;
        
        for (int i = 0; i <= num_posts; i++) {
            float t = (float)i / (float)num_posts;
            Vector3 pos = start.lerp(end, t);
            
            // Fence post
            MeshInstance3D *post = memnew(MeshInstance3D);
            post->set_name("FencePost");
            building_root->add_child(post);
            Ref<CylinderMesh> post_mesh;
            post_mesh.instantiate();
            post_mesh->set_top_radius(0.06f);
            post_mesh->set_bottom_radius(0.08f);
            post_mesh->set_height(fence_height + 0.3f);
            post->set_mesh(post_mesh);
            post->set_position(pos + Vector3(0, fence_height / 2.0f, 0));
            post->set_surface_override_material(0, metal_material);
            detail_meshes.push_back(post);
            
            // Post cap
            MeshInstance3D *cap = memnew(MeshInstance3D);
            cap->set_name("PostCap");
            building_root->add_child(cap);
            Ref<SphereMesh> cap_mesh;
            cap_mesh.instantiate();
            cap_mesh->set_radius(0.08f);
            cap_mesh->set_height(0.16f);
            cap->set_mesh(cap_mesh);
            cap->set_position(pos + Vector3(0, fence_height + 0.2f, 0));
            cap->set_surface_override_material(0, metal_material);
            detail_meshes.push_back(cap);
        }
        
        // Chain-link fence panel
        MeshInstance3D *fence = memnew(MeshInstance3D);
        fence->set_name("FencePanel");
        building_root->add_child(fence);
        Ref<BoxMesh> fence_mesh;
        fence_mesh.instantiate();
        fence_mesh->set_size(Vector3(length, fence_height - 0.3f, 0.05f));
        fence->set_mesh(fence_mesh);
        
        Vector3 center = (start + end) / 2.0f;
        fence->set_position(center + Vector3(0, fence_height / 2.0f, 0));
        
        // Calculate rotation to face direction
        float angle = Math::atan2(dir.x, dir.z);
        fence->set_rotation(Vector3(0, angle, 0));
        
        // Semi-transparent chain-link material
        Ref<StandardMaterial3D> fence_mat;
        fence_mat.instantiate();
        fence_mat->set_albedo(Color(0.4f, 0.4f, 0.4f, 0.7f));
        fence_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
        fence_mat->set_roughness(0.6f);
        fence_mat->set_metallic(0.5f);
        fence_mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
        fence->set_surface_override_material(0, fence_mat);
        detail_meshes.push_back(fence);
    };
    
    // Create fence around perimeter (leaving gap for entrance)
    float hw = perimeter_w / 2.0f;
    float hd = perimeter_d / 2.0f;
    
    // Back fence
    create_fence_section(Vector3(-hw, 0, -hd), Vector3(hw, 0, -hd));
    // Left fence
    create_fence_section(Vector3(-hw, 0, -hd), Vector3(-hw, 0, hd - 8.0f));
    // Right fence
    create_fence_section(Vector3(hw, 0, -hd), Vector3(hw, 0, hd - 8.0f));
    // Front fence (with gate gap)
    create_fence_section(Vector3(-hw, 0, hd), Vector3(-4.0f, 0, hd));
    create_fence_section(Vector3(4.0f, 0, hd), Vector3(hw, 0, hd));
    
    // Gate posts (larger)
    for (int x = -1; x <= 1; x += 2) {
        MeshInstance3D *gate_post = memnew(MeshInstance3D);
        gate_post->set_name("GatePost");
        building_root->add_child(gate_post);
        Ref<BoxMesh> gp_mesh;
        gp_mesh.instantiate();
        gp_mesh->set_size(Vector3(0.4f, 3.0f, 0.4f));
        gate_post->set_mesh(gp_mesh);
        gate_post->set_position(Vector3(x * 4.0f, 1.5f, hd));
        gate_post->set_surface_override_material(0, concrete_material);
        detail_meshes.push_back(gate_post);
    }
    
    // Gate header
    MeshInstance3D *gate_header = memnew(MeshInstance3D);
    gate_header->set_name("GateHeader");
    building_root->add_child(gate_header);
    Ref<BoxMesh> gh_mesh;
    gh_mesh.instantiate();
    gh_mesh->set_size(Vector3(8.5f, 0.5f, 0.5f));
    gate_header->set_mesh(gh_mesh);
    gate_header->set_position(Vector3(0, 3.25f, hd));
    gate_header->set_surface_override_material(0, concrete_material);
    detail_meshes.push_back(gate_header);
    
    // Barbed wire on top of fence (simplified as cylinders)
    for (int i = 0; i < 8; i++) {
        MeshInstance3D *wire = memnew(MeshInstance3D);
        wire->set_name("BarbedWire");
        building_root->add_child(wire);
        Ref<CylinderMesh> wire_mesh;
        wire_mesh.instantiate();
        wire_mesh->set_top_radius(0.02f);
        wire_mesh->set_bottom_radius(0.02f);
        wire_mesh->set_height(perimeter_w);
        wire->set_mesh(wire_mesh);
        wire->set_position(Vector3(0, fence_height + 0.35f, -hd + i * (perimeter_d / 7.0f)));
        wire->set_rotation_degrees(Vector3(0, 0, 90));
        wire->set_surface_override_material(0, dark_metal_material);
        detail_meshes.push_back(wire);
    }
}

} // namespace rts
