/**
 * CommandCenter.cpp
 * Highly detailed command center building with animated garage door and radar.
 */

#include "CommandCenter.h"
#include "Bulldozer.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/surface_tool.hpp>

using namespace godot;

namespace rts {

void CommandCenter::_bind_methods() {
    // Door controls
    ClassDB::bind_method(D_METHOD("open_garage_door"), &CommandCenter::open_garage_door);
    ClassDB::bind_method(D_METHOD("close_garage_door"), &CommandCenter::close_garage_door);
    ClassDB::bind_method(D_METHOD("toggle_garage_door"), &CommandCenter::toggle_garage_door);
    ClassDB::bind_method(D_METHOD("is_door_open"), &CommandCenter::is_door_open);
    ClassDB::bind_method(D_METHOD("is_door_animating"), &CommandCenter::is_door_animating);
    
    // Radar
    ClassDB::bind_method(D_METHOD("set_radar_speed", "speed"), &CommandCenter::set_radar_speed);
    ClassDB::bind_method(D_METHOD("get_radar_speed"), &CommandCenter::get_radar_speed);
    
    // Bulldozer spawning
    ClassDB::bind_method(D_METHOD("spawn_bulldozer"), &CommandCenter::spawn_bulldozer);
    ClassDB::bind_method(D_METHOD("queue_bulldozer"), &CommandCenter::queue_bulldozer);
    ClassDB::bind_method(D_METHOD("get_bulldozer_queue"), &CommandCenter::get_bulldozer_queue);
    ClassDB::bind_method(D_METHOD("get_build_progress"), &CommandCenter::get_build_progress);
    ClassDB::bind_method(D_METHOD("get_is_building"), &CommandCenter::get_is_building);
    ClassDB::bind_method(D_METHOD("get_spawn_point"), &CommandCenter::get_spawn_point);
    ClassDB::bind_method(D_METHOD("get_rally_point"), &CommandCenter::get_rally_point);
    
    // Signals
    ADD_SIGNAL(MethodInfo("door_opened"));
    ADD_SIGNAL(MethodInfo("door_closed"));
    ADD_SIGNAL(MethodInfo("bulldozer_spawned", PropertyInfo(Variant::OBJECT, "bulldozer")));
    ADD_SIGNAL(MethodInfo("bulldozer_build_started"));
    ADD_SIGNAL(MethodInfo("bulldozer_build_complete"));
}

CommandCenter::CommandCenter() {
    set_building_name("Command Center");
    set_building_size(BASE_WIDTH);
    set_building_height(BASE_HEIGHT + RADAR_HEIGHT);
    set_max_health(2000);
    set_health(2000);
    set_armor(15);
}

CommandCenter::~CommandCenter() {
}

void CommandCenter::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Set collision layer to 4 (buildings)
    set_collision_layer(4);
    set_collision_mask(0);
    
    create_command_center();
    
    // Setup collision (use parent implementation)
    setup_collision();
    
    // Snap to terrain
    snap_to_terrain();
    
    // Calculate spawn point (in front of garage)
    spawn_point = get_global_position() + Vector3(0, 0, BASE_DEPTH / 2.0f + 3.0f);
    
    // Schedule initial bulldozer spawn
    spawn_pending = true;
    spawn_delay_timer = 1.5f;  // Wait for door animation
    
    UtilityFunctions::print("CommandCenter: Ready - extremely detailed headquarters");
}

void CommandCenter::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Animate radar
    update_radar_rotation(delta);
    
    // Animate door
    update_door_animation(delta);
    
    // Handle bulldozer production
    update_bulldozer_production(delta);
    
    // Handle spawn delay
    if (spawn_pending) {
        spawn_delay_timer -= delta;
        if (spawn_delay_timer <= 0.0f) {
            spawn_pending = false;
            if (!has_spawned_initial_bulldozer) {
                open_garage_door();
                has_spawned_initial_bulldozer = true;
                // Spawn after door opens
                spawn_delay_timer = 1.0f / door_animation_speed + 0.3f;
                spawn_pending = true;
            } else {
                // Actually spawn the bulldozer
                spawn_bulldozer();
            }
        }
    }
}

void CommandCenter::create_command_center() {
    // Create materials first
    create_materials();
    
    // Create root node for all building parts
    building_root = memnew(Node3D);
    building_root->set_name("BuildingRoot");
    add_child(building_root);
    
    // Build all components - massive military command complex
    create_foundation();
    create_main_building();
    create_garage_section();
    create_vehicle_bay();
    create_control_tower();
    create_roof();
    create_garage_door();
    create_radar_system();
    create_communications_array();
    create_helipad();
    create_power_generators();
    create_fuel_depot();
    create_perimeter_walls();
    create_guard_posts();
    create_landing_lights();
    create_details();
    create_windows();
    
    UtilityFunctions::print("CommandCenter: Created massive military headquarters complex");
}

void CommandCenter::create_materials() {
    // Main building material - concrete/industrial
    main_material.instantiate();
    main_material->set_albedo(Color(0.55f, 0.52f, 0.48f));  // Warm concrete gray
    main_material->set_roughness(0.85f);
    main_material->set_metallic(0.0f);
    
    // Metal trim material
    metal_material.instantiate();
    metal_material->set_albedo(Color(0.7f, 0.72f, 0.74f));  // Light steel
    metal_material->set_roughness(0.35f);
    metal_material->set_metallic(0.8f);
    
    // Dark metal (vents, structural)
    dark_metal_material.instantiate();
    dark_metal_material->set_albedo(Color(0.25f, 0.25f, 0.28f));
    dark_metal_material->set_roughness(0.4f);
    dark_metal_material->set_metallic(0.7f);
    
    // Window material - tinted glass
    window_material.instantiate();
    window_material->set_albedo(Color(0.15f, 0.25f, 0.35f, 0.85f));
    window_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    window_material->set_roughness(0.05f);
    window_material->set_metallic(0.3f);
    
    // Garage door material - industrial yellow/orange
    door_material.instantiate();
    door_material->set_albedo(Color(0.85f, 0.55f, 0.1f));  // Safety orange
    door_material->set_roughness(0.6f);
    door_material->set_metallic(0.4f);
    
    // Accent material - military green
    accent_material.instantiate();
    accent_material->set_albedo(Color(0.25f, 0.35f, 0.2f));  // OD green
    accent_material->set_roughness(0.7f);
    accent_material->set_metallic(0.2f);
    
    // Radar dish material - white/off-white
    radar_material.instantiate();
    radar_material->set_albedo(Color(0.92f, 0.92f, 0.9f));
    radar_material->set_roughness(0.3f);
    radar_material->set_metallic(0.4f);
    
    // Light/emission material
    light_material.instantiate();
    light_material->set_albedo(Color(1.0f, 0.95f, 0.8f));
    light_material->set_feature(StandardMaterial3D::FEATURE_EMISSION, true);
    light_material->set_emission(Color(1.0f, 0.9f, 0.6f));
    light_material->set_emission_energy_multiplier(2.0f);
}

void CommandCenter::create_foundation() {
    // Concrete foundation/platform
    MeshInstance3D *foundation = memnew(MeshInstance3D);
    foundation->set_name("Foundation");
    building_root->add_child(foundation);
    
    Ref<BoxMesh> fnd_mesh;
    fnd_mesh.instantiate();
    fnd_mesh->set_size(Vector3(BASE_WIDTH + 1.5f, 0.4f, BASE_DEPTH + 1.5f));
    foundation->set_mesh(fnd_mesh);
    foundation->set_position(Vector3(0, 0.2f, 0));
    foundation->set_surface_override_material(0, main_material);
    
    // Foundation edge trim
    MeshInstance3D *trim = memnew(MeshInstance3D);
    trim->set_name("FoundationTrim");
    building_root->add_child(trim);
    
    Ref<BoxMesh> trim_mesh;
    trim_mesh.instantiate();
    trim_mesh->set_size(Vector3(BASE_WIDTH + 2.0f, 0.15f, BASE_DEPTH + 2.0f));
    trim->set_mesh(trim_mesh);
    trim->set_position(Vector3(0, 0.075f, 0));
    trim->set_surface_override_material(0, dark_metal_material);
    
    detail_meshes.push_back(foundation);
    detail_meshes.push_back(trim);
}

void CommandCenter::create_main_building() {
    // Main building body
    main_building = memnew(MeshInstance3D);
    main_building->set_name("MainBuilding");
    building_root->add_child(main_building);
    
    Ref<BoxMesh> main_mesh;
    main_mesh.instantiate();
    main_mesh->set_size(Vector3(BASE_WIDTH, BASE_HEIGHT, BASE_DEPTH * 0.6f));
    main_building->set_mesh(main_mesh);
    main_building->set_position(Vector3(0, BASE_HEIGHT / 2.0f + 0.4f, -BASE_DEPTH * 0.2f));
    main_building->set_surface_override_material(0, main_material);
    
    // Horizontal accent stripes
    for (int i = 0; i < 3; i++) {
        MeshInstance3D *stripe = memnew(MeshInstance3D);
        stripe->set_name(String("Stripe") + String::num_int64(i));
        building_root->add_child(stripe);
        
        Ref<BoxMesh> stripe_mesh;
        stripe_mesh.instantiate();
        stripe_mesh->set_size(Vector3(BASE_WIDTH + 0.1f, 0.12f, BASE_DEPTH * 0.6f + 0.1f));
        stripe->set_mesh(stripe_mesh);
        float y = 1.2f + i * 1.6f + 0.4f;
        stripe->set_position(Vector3(0, y, -BASE_DEPTH * 0.2f));
        stripe->set_surface_override_material(0, accent_material);
        detail_meshes.push_back(stripe);
    }
    
    // Corner pillars
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            if (z > 0) continue;  // Skip front corners (garage side)
            
            MeshInstance3D *pillar = memnew(MeshInstance3D);
            pillar->set_name("Pillar");
            building_root->add_child(pillar);
            
            Ref<BoxMesh> pillar_mesh;
            pillar_mesh.instantiate();
            pillar_mesh->set_size(Vector3(0.4f, BASE_HEIGHT + 0.2f, 0.4f));
            pillar->set_mesh(pillar_mesh);
            float px = x * (BASE_WIDTH / 2.0f - 0.2f);
            float pz = z * (BASE_DEPTH * 0.3f - 0.2f) - BASE_DEPTH * 0.2f;
            pillar->set_position(Vector3(px, BASE_HEIGHT / 2.0f + 0.4f, pz));
            pillar->set_surface_override_material(0, dark_metal_material);
            detail_meshes.push_back(pillar);
        }
    }
}

void CommandCenter::create_garage_section() {
    // Garage bay - front section
    garage_section = memnew(MeshInstance3D);
    garage_section->set_name("GarageSection");
    building_root->add_child(garage_section);
    
    Ref<BoxMesh> garage_mesh;
    garage_mesh.instantiate();
    garage_mesh->set_size(Vector3(BASE_WIDTH, GARAGE_HEIGHT, BASE_DEPTH * 0.4f));
    garage_section->set_mesh(garage_mesh);
    garage_section->set_position(Vector3(0, GARAGE_HEIGHT / 2.0f + 0.4f, BASE_DEPTH * 0.3f));
    garage_section->set_surface_override_material(0, main_material);
    
    // Garage frame - heavy industrial look
    MeshInstance3D *frame_top = memnew(MeshInstance3D);
    frame_top->set_name("GarageFrameTop");
    building_root->add_child(frame_top);
    
    Ref<BoxMesh> frame_mesh;
    frame_mesh.instantiate();
    frame_mesh->set_size(Vector3(5.5f, 0.4f, 0.5f));
    frame_top->set_mesh(frame_mesh);
    frame_top->set_position(Vector3(0, GARAGE_HEIGHT + 0.4f, BASE_DEPTH / 2.0f + 0.1f));
    frame_top->set_surface_override_material(0, metal_material);
    detail_meshes.push_back(frame_top);
    
    // Side frames
    for (int x = -1; x <= 1; x += 2) {
        MeshInstance3D *frame_side = memnew(MeshInstance3D);
        frame_side->set_name("GarageFrameSide");
        building_root->add_child(frame_side);
        
        Ref<BoxMesh> side_mesh;
        side_mesh.instantiate();
        side_mesh->set_size(Vector3(0.35f, GARAGE_HEIGHT, 0.5f));
        frame_side->set_mesh(side_mesh);
        frame_side->set_position(Vector3(x * 2.6f, GARAGE_HEIGHT / 2.0f + 0.4f, BASE_DEPTH / 2.0f + 0.1f));
        frame_side->set_surface_override_material(0, metal_material);
        detail_meshes.push_back(frame_side);
    }
    
    // Warning stripes on floor in front of garage
    for (int i = 0; i < 6; i++) {
        MeshInstance3D *stripe = memnew(MeshInstance3D);
        stripe->set_name("WarningStripe");
        building_root->add_child(stripe);
        
        Ref<BoxMesh> stripe_mesh;
        stripe_mesh.instantiate();
        stripe_mesh->set_size(Vector3(0.4f, 0.02f, 2.5f));
        stripe->set_mesh(stripe_mesh);
        stripe->set_position(Vector3(-2.0f + i * 0.8f, 0.41f, BASE_DEPTH / 2.0f + 1.5f));
        
        Ref<StandardMaterial3D> stripe_mat;
        stripe_mat.instantiate();
        stripe_mat->set_albedo(i % 2 == 0 ? Color(0.9f, 0.8f, 0.0f) : Color(0.15f, 0.15f, 0.15f));
        stripe->set_surface_override_material(0, stripe_mat);
        detail_meshes.push_back(stripe);
    }
}

void CommandCenter::create_control_tower() {
    // Elevated control room / observation deck
    control_tower = memnew(MeshInstance3D);
    control_tower->set_name("ControlTower");
    building_root->add_child(control_tower);
    
    Ref<BoxMesh> tower_mesh;
    tower_mesh.instantiate();
    tower_mesh->set_size(Vector3(4.0f, 2.0f, 3.0f));
    control_tower->set_mesh(tower_mesh);
    control_tower->set_position(Vector3(0, BASE_HEIGHT + 1.4f, -BASE_DEPTH * 0.35f));
    control_tower->set_surface_override_material(0, main_material);
    
    // Tower windows (wide panoramic)
    MeshInstance3D *tower_window = memnew(MeshInstance3D);
    tower_window->set_name("TowerWindow");
    building_root->add_child(tower_window);
    
    Ref<BoxMesh> window_mesh;
    window_mesh.instantiate();
    window_mesh->set_size(Vector3(3.5f, 1.2f, 0.1f));
    tower_window->set_mesh(window_mesh);
    tower_window->set_position(Vector3(0, BASE_HEIGHT + 1.4f, -BASE_DEPTH * 0.35f + 1.55f));
    tower_window->set_surface_override_material(0, window_material);
    detail_meshes.push_back(tower_window);
    
    // Side windows
    for (int x = -1; x <= 1; x += 2) {
        MeshInstance3D *side_win = memnew(MeshInstance3D);
        side_win->set_name("TowerSideWindow");
        building_root->add_child(side_win);
        
        Ref<BoxMesh> sw_mesh;
        sw_mesh.instantiate();
        sw_mesh->set_size(Vector3(0.1f, 1.2f, 2.5f));
        side_win->set_mesh(sw_mesh);
        side_win->set_position(Vector3(x * 2.05f, BASE_HEIGHT + 1.4f, -BASE_DEPTH * 0.35f));
        side_win->set_surface_override_material(0, window_material);
        detail_meshes.push_back(side_win);
    }
    
    // Tower roof overhang
    MeshInstance3D *overhang = memnew(MeshInstance3D);
    overhang->set_name("TowerOverhang");
    building_root->add_child(overhang);
    
    Ref<BoxMesh> oh_mesh;
    oh_mesh.instantiate();
    oh_mesh->set_size(Vector3(4.5f, 0.2f, 3.5f));
    overhang->set_mesh(oh_mesh);
    overhang->set_position(Vector3(0, BASE_HEIGHT + 2.5f, -BASE_DEPTH * 0.35f));
    overhang->set_surface_override_material(0, dark_metal_material);
    detail_meshes.push_back(overhang);
}

void CommandCenter::create_roof() {
    // Main roof
    roof = memnew(MeshInstance3D);
    roof->set_name("MainRoof");
    building_root->add_child(roof);
    
    Ref<BoxMesh> roof_mesh;
    roof_mesh.instantiate();
    roof_mesh->set_size(Vector3(BASE_WIDTH + 0.3f, 0.3f, BASE_DEPTH * 0.6f + 0.3f));
    roof->set_mesh(roof_mesh);
    roof->set_position(Vector3(0, BASE_HEIGHT + 0.55f, -BASE_DEPTH * 0.2f));
    roof->set_surface_override_material(0, dark_metal_material);
    
    // Garage roof
    MeshInstance3D *garage_roof = memnew(MeshInstance3D);
    garage_roof->set_name("GarageRoof");
    building_root->add_child(garage_roof);
    
    Ref<BoxMesh> gr_mesh;
    gr_mesh.instantiate();
    gr_mesh->set_size(Vector3(BASE_WIDTH + 0.3f, 0.25f, BASE_DEPTH * 0.4f + 0.3f));
    garage_roof->set_mesh(gr_mesh);
    garage_roof->set_position(Vector3(0, GARAGE_HEIGHT + 0.52f, BASE_DEPTH * 0.3f));
    garage_roof->set_surface_override_material(0, dark_metal_material);
    detail_meshes.push_back(garage_roof);
    
    // Roof edge trim
    for (int x = -1; x <= 1; x += 2) {
        MeshInstance3D *edge = memnew(MeshInstance3D);
        edge->set_name("RoofEdge");
        building_root->add_child(edge);
        
        Ref<BoxMesh> edge_mesh;
        edge_mesh.instantiate();
        edge_mesh->set_size(Vector3(0.15f, 0.5f, BASE_DEPTH + 0.5f));
        edge->set_mesh(edge_mesh);
        edge->set_position(Vector3(x * (BASE_WIDTH / 2.0f + 0.15f), BASE_HEIGHT + 0.4f, 0));
        edge->set_surface_override_material(0, metal_material);
        detail_meshes.push_back(edge);
    }
}

void CommandCenter::create_garage_door() {
    // Create pivot point at top of door opening
    garage_door_pivot = memnew(Node3D);
    garage_door_pivot->set_name("GarageDoorPivot");
    building_root->add_child(garage_door_pivot);
    garage_door_pivot->set_position(Vector3(0, GARAGE_HEIGHT + 0.35f, BASE_DEPTH / 2.0f + 0.12f));
    
    // Create segmented roll-up door (5 segments)
    int num_segments = 5;
    float segment_height = (GARAGE_HEIGHT - 0.3f) / num_segments;
    float door_width = 4.8f;
    float door_depth = 0.12f;
    
    for (int i = 0; i < num_segments; i++) {
        MeshInstance3D *segment = memnew(MeshInstance3D);
        segment->set_name(String("DoorSegment") + String::num_int64(i));
        garage_door_pivot->add_child(segment);
        
        Ref<BoxMesh> seg_mesh;
        seg_mesh.instantiate();
        seg_mesh->set_size(Vector3(door_width, segment_height - 0.05f, door_depth));
        segment->set_mesh(seg_mesh);
        
        // Position segments stacked from top
        float y = -(i + 0.5f) * segment_height;
        segment->set_position(Vector3(0, y, 0));
        segment->set_surface_override_material(0, door_material);
        
        door_segments.push_back(segment);
    }
    
    // Add door details - horizontal ribs
    for (int i = 0; i < num_segments; i++) {
        MeshInstance3D *rib = memnew(MeshInstance3D);
        rib->set_name("DoorRib");
        door_segments[i]->add_child(rib);
        
        Ref<BoxMesh> rib_mesh;
        rib_mesh.instantiate();
        rib_mesh->set_size(Vector3(door_width + 0.02f, 0.08f, door_depth + 0.02f));
        rib->set_mesh(rib_mesh);
        rib->set_position(Vector3(0, segment_height / 2.0f - 0.04f, 0));
        rib->set_surface_override_material(0, dark_metal_material);
    }
    
    garage_door = door_segments[0];  // Reference to first segment
    
    UtilityFunctions::print("CommandCenter: Created segmented garage door with ", num_segments, " segments");
}

void CommandCenter::create_radar_system() {
    // Radar pivot on top of control tower
    radar_pivot = memnew(Node3D);
    radar_pivot->set_name("RadarPivot");
    building_root->add_child(radar_pivot);
    radar_pivot->set_position(Vector3(0, BASE_HEIGHT + 2.7f, -BASE_DEPTH * 0.35f));
    
    // Radar base/pedestal
    radar_base = memnew(MeshInstance3D);
    radar_base->set_name("RadarBase");
    radar_pivot->add_child(radar_base);
    
    Ref<CylinderMesh> base_mesh;
    base_mesh.instantiate();
    base_mesh->set_top_radius(0.4f);
    base_mesh->set_bottom_radius(0.5f);
    base_mesh->set_height(0.6f);
    radar_base->set_mesh(base_mesh);
    radar_base->set_position(Vector3(0, 0.3f, 0));
    radar_base->set_surface_override_material(0, dark_metal_material);
    
    // Radar arm (horizontal)
    radar_arm = memnew(MeshInstance3D);
    radar_arm->set_name("RadarArm");
    radar_pivot->add_child(radar_arm);
    
    Ref<BoxMesh> arm_mesh;
    arm_mesh.instantiate();
    arm_mesh->set_size(Vector3(0.15f, 0.2f, 1.8f));
    radar_arm->set_mesh(arm_mesh);
    radar_arm->set_position(Vector3(0, 0.7f, 0.9f));
    radar_arm->set_surface_override_material(0, metal_material);
    
    // Radar dish (parabolic)
    radar_dish = memnew(MeshInstance3D);
    radar_dish->set_name("RadarDish");
    radar_pivot->add_child(radar_dish);
    
    // Use sphere section for dish
    Ref<SphereMesh> dish_mesh;
    dish_mesh.instantiate();
    dish_mesh->set_radius(1.2f);
    dish_mesh->set_height(0.6f);
    dish_mesh->set_radial_segments(24);
    dish_mesh->set_rings(8);
    radar_dish->set_mesh(dish_mesh);
    radar_dish->set_position(Vector3(0, 0.7f, 1.9f));
    radar_dish->set_rotation_degrees(Vector3(-90, 0, 0));
    radar_dish->set_surface_override_material(0, radar_material);
    
    // Radar center cone/feed
    MeshInstance3D *feed = memnew(MeshInstance3D);
    feed->set_name("RadarFeed");
    radar_pivot->add_child(feed);
    
    Ref<CylinderMesh> feed_mesh;
    feed_mesh.instantiate();
    feed_mesh->set_top_radius(0.05f);
    feed_mesh->set_bottom_radius(0.1f);
    feed_mesh->set_height(0.4f);
    feed->set_mesh(feed_mesh);
    feed->set_position(Vector3(0, 0.7f, 1.5f));
    feed->set_rotation_degrees(Vector3(-90, 0, 0));
    feed->set_surface_override_material(0, dark_metal_material);
    
    UtilityFunctions::print("CommandCenter: Created rotating radar system");
}

void CommandCenter::create_details() {
    // AC unit on main roof
    ac_unit = memnew(MeshInstance3D);
    ac_unit->set_name("ACUnit");
    building_root->add_child(ac_unit);
    
    Ref<BoxMesh> ac_mesh;
    ac_mesh.instantiate();
    ac_mesh->set_size(Vector3(2.0f, 0.8f, 1.5f));
    ac_unit->set_mesh(ac_mesh);
    ac_unit->set_position(Vector3(-3.5f, BASE_HEIGHT + 1.1f, -BASE_DEPTH * 0.2f));
    ac_unit->set_surface_override_material(0, metal_material);
    
    // AC fan grille
    MeshInstance3D *ac_grille = memnew(MeshInstance3D);
    ac_grille->set_name("ACGrille");
    building_root->add_child(ac_grille);
    
    Ref<CylinderMesh> grille_mesh;
    grille_mesh.instantiate();
    grille_mesh->set_top_radius(0.35f);
    grille_mesh->set_bottom_radius(0.35f);
    grille_mesh->set_height(0.1f);
    ac_grille->set_mesh(grille_mesh);
    ac_grille->set_position(Vector3(-3.5f, BASE_HEIGHT + 1.55f, -BASE_DEPTH * 0.2f));
    ac_grille->set_surface_override_material(0, dark_metal_material);
    
    // Exhaust vent
    exhaust_vent = memnew(MeshInstance3D);
    exhaust_vent->set_name("ExhaustVent");
    building_root->add_child(exhaust_vent);
    
    Ref<BoxMesh> vent_mesh;
    vent_mesh.instantiate();
    vent_mesh->set_size(Vector3(0.8f, 1.2f, 0.8f));
    exhaust_vent->set_mesh(vent_mesh);
    exhaust_vent->set_position(Vector3(3.5f, BASE_HEIGHT + 1.3f, -BASE_DEPTH * 0.1f));
    exhaust_vent->set_surface_override_material(0, dark_metal_material);
    
    // Vent cap
    MeshInstance3D *vent_cap = memnew(MeshInstance3D);
    vent_cap->set_name("VentCap");
    building_root->add_child(vent_cap);
    
    Ref<BoxMesh> cap_mesh;
    cap_mesh.instantiate();
    cap_mesh->set_size(Vector3(1.0f, 0.1f, 1.0f));
    vent_cap->set_mesh(cap_mesh);
    vent_cap->set_position(Vector3(3.5f, BASE_HEIGHT + 1.95f, -BASE_DEPTH * 0.1f));
    vent_cap->set_surface_override_material(0, metal_material);
    
    // Antennas
    antenna1 = memnew(MeshInstance3D);
    antenna1->set_name("Antenna1");
    building_root->add_child(antenna1);
    
    Ref<CylinderMesh> ant1_mesh;
    ant1_mesh.instantiate();
    ant1_mesh->set_top_radius(0.03f);
    ant1_mesh->set_bottom_radius(0.05f);
    ant1_mesh->set_height(2.5f);
    antenna1->set_mesh(ant1_mesh);
    antenna1->set_position(Vector3(-5.0f, BASE_HEIGHT + 1.95f, -BASE_DEPTH * 0.4f));
    antenna1->set_surface_override_material(0, metal_material);
    
    antenna2 = memnew(MeshInstance3D);
    antenna2->set_name("Antenna2");
    building_root->add_child(antenna2);
    
    Ref<CylinderMesh> ant2_mesh;
    ant2_mesh.instantiate();
    ant2_mesh->set_top_radius(0.02f);
    ant2_mesh->set_bottom_radius(0.04f);
    ant2_mesh->set_height(3.0f);
    antenna2->set_mesh(ant2_mesh);
    antenna2->set_position(Vector3(5.0f, BASE_HEIGHT + 2.2f, -BASE_DEPTH * 0.4f));
    antenna2->set_surface_override_material(0, metal_material);
    
    // Small satellite dish
    satellite_dish = memnew(MeshInstance3D);
    satellite_dish->set_name("SatelliteDish");
    building_root->add_child(satellite_dish);
    
    Ref<SphereMesh> sat_mesh;
    sat_mesh.instantiate();
    sat_mesh->set_radius(0.5f);
    sat_mesh->set_height(0.25f);
    sat_mesh->set_radial_segments(16);
    sat_mesh->set_rings(6);
    satellite_dish->set_mesh(sat_mesh);
    satellite_dish->set_position(Vector3(4.5f, BASE_HEIGHT + 0.9f, -BASE_DEPTH * 0.45f));
    satellite_dish->set_rotation_degrees(Vector3(-45, -30, 0));
    satellite_dish->set_surface_override_material(0, radar_material);
    
    // Flag pole
    flag_pole = memnew(MeshInstance3D);
    flag_pole->set_name("FlagPole");
    building_root->add_child(flag_pole);
    
    Ref<CylinderMesh> pole_mesh;
    pole_mesh.instantiate();
    pole_mesh->set_top_radius(0.03f);
    pole_mesh->set_bottom_radius(0.05f);
    pole_mesh->set_height(4.0f);
    flag_pole->set_mesh(pole_mesh);
    flag_pole->set_position(Vector3(-BASE_WIDTH / 2.0f - 0.5f, 2.4f, BASE_DEPTH / 2.0f - 0.5f));
    flag_pole->set_surface_override_material(0, metal_material);
    
    // Flag
    MeshInstance3D *flag = memnew(MeshInstance3D);
    flag->set_name("Flag");
    building_root->add_child(flag);
    
    Ref<BoxMesh> flag_mesh;
    flag_mesh.instantiate();
    flag_mesh->set_size(Vector3(1.2f, 0.8f, 0.02f));
    flag->set_mesh(flag_mesh);
    flag->set_position(Vector3(-BASE_WIDTH / 2.0f - 0.5f + 0.65f, 4.0f, BASE_DEPTH / 2.0f - 0.5f));
    flag->set_surface_override_material(0, accent_material);
    
    // Warning lights on corners
    for (int i = 0; i < 2; i++) {
        MeshInstance3D *light = memnew(MeshInstance3D);
        light->set_name(String("WarningLight") + String::num_int64(i));
        building_root->add_child(light);
        
        Ref<SphereMesh> light_mesh;
        light_mesh.instantiate();
        light_mesh->set_radius(0.15f);
        light_mesh->set_height(0.3f);
        light->set_mesh(light_mesh);
        float x = (i == 0) ? -BASE_WIDTH / 2.0f + 0.2f : BASE_WIDTH / 2.0f - 0.2f;
        light->set_position(Vector3(x, BASE_HEIGHT + 0.85f, -BASE_DEPTH * 0.5f + 0.2f));
        light->set_surface_override_material(0, light_material);
        detail_meshes.push_back(light);
    }
    
    // Pipes on side
    for (int i = 0; i < 3; i++) {
        MeshInstance3D *pipe = memnew(MeshInstance3D);
        pipe->set_name("Pipe");
        building_root->add_child(pipe);
        
        Ref<CylinderMesh> pipe_mesh;
        pipe_mesh.instantiate();
        pipe_mesh->set_top_radius(0.08f);
        pipe_mesh->set_bottom_radius(0.08f);
        pipe_mesh->set_height(BASE_HEIGHT);
        pipe->set_mesh(pipe_mesh);
        pipe->set_position(Vector3(-BASE_WIDTH / 2.0f - 0.15f, BASE_HEIGHT / 2.0f + 0.4f, -1.0f + i * 1.2f));
        pipe->set_surface_override_material(0, dark_metal_material);
        detail_meshes.push_back(pipe);
    }
    
    UtilityFunctions::print("CommandCenter: Created all detail meshes");
}

void CommandCenter::create_windows() {
    // Main building windows - two rows
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 4; col++) {
            // Front windows (facing garage)
            MeshInstance3D *window = memnew(MeshInstance3D);
            window->set_name("Window");
            building_root->add_child(window);
            
            Ref<BoxMesh> win_mesh;
            win_mesh.instantiate();
            win_mesh->set_size(Vector3(1.0f, 0.8f, 0.08f));
            window->set_mesh(win_mesh);
            float x = -2.25f + col * 1.5f;
            float y = 2.0f + row * 1.5f + 0.4f;
            window->set_position(Vector3(x, y, -BASE_DEPTH * 0.2f + BASE_DEPTH * 0.3f + 0.05f));
            window->set_surface_override_material(0, window_material);
            detail_meshes.push_back(window);
            
            // Back windows
            MeshInstance3D *back_win = memnew(MeshInstance3D);
            back_win->set_name("BackWindow");
            building_root->add_child(back_win);
            back_win->set_mesh(win_mesh);
            back_win->set_position(Vector3(x, y, -BASE_DEPTH * 0.2f - BASE_DEPTH * 0.3f - 0.05f));
            back_win->set_surface_override_material(0, window_material);
            detail_meshes.push_back(back_win);
        }
    }
    
    // Side windows
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 3; i++) {
            MeshInstance3D *side_win = memnew(MeshInstance3D);
            side_win->set_name("SideWindow");
            building_root->add_child(side_win);
            
            Ref<BoxMesh> sw_mesh;
            sw_mesh.instantiate();
            sw_mesh->set_size(Vector3(0.08f, 0.8f, 1.0f));
            side_win->set_mesh(sw_mesh);
            float x = side * (BASE_WIDTH / 2.0f + 0.05f);
            float z = -BASE_DEPTH * 0.35f + i * 1.2f;
            side_win->set_position(Vector3(x, 2.5f + 0.4f, z));
            side_win->set_surface_override_material(0, window_material);
            detail_meshes.push_back(side_win);
        }
    }
}

void CommandCenter::open_garage_door() {
    if (door_is_open || door_animating) return;
    door_animating = true;
    door_target_open = true;
    UtilityFunctions::print("CommandCenter: Opening garage door");
}

void CommandCenter::close_garage_door() {
    if (!door_is_open || door_animating) return;
    door_animating = true;
    door_target_open = false;
    UtilityFunctions::print("CommandCenter: Closing garage door");
}

void CommandCenter::toggle_garage_door() {
    if (door_is_open) {
        close_garage_door();
    } else {
        open_garage_door();
    }
}

void CommandCenter::update_door_animation(double delta) {
    if (!door_animating) return;
    
    float target = door_target_open ? 1.0f : 0.0f;
    float diff = target - door_open_amount;
    
    if (Math::abs(diff) < 0.01f) {
        door_open_amount = target;
        door_animating = false;
        door_is_open = door_target_open;
        
        if (door_is_open) {
            emit_signal("door_opened");
        } else {
            emit_signal("door_closed");
        }
        return;
    }
    
    float speed = door_animation_speed * delta;
    door_open_amount += (diff > 0 ? 1 : -1) * speed;
    door_open_amount = Math::clamp(door_open_amount, 0.0f, 1.0f);
    
    // Animate door segments rolling up
    if (garage_door_pivot) {
        // Roll up animation - segments move up and curve back
        float segment_height = (GARAGE_HEIGHT - 0.3f) / door_segments.size();
        
        for (size_t i = 0; i < door_segments.size(); i++) {
            MeshInstance3D *seg = door_segments[i];
            if (!seg) continue;
            
            // Calculate segment animation based on open amount
            // Lower segments start moving first
            float segment_delay = (float)i / door_segments.size() * 0.5f;
            float segment_progress = Math::clamp((door_open_amount - segment_delay) / (1.0f - segment_delay), 0.0f, 1.0f);
            
            // Move up and rotate back
            float base_y = -(i + 0.5f) * segment_height;
            float lift = segment_progress * (GARAGE_HEIGHT - 0.3f + i * segment_height * 0.8f);
            float roll = segment_progress * 90.0f;  // Roll back 90 degrees
            
            // Add curve effect - segments curve over the top
            float curve_offset = Math::sin(segment_progress * Math_PI * 0.5f) * 0.5f;
            
            seg->set_position(Vector3(0, base_y + lift, -curve_offset * segment_progress));
            seg->set_rotation_degrees(Vector3(roll, 0, 0));
        }
    }
}

bool CommandCenter::is_door_open() const {
    return door_is_open;
}

bool CommandCenter::is_door_animating() const {
    return door_animating;
}

void CommandCenter::update_radar_rotation(double delta) {
    if (!radar_pivot) return;
    
    radar_rotation += radar_rotation_speed * delta;
    if (radar_rotation >= 360.0f) {
        radar_rotation -= 360.0f;
    }
    
    radar_pivot->set_rotation_degrees(Vector3(0, radar_rotation, 0));
}

void CommandCenter::set_radar_speed(float speed) {
    radar_rotation_speed = speed;
}

float CommandCenter::get_radar_speed() const {
    return radar_rotation_speed;
}

void CommandCenter::spawn_bulldozer() {
    // Create bulldozer at spawn point
    Bulldozer *bulldozer = memnew(Bulldozer);
    
    // Add to scene
    Node *parent = get_parent();
    if (parent) {
        parent->add_child(bulldozer);
        
        // Position at spawn point (in front of garage)
        Vector3 pos = get_global_position();
        pos.z += BASE_DEPTH / 2.0f + 2.0f;  // In front of garage
        pos.y += 0.1f;
        bulldozer->set_global_position(pos);
        
        // Make it face away from building
        bulldozer->set_rotation_degrees(Vector3(0, 0, 0));
        
        UtilityFunctions::print("CommandCenter: Spawned bulldozer at ", pos);
        emit_signal("bulldozer_spawned", bulldozer);
        
        // Close door after a delay
        // (Will be handled in next process frame)
    }
}

void CommandCenter::queue_bulldozer() {
    bulldozer_queue++;
    
    if (!is_building_bulldozer) {
        is_building_bulldozer = true;
        build_progress = 0.0f;
        emit_signal("bulldozer_build_started");
    }
    
    UtilityFunctions::print("CommandCenter: Bulldozer queued. Queue size: ", bulldozer_queue);
}

void CommandCenter::update_bulldozer_production(double delta) {
    if (!is_building_bulldozer || bulldozer_queue <= 0) return;
    
    build_progress += delta / bulldozer_build_time;
    
    if (build_progress >= 1.0f) {
        // Build complete
        build_progress = 0.0f;
        bulldozer_queue--;
        
        emit_signal("bulldozer_build_complete");
        
        // Open door and spawn
        open_garage_door();
        spawn_pending = true;
        spawn_delay_timer = 1.0f / door_animation_speed + 0.3f;
        
        if (bulldozer_queue <= 0) {
            is_building_bulldozer = false;
        }
    }
}

Vector3 CommandCenter::get_spawn_point() const {
    Vector3 pos = get_global_position();
    pos.z += BASE_DEPTH / 2.0f + 2.0f;
    return pos;
}

Vector3 CommandCenter::get_rally_point() const {
    Vector3 pos = get_global_position();
    pos.z += BASE_DEPTH / 2.0f + 8.0f;
    return pos;
}

int CommandCenter::get_bulldozer_queue() const {
    return bulldozer_queue;
}

float CommandCenter::get_build_progress() const {
    return build_progress;
}

bool CommandCenter::get_is_building() const {
    return is_building_bulldozer;
}

// Mesh generation helpers
Ref<ArrayMesh> CommandCenter::create_box_mesh(float width, float height, float depth) {
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    
    // Use BoxMesh as base
    Ref<BoxMesh> box;
    box.instantiate();
    box->set_size(Vector3(width, height, depth));
    
    // Convert to ArrayMesh
    Array arrays = box->get_mesh_arrays();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    
    return mesh;
}

Ref<ArrayMesh> CommandCenter::create_cylinder_mesh(float radius, float height, int segments) {
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    
    Ref<CylinderMesh> cyl;
    cyl.instantiate();
    cyl->set_top_radius(radius);
    cyl->set_bottom_radius(radius);
    cyl->set_height(height);
    cyl->set_radial_segments(segments);
    
    Array arrays = cyl->get_mesh_arrays();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    
    return mesh;
}

Ref<ArrayMesh> CommandCenter::create_cone_mesh(float radius, float height, int segments) {
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    
    Ref<CylinderMesh> cone;
    cone.instantiate();
    cone->set_top_radius(0.0f);
    cone->set_bottom_radius(radius);
    cone->set_height(height);
    cone->set_radial_segments(segments);
    
    Array arrays = cone->get_mesh_arrays();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    
    return mesh;
}

Ref<ArrayMesh> CommandCenter::create_radar_dish_mesh(float radius, float depth, int segments) {
    // Uses sphere mesh section for parabolic dish
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    
    Ref<SphereMesh> sphere;
    sphere.instantiate();
    sphere->set_radius(radius);
    sphere->set_height(depth);
    sphere->set_radial_segments(segments);
    sphere->set_rings(segments / 3);
    
    Array arrays = sphere->get_mesh_arrays();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    
    return mesh;
}

Ref<ArrayMesh> CommandCenter::create_door_segment_mesh(float width, float height, float depth) {
    return create_box_mesh(width, height, depth);
}

} // namespace rts
