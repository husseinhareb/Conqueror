/**
 * TerrainGenerator.cpp
 * Procedural terrain generation implementation.
 */

#include "TerrainGenerator.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <vector>

using namespace godot;

namespace rts {

void TerrainGenerator::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("generate_terrain"), &TerrainGenerator::generate_terrain);
    ClassDB::bind_method(D_METHOD("generate_terrain_with_seed", "seed"), &TerrainGenerator::generate_terrain_with_seed);
    ClassDB::bind_method(D_METHOD("clear_terrain"), &TerrainGenerator::clear_terrain);
    
    ClassDB::bind_method(D_METHOD("get_height_at", "x", "z"), &TerrainGenerator::get_height_at);
    ClassDB::bind_method(D_METHOD("is_water_at", "x", "z"), &TerrainGenerator::is_water_at);
    ClassDB::bind_method(D_METHOD("is_buildable_at", "x", "z"), &TerrainGenerator::is_buildable_at);
    ClassDB::bind_method(D_METHOD("is_within_bounds", "x", "z"), &TerrainGenerator::is_within_bounds);
    ClassDB::bind_method(D_METHOD("get_world_size"), &TerrainGenerator::get_world_size);
    ClassDB::bind_method(D_METHOD("get_world_center"), &TerrainGenerator::get_world_center);
    
    // Properties
    ClassDB::bind_method(D_METHOD("set_map_size", "size"), &TerrainGenerator::set_map_size);
    ClassDB::bind_method(D_METHOD("get_map_size"), &TerrainGenerator::get_map_size);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "map_size", PROPERTY_HINT_RANGE, "32,512,32"), "set_map_size", "get_map_size");
    
    ClassDB::bind_method(D_METHOD("set_tile_size", "size"), &TerrainGenerator::set_tile_size);
    ClassDB::bind_method(D_METHOD("get_tile_size"), &TerrainGenerator::get_tile_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tile_size", PROPERTY_HINT_RANGE, "0.5,4.0,0.5"), "set_tile_size", "get_tile_size");
    
    ClassDB::bind_method(D_METHOD("set_max_height", "height"), &TerrainGenerator::set_max_height);
    ClassDB::bind_method(D_METHOD("get_max_height"), &TerrainGenerator::get_max_height);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height", PROPERTY_HINT_RANGE, "5.0,100.0,5.0"), "set_max_height", "get_max_height");
    
    ClassDB::bind_method(D_METHOD("set_water_level", "level"), &TerrainGenerator::set_water_level);
    ClassDB::bind_method(D_METHOD("get_water_level"), &TerrainGenerator::get_water_level);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "water_level", PROPERTY_HINT_RANGE, "-20.0,20.0,0.5"), "set_water_level", "get_water_level");
    
    ClassDB::bind_method(D_METHOD("set_seed", "seed"), &TerrainGenerator::set_seed);
    ClassDB::bind_method(D_METHOD("get_seed"), &TerrainGenerator::get_seed);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
    
    ClassDB::bind_method(D_METHOD("set_mountain_frequency", "freq"), &TerrainGenerator::set_mountain_frequency);
    ClassDB::bind_method(D_METHOD("get_mountain_frequency"), &TerrainGenerator::get_mountain_frequency);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mountain_frequency", PROPERTY_HINT_RANGE, "0.001,0.1,0.001"), "set_mountain_frequency", "get_mountain_frequency");
    
    ClassDB::bind_method(D_METHOD("set_lake_count", "count"), &TerrainGenerator::set_lake_count);
    ClassDB::bind_method(D_METHOD("get_lake_count"), &TerrainGenerator::get_lake_count);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "lake_count", PROPERTY_HINT_RANGE, "0,10,1"), "set_lake_count", "get_lake_count");
}

TerrainGenerator::TerrainGenerator() {
}

TerrainGenerator::~TerrainGenerator() {
}

void TerrainGenerator::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Auto-generate terrain on ready
    generate_terrain();
}

// Simple hash-based noise function
float TerrainGenerator::noise2d(float x, float y, int seed) {
    int xi = (int)floor(x);
    int yi = (int)floor(y);
    float xf = x - xi;
    float yf = y - yi;
    
    // Smoothstep interpolation
    float u = xf * xf * (3.0f - 2.0f * xf);
    float v = yf * yf * (3.0f - 2.0f * yf);
    
    // Hash function for corner values
    auto hash = [seed](int x, int y) -> float {
        int n = x + y * 57 + seed * 131;
        n = (n << 13) ^ n;
        return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
    };
    
    float n00 = hash(xi, yi);
    float n10 = hash(xi + 1, yi);
    float n01 = hash(xi, yi + 1);
    float n11 = hash(xi + 1, yi + 1);
    
    float nx0 = lerp(n00, n10, u);
    float nx1 = lerp(n01, n11, u);
    
    return lerp(nx0, nx1, v);
}

float TerrainGenerator::fbm_noise(float x, float y, int octaves, float persistence, float lacunarity, float frequency, int seed) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float max_value = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += noise2d(x * frequency, y * frequency, seed + i * 1000) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    return value / max_value;
}

float TerrainGenerator::smoothstep(float edge0, float edge1, float x) {
    x = Math::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

float TerrainGenerator::lerp(float a, float b, float t) {
    return a + t * (b - a);
}

void TerrainGenerator::generate_terrain() {
    generate_terrain_with_seed(config.seed);
}

void TerrainGenerator::generate_terrain_with_seed(int seed) {
    config.seed = seed;
    
    UtilityFunctions::print("TerrainGenerator: Starting terrain generation with seed ", seed);
    UtilityFunctions::print("TerrainGenerator: map_size=", config.map_size, " tile_size=", config.tile_size, " max_height=", config.max_height);
    
    // Clear existing terrain
    clear_terrain();
    
    // Initialize heightmap array
    int size = config.map_size;
    heightmap.resize(size * size);
    
    // Generate terrain in steps
    generate_base_heightmap();
    apply_mountains();
    carve_lakes();
    smooth_terrain(2);
    generate_normalmap();
    generate_splatmap();
    
    // Debug: print some height samples
    float min_h = 9999.0f, max_h = -9999.0f, sum_h = 0.0f;
    for (int i = 0; i < size * size; i++) {
        float h = heightmap[i] * config.max_height;
        min_h = Math::min(min_h, h);
        max_h = Math::max(max_h, h);
        sum_h += h;
    }
    float avg_h = sum_h / (size * size);
    float world_size = size * config.tile_size;
    UtilityFunctions::print("TerrainGenerator: Height range: min=", min_h, " max=", max_h, " avg=", avg_h, " water_level=", config.water_level);
    UtilityFunctions::print("TerrainGenerator: World size=", world_size, " (from -", world_size/2, " to +", world_size/2, ")");
    
    // Create visual mesh and collision
    create_terrain_mesh();
    create_terrain_collision();
    create_water_plane();
    
    // Setup shaders and materials
    generate_procedural_textures();
    setup_terrain_shader();
    apply_terrain_material();
    
    // Generate vegetation
    generate_trees();
    generate_grass();
    
    // Setup environment (fog, lighting, sky)
    setup_environment();
    
    UtilityFunctions::print("TerrainGenerator: Terrain generation complete");
}

void TerrainGenerator::clear_terrain() {
    if (terrain_mesh) {
        terrain_mesh->queue_free();
        terrain_mesh = nullptr;
    }
    if (terrain_body) {
        terrain_body->queue_free();
        terrain_body = nullptr;
    }
    if (water_mesh) {
        water_mesh->queue_free();
        water_mesh = nullptr;
    }
    if (trees_container) {
        trees_container->queue_free();
        trees_container = nullptr;
    }
    if (lakes_container) {
        lakes_container->queue_free();
        lakes_container = nullptr;
    }
    if (grass_instance) {
        grass_instance->queue_free();
        grass_instance = nullptr;
    }
    if (world_environment) {
        world_environment->queue_free();
        world_environment = nullptr;
    }
    if (sun_light) {
        sun_light->queue_free();
        sun_light = nullptr;
    }
    grass_multimesh.unref();
    grass_blade_mesh.unref();
    grass_material.unref();
    terrain_shader_material.unref();
    terrain_shader.unref();
    environment.unref();
    sky.unref();
    terrain_collision = nullptr;
    heightmap.clear();
    lake_positions.clear();
}

void TerrainGenerator::generate_base_heightmap() {
    int size = config.map_size;
    float half_size = size * 0.5f;
    
    // Ground level as normalized value (0-1)
    // We want ground to be at config.ground_level out of max_height
    float base_ground = config.ground_level / config.max_height;
    
    for (int z = 0; z < size; z++) {
        for (int x = 0; x < size; x++) {
            float wx = (x - half_size) * config.tile_size;
            float wz = (z - half_size) * config.tile_size;
            
            // Very subtle terrain variation - mostly flat
            float noise = fbm_noise(wx, wz, config.octaves, config.persistence, 
                                     config.lacunarity, config.base_frequency, config.seed);
            
            // Start at base ground level, add tiny variation
            float height = base_ground + noise * config.base_amplitude;
            
            // Keep height positive and reasonable
            height = Math::max(height, base_ground * 0.9f);
            
            heightmap[z * size + x] = height;
        }
    }
}

void TerrainGenerator::apply_mountains() {
    int size = config.map_size;
    float half_size = size * 0.5f;
    
    for (int z = 0; z < size; z++) {
        for (int x = 0; x < size; x++) {
            float wx = (x - half_size) * config.tile_size;
            float wz = (z - half_size) * config.tile_size;
            
            // Mountain noise layer
            float mountain_noise = fbm_noise(wx, wz, 3, 0.5f, 2.0f, 
                                             config.mountain_frequency, config.seed + 5000);
            mountain_noise = (mountain_noise + 1.0f) * 0.5f;
            
            // Only add mountains where noise is above threshold
            if (mountain_noise > config.mountain_threshold) {
                float mountain_factor = (mountain_noise - config.mountain_threshold) / (1.0f - config.mountain_threshold);
                mountain_factor = mountain_factor * mountain_factor; // Square for sharper peaks
                
                float current = heightmap[z * size + x];
                heightmap[z * size + x] = current + mountain_factor * config.mountain_amplitude;
            }
        }
    }
}

void TerrainGenerator::carve_lakes() {
    int size = config.map_size;
    float half_size = size * 0.5f;
    float half_world = half_size * config.tile_size;
    
    // Clear previous lake data
    lake_positions.clear();
    
    // Lake water level - the water surface height
    float lake_water_height = 0.8f; // Slightly below ground level
    float lake_bottom_normalized = -0.1f / config.max_height; // Deep lake bed (below 0)
    
    // Create several large, natural-looking lakes
    for (int i = 0; i < config.lake_count; i++) {
        // Pseudo-random lake center based on seed
        int hash1 = (config.seed * (i + 1) * 16807) % 2147483647;
        int hash2 = (hash1 * 16807) % 2147483647;
        int hash3 = (hash2 * 16807) % 2147483647;
        int hash4 = (hash3 * 16807) % 2147483647;
        
        float lake_x = (float)(hash1 % size);
        float lake_z = (float)(hash2 % size);
        
        // Keep lakes away from edges but allow more of the map
        lake_x = Math::clamp(lake_x, size * 0.15f, size * 0.85f);
        lake_z = Math::clamp(lake_z, size * 0.15f, size * 0.85f);
        
        // Avoid placing lakes too close to map center (player start area)
        float center_dist = sqrt(pow(lake_x - half_size, 2) + pow(lake_z - half_size, 2));
        if (center_dist < size * 0.12f) {
            // Push lake away from center
            float angle = atan2(lake_z - half_size, lake_x - half_size);
            lake_x = half_size + cos(angle) * size * 0.2f;
            lake_z = half_size + sin(angle) * size * 0.2f;
        }
        
        // Lake size - make them LARGE and natural
        float size_variation = (float)(hash3 % 100) / 100.0f;
        float lake_radius = config.lake_size + size_variation * (config.lake_max_size - config.lake_size);
        
        // Add irregularity to lake shape
        float shape_variation = 0.3f + (float)(hash4 % 100) / 100.0f * 0.4f; // 0.3 to 0.7
        
        // Convert to world coordinates and store for water plane creation
        float world_x = (lake_x - half_size) * config.tile_size;
        float world_z = (lake_z - half_size) * config.tile_size;
        
        LakeData lake;
        lake.world_x = world_x;
        lake.world_z = world_z;
        lake.radius = lake_radius * config.tile_size;
        lake.water_height = lake_water_height;
        lake_positions.push_back(lake);
        
        // Carve the lake depression with natural irregular edges
        for (int z = 0; z < size; z++) {
            for (int x = 0; x < size; x++) {
                float dx = x - lake_x;
                float dz = z - lake_z;
                float dist = sqrt(dx * dx + dz * dz);
                
                // Add noise to lake edge for natural shape
                float angle = atan2(dz, dx);
                float edge_noise = sin(angle * 5.0f + config.seed) * 0.15f + 
                                   sin(angle * 8.0f + config.seed * 2) * 0.1f +
                                   sin(angle * 13.0f + config.seed * 3) * 0.05f;
                float effective_radius = lake_radius * (1.0f + edge_noise * shape_variation);
                
                if (dist < effective_radius) {
                    // Create deep bowl shape with flat bottom
                    float edge_factor = dist / effective_radius;
                    float depth_factor;
                    
                    if (edge_factor < 0.7f) {
                        // Flat deep center (70% of lake is deep)
                        depth_factor = 1.0f;
                    } else {
                        // Smooth shore transition
                        float shore_factor = (edge_factor - 0.7f) / 0.3f;
                        depth_factor = 1.0f - (shore_factor * shore_factor);
                    }
                    
                    float current = heightmap[z * size + x];
                    // Target is well below water level for deep lake bed
                    float target = lake_bottom_normalized;
                    
                    // Blend to create the depression
                    heightmap[z * size + x] = lerp(current, target, depth_factor * 0.95f);
                }
            }
        }
    }
    
    UtilityFunctions::print("TerrainGenerator: Carved ", config.lake_count, " large lakes");
}

void TerrainGenerator::smooth_terrain(int iterations) {
    int size = config.map_size;
    PackedFloat32Array temp;
    temp.resize(size * size);
    
    for (int iter = 0; iter < iterations; iter++) {
        for (int z = 1; z < size - 1; z++) {
            for (int x = 1; x < size - 1; x++) {
                float sum = 0.0f;
                sum += heightmap[(z - 1) * size + (x - 1)];
                sum += heightmap[(z - 1) * size + x];
                sum += heightmap[(z - 1) * size + (x + 1)];
                sum += heightmap[z * size + (x - 1)];
                sum += heightmap[z * size + x] * 4.0f; // Weight center more
                sum += heightmap[z * size + (x + 1)];
                sum += heightmap[(z + 1) * size + (x - 1)];
                sum += heightmap[(z + 1) * size + x];
                sum += heightmap[(z + 1) * size + (x + 1)];
                
                temp[z * size + x] = sum / 12.0f;
            }
        }
        
        // Copy back
        for (int i = 0; i < size * size; i++) {
            if (temp[i] > 0) {
                heightmap[i] = temp[i];
            }
        }
    }
}

void TerrainGenerator::generate_normalmap() {
    int size = config.map_size;
    float half_world = (size * config.tile_size) * 0.5f;
    
    normalmap_image = Image::create(size, size, false, Image::FORMAT_RGB8);
    
    for (int z = 0; z < size; z++) {
        for (int x = 0; x < size; x++) {
            // Convert pixel to world coordinates (centered at origin)
            float wx = x * config.tile_size - half_world;
            float wz = z * config.tile_size - half_world;
            
            // Get neighboring heights using world coordinates
            float hL = get_height_at(wx - config.tile_size, wz);
            float hR = get_height_at(wx + config.tile_size, wz);
            float hD = get_height_at(wx, wz - config.tile_size);
            float hU = get_height_at(wx, wz + config.tile_size);
            
            // Calculate normal
            Vector3 normal = Vector3(hL - hR, 2.0f * config.tile_size, hD - hU).normalized();
            
            // Encode to color
            Color color((normal.x + 1.0f) * 0.5f, (normal.y + 1.0f) * 0.5f, (normal.z + 1.0f) * 0.5f);
            normalmap_image->set_pixel(x, z, color);
        }
    }
    
    normalmap_texture.instantiate();
    normalmap_texture->set_image(normalmap_image);
}

void TerrainGenerator::generate_splatmap() {
    int size = config.map_size;
    float half_world = (size * config.tile_size) * 0.5f;
    
    splatmap_image = Image::create(size, size, false, Image::FORMAT_RGBA8);
    
    for (int z = 0; z < size; z++) {
        for (int x = 0; x < size; x++) {
            float height = heightmap[z * size + x] * config.max_height;
            // Convert pixel to world coordinates
            float wx = x * config.tile_size - half_world;
            float wz = z * config.tile_size - half_world;
            Vector3 normal = get_normal_at(wx, wz);
            float slope = 1.0f - normal.y; // 0 = flat, 1 = vertical
            
            // RGBA channels: R=grass, G=dirt, B=rock, A=sand
            float grass = 0.0f, dirt = 0.0f, rock = 0.0f, sand = 0.0f;
            
            if (height <= config.water_level + 1.0f) {
                // Beach/sand near water
                sand = 1.0f;
            } else if (height > config.snow_level) {
                // Snow on high peaks (represented as white rock)
                rock = 1.0f;
            } else if (slope > 0.5f) {
                // Steep slopes get rock
                rock = smoothstep(0.5f, 0.8f, slope);
                dirt = 1.0f - rock;
            } else if (slope > 0.3f) {
                // Medium slopes get dirt
                dirt = smoothstep(0.3f, 0.5f, slope);
                grass = 1.0f - dirt;
            } else {
                // Flat areas get grass
                grass = 1.0f;
            }
            
            // Normalize
            float total = grass + dirt + rock + sand;
            if (total > 0) {
                grass /= total;
                dirt /= total;
                rock /= total;
                sand /= total;
            }
            
            splatmap_image->set_pixel(x, z, Color(grass, dirt, rock, sand));
        }
    }
    
    splatmap_texture.instantiate();
    splatmap_texture->set_image(splatmap_image);
}

void TerrainGenerator::create_terrain_mesh() {
    int size = config.map_size;
    float half_world = (size * config.tile_size) * 0.5f;
    
    Ref<SurfaceTool> st;
    st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    
    // Create vertices
    for (int z = 0; z < size; z++) {
        for (int x = 0; x < size; x++) {
            float height = heightmap[z * size + x] * config.max_height;
            float wx = x * config.tile_size - half_world;
            float wz = z * config.tile_size - half_world;
            
            // UV coordinates
            float u = (float)x / (size - 1);
            float v = (float)z / (size - 1);
            
            // Calculate normal using world coordinates (wx, wz are already world coords)
            Vector3 normal = get_normal_at(wx, wz);
            
            // Color based on height (for basic visualization)
            // Heights typically range from ~0.5 to ~7, so use appropriate thresholds
            Color color;
            if (height <= config.water_level) {
                color = Color(0.15f, 0.4f, 0.7f); // Deep water blue
            } else if (height < 1.5f) {
                // Beach/low ground - vibrant grass
                color = Color(0.35f, 0.65f, 0.2f);
            } else if (height < 3.0f) {
                // Low grass - bright vivid green
                color = Color(0.2f, 0.7f, 0.15f);
            } else if (height < 5.0f) {
                // Medium grass - lush green
                color = Color(0.15f, 0.6f, 0.1f);
            } else if (height < 7.0f) {
                // High grass - rich green
                color = Color(0.2f, 0.55f, 0.15f);
            } else if (height < 10.0f) {
                // Rocky dirt - warmer brown
                color = Color(0.55f, 0.45f, 0.3f);
            } else if (height < 14.0f) {
                // Mountain rock
                color = Color(0.5f, 0.5f, 0.5f);
            } else {
                // Snow peaks
                color = Color(0.95f, 0.95f, 0.95f);
            }
            
            st->set_color(color);
            st->set_uv(Vector2(u, v));
            st->set_normal(normal);
            st->add_vertex(Vector3(wx, height, wz));
        }
    }
    
    // Create triangles - counter-clockwise winding for front faces (looking down from +Y)
    for (int z = 0; z < size - 1; z++) {
        for (int x = 0; x < size - 1; x++) {
            int i = z * size + x;
            
            // First triangle (bottom-left triangle of quad)
            st->add_index(i);
            st->add_index(i + 1);
            st->add_index(i + size);
            
            // Second triangle (top-right triangle of quad)
            st->add_index(i + 1);
            st->add_index(i + size + 1);
            st->add_index(i + size);
        }
    }
    
    // Generate tangents for normal mapping
    st->generate_tangents();
    
    // Create mesh
    Ref<ArrayMesh> mesh = st->commit();
    
    // Create mesh instance
    terrain_mesh = memnew(MeshInstance3D);
    terrain_mesh->set_mesh(mesh);
    terrain_mesh->set_name("TerrainMesh");
    add_child(terrain_mesh);
}

void TerrainGenerator::create_terrain_collision() {
    int size = config.map_size;
    float half_world = (size * config.tile_size) * 0.5f;
    
    // Create heightmap shape
    Ref<HeightMapShape3D> shape;
    shape.instantiate();
    shape->set_map_width(size);
    shape->set_map_depth(size);
    
    // Scale heights for collision
    PackedFloat32Array collision_heights;
    collision_heights.resize(size * size);
    
    for (int i = 0; i < size * size; i++) {
        collision_heights[i] = heightmap[i] * config.max_height;
    }
    
    shape->set_map_data(collision_heights);
    
    // Create static body
    terrain_body = memnew(StaticBody3D);
    terrain_body->set_name("TerrainCollision");
    terrain_body->set_collision_layer(1); // Ground layer
    terrain_body->set_collision_mask(0);
    add_child(terrain_body);
    
    // Create collision shape
    terrain_collision = memnew(CollisionShape3D);
    terrain_collision->set_shape(shape);
    
    // HeightMapShape3D is centered at origin and spans from -width/2 to +width/2
    // We need to scale it to match our tile_size, and it's already centered
    float scale_factor = config.tile_size;
    terrain_collision->set_scale(Vector3(scale_factor, 1.0f, scale_factor));
    // The HeightMapShape3D center aligns with our mesh which also uses -half_world to +half_world
    terrain_collision->set_position(Vector3(0, 0, 0));
    
    terrain_body->add_child(terrain_collision);
}

void TerrainGenerator::create_water_plane() {
    // Create container for lake water planes
    lakes_container = memnew(Node3D);
    lakes_container->set_name("Lakes");
    add_child(lakes_container);
    
    // Try to load water shader
    Ref<ShaderMaterial> water_shader_mat;
    ResourceLoader *loader = ResourceLoader::get_singleton();
    String water_shader_path = "res://shaders/water.gdshader";
    
    if (loader && FileAccess::file_exists(water_shader_path)) {
        Ref<Shader> water_shader = loader->load(water_shader_path);
        if (water_shader.is_valid()) {
            water_shader_mat.instantiate();
            water_shader_mat->set_shader(water_shader);
            
            // Realistic water colors
            water_shader_mat->set_shader_parameter("water_color_shallow", Color(0.1f, 0.35f, 0.5f));
            water_shader_mat->set_shader_parameter("water_color_deep", Color(0.02f, 0.1f, 0.2f));
            water_shader_mat->set_shader_parameter("water_color_fresnel", Color(0.12f, 0.3f, 0.42f));
            
            // Gerstner wave parameters - gentle lake waves
            water_shader_mat->set_shader_parameter("wave_speed", 0.6f);
            water_shader_mat->set_shader_parameter("wave_scale", 0.015f);
            
            // Wave 1 - Large gentle swell
            water_shader_mat->set_shader_parameter("wave1_amplitude", 0.25f);
            water_shader_mat->set_shader_parameter("wave1_frequency", 0.08f);
            water_shader_mat->set_shader_parameter("wave1_steepness", 0.4f);
            water_shader_mat->set_shader_parameter("wave1_direction", Vector2(1.0f, 0.2f));
            
            // Wave 2 - Medium waves
            water_shader_mat->set_shader_parameter("wave2_amplitude", 0.15f);
            water_shader_mat->set_shader_parameter("wave2_frequency", 0.15f);
            water_shader_mat->set_shader_parameter("wave2_steepness", 0.35f);
            water_shader_mat->set_shader_parameter("wave2_direction", Vector2(0.6f, 0.8f));
            
            // Wave 3 - Small choppy waves
            water_shader_mat->set_shader_parameter("wave3_amplitude", 0.08f);
            water_shader_mat->set_shader_parameter("wave3_frequency", 0.3f);
            water_shader_mat->set_shader_parameter("wave3_steepness", 0.3f);
            water_shader_mat->set_shader_parameter("wave3_direction", Vector2(-0.3f, 0.95f));
            
            // Wave 4 - Micro ripples
            water_shader_mat->set_shader_parameter("wave4_amplitude", 0.04f);
            water_shader_mat->set_shader_parameter("wave4_frequency", 0.6f);
            water_shader_mat->set_shader_parameter("wave4_steepness", 0.2f);
            water_shader_mat->set_shader_parameter("wave4_direction", Vector2(0.85f, -0.5f));
            
            // Surface properties
            water_shader_mat->set_shader_parameter("roughness", 0.04f);
            water_shader_mat->set_shader_parameter("metallic", 0.1f);
            water_shader_mat->set_shader_parameter("opacity", 0.96f);
            
            // Fresnel
            water_shader_mat->set_shader_parameter("fresnel_power", 4.5f);
            water_shader_mat->set_shader_parameter("fresnel_bias", 0.03f);
            
            // Reflection
            water_shader_mat->set_shader_parameter("reflection_strength", 0.75f);
            water_shader_mat->set_shader_parameter("sky_color", Color(0.5f, 0.7f, 0.95f));
            
            // Foam (subtle for lakes)
            water_shader_mat->set_shader_parameter("foam_amount", 0.15f);
            water_shader_mat->set_shader_parameter("foam_cutoff", 0.8f);
            water_shader_mat->set_shader_parameter("foam_color", Color(0.95f, 0.98f, 1.0f));
            
            // Subsurface scattering
            water_shader_mat->set_shader_parameter("sss_strength", 0.35f);
            water_shader_mat->set_shader_parameter("sss_color", Color(0.08f, 0.45f, 0.35f));
            
            // Caustics
            water_shader_mat->set_shader_parameter("caustic_strength", 0.12f);
            water_shader_mat->set_shader_parameter("caustic_scale", 0.025f);
            
            // Try to load water normal map textures if available
            String normal1_path = "res://assets/textures/water_normal_1.png";
            String normal2_path = "res://assets/textures/water_normal_2.png";
            
            if (FileAccess::file_exists(normal1_path)) {
                Ref<Texture2D> water_normal1 = loader->load(normal1_path);
                if (water_normal1.is_valid()) {
                    water_shader_mat->set_shader_parameter("normal_map_1", water_normal1);
                    UtilityFunctions::print("TerrainGenerator: Water normal map 1 loaded");
                }
            }
            
            if (FileAccess::file_exists(normal2_path)) {
                Ref<Texture2D> water_normal2 = loader->load(normal2_path);
                if (water_normal2.is_valid()) {
                    water_shader_mat->set_shader_parameter("normal_map_2", water_normal2);
                    UtilityFunctions::print("TerrainGenerator: Water normal map 2 loaded");
                }
            }
            
            water_shader_mat->set_shader_parameter("normal_map_scale", 0.04f);
            water_shader_mat->set_shader_parameter("normal_map_strength", 0.5f);
            
            UtilityFunctions::print("TerrainGenerator: Realistic Gerstner wave water shader loaded");
        }
    }
    
    // Fallback material if shader not available
    Ref<StandardMaterial3D> water_mat_fallback;
    if (!water_shader_mat.is_valid()) {
        water_mat_fallback.instantiate();
        water_mat_fallback->set_albedo(Color(0.05f, 0.15f, 0.25f, 0.95f));
        water_mat_fallback->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
        water_mat_fallback->set_roughness(0.02f);
        water_mat_fallback->set_metallic(0.6f);
        water_mat_fallback->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    }
    
    // Create an irregular circular water mesh for each lake
    for (size_t i = 0; i < lake_positions.size(); i++) {
        const LakeData &lake = lake_positions[i];
        
        // Generate irregular circular mesh
        Ref<ArrayMesh> lake_mesh_data = create_irregular_lake_mesh(
            lake.radius, 
            64,  // Number of radial segments
            24,  // Number of rings
            config.seed + (int)i * 1000
        );
        
        MeshInstance3D *lake_mesh = memnew(MeshInstance3D);
        lake_mesh->set_mesh(lake_mesh_data);
        
        if (water_shader_mat.is_valid()) {
            lake_mesh->set_surface_override_material(0, water_shader_mat);
        } else {
            lake_mesh->set_surface_override_material(0, water_mat_fallback);
        }
        
        lake_mesh->set_position(Vector3(lake.world_x, lake.water_height + 0.05f, lake.world_z));
        lake_mesh->set_name(String("Lake_") + String::num_int64(i));
        
        lakes_container->add_child(lake_mesh);
    }
    
    UtilityFunctions::print("TerrainGenerator: Created ", lake_positions.size(), " irregular lake meshes");
}

// Creates an irregular circular mesh for natural-looking lake shapes
Ref<ArrayMesh> TerrainGenerator::create_irregular_lake_mesh(float radius, int radial_segments, int rings, int seed) {
    // Generate irregular lake mesh with noise-based edge variation
    PackedVector3Array vertices;
    PackedVector3Array normals;
    PackedVector2Array uvs;
    PackedInt32Array indices;
    
    // Reserve space
    int total_vertices = 1 + radial_segments * rings; // center + rings
    vertices.resize(total_vertices);
    normals.resize(total_vertices);
    uvs.resize(total_vertices);
    
    // Center vertex
    vertices[0] = Vector3(0, 0, 0);
    normals[0] = Vector3(0, 1, 0);
    uvs[0] = Vector2(0.5f, 0.5f);
    
    // Generate ring vertices with noise-based edge variation
    int vertex_idx = 1;
    for (int ring = 0; ring < rings; ring++) {
        float ring_ratio = (float)(ring + 1) / (float)rings;
        float base_ring_radius = radius * ring_ratio;
        
        for (int seg = 0; seg < radial_segments; seg++) {
            float angle = (float)seg / (float)radial_segments * Math_PI * 2.0f;
            
            // Add noise variation for outer rings (creates irregular shoreline)
            float noise_strength = 0.0f;
            if (ring >= rings - 4) {  // Only affect outer 4 rings
                float outer_ratio = (float)(ring - (rings - 4)) / 4.0f;
                noise_strength = 0.15f * outer_ratio;  // Up to 15% variation
            }
            
            // Multi-frequency noise for natural shoreline
            float noise_x = Math::cos(angle) * 3.0f + seed * 0.1f;
            float noise_z = Math::sin(angle) * 3.0f + seed * 0.1f;
            float edge_noise = noise2d(noise_x, noise_z, seed);
            edge_noise += 0.5f * noise2d(noise_x * 2.3f, noise_z * 2.3f, seed + 1);
            edge_noise += 0.25f * noise2d(noise_x * 5.1f, noise_z * 5.1f, seed + 2);
            edge_noise = (edge_noise / 1.75f) * 2.0f - 1.0f;  // Normalize to -1 to 1
            
            float radius_variation = 1.0f + edge_noise * noise_strength;
            float final_radius = base_ring_radius * radius_variation;
            
            // For the outermost ring, add extra "bay" and "peninsula" features
            if (ring == rings - 1) {
                // Add larger features (bays and peninsulas)
                float feature_noise = noise2d(noise_x * 0.5f, noise_z * 0.5f, seed + 3);
                feature_noise = (feature_noise * 2.0f - 1.0f) * 0.2f;  // ±20% variation
                final_radius *= (1.0f + feature_noise);
            }
            
            float x = Math::cos(angle) * final_radius;
            float z = Math::sin(angle) * final_radius;
            
            vertices[vertex_idx] = Vector3(x, 0, z);
            normals[vertex_idx] = Vector3(0, 1, 0);
            
            // UV coordinates for texture mapping (circular mapping)
            float u = 0.5f + Math::cos(angle) * ring_ratio * 0.5f;
            float v = 0.5f + Math::sin(angle) * ring_ratio * 0.5f;
            uvs[vertex_idx] = Vector2(u, v);
            
            vertex_idx++;
        }
    }
    
    // Generate triangles
    // Inner fan (center to first ring)
    for (int seg = 0; seg < radial_segments; seg++) {
        int next_seg = (seg + 1) % radial_segments;
        indices.push_back(0);  // Center
        indices.push_back(1 + seg);
        indices.push_back(1 + next_seg);
    }
    
    // Ring strips
    for (int ring = 0; ring < rings - 1; ring++) {
        int ring_start = 1 + ring * radial_segments;
        int next_ring_start = ring_start + radial_segments;
        
        for (int seg = 0; seg < radial_segments; seg++) {
            int next_seg = (seg + 1) % radial_segments;
            
            int v0 = ring_start + seg;
            int v1 = ring_start + next_seg;
            int v2 = next_ring_start + seg;
            int v3 = next_ring_start + next_seg;
            
            // Two triangles per quad
            indices.push_back(v0);
            indices.push_back(v2);
            indices.push_back(v1);
            
            indices.push_back(v1);
            indices.push_back(v2);
            indices.push_back(v3);
        }
    }
    
    // Create the mesh
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_NORMAL] = normals;
    arrays[Mesh::ARRAY_TEX_UV] = uvs;
    arrays[Mesh::ARRAY_INDEX] = indices;
    
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    
    return mesh;
}

void TerrainGenerator::apply_terrain_material() {
    if (!terrain_mesh) return;
    
    // Use shader material if available, otherwise fall back to vertex colors
    if (terrain_shader_material.is_valid()) {
        terrain_mesh->set_surface_override_material(0, terrain_shader_material);
        UtilityFunctions::print("TerrainGenerator: Applied shader material to terrain");
    } else {
        // Create a basic material with vertex colors as fallback
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        mat->set_roughness(0.9f);
        mat->set_metallic(0.0f);
        mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
        terrain_mesh->set_surface_override_material(0, mat);
        UtilityFunctions::print("TerrainGenerator: Applied fallback vertex color material");
    }
}

float TerrainGenerator::get_height_at(float x, float z) const {
    if (heightmap.is_empty()) {
        UtilityFunctions::print("TerrainGenerator::get_height_at: heightmap is EMPTY! Returning 0");
        return 0.0f;
    }
    
    int size = config.map_size;
    float half_world = (size * config.tile_size) * 0.5f;
    
    // Convert world coords to heightmap coords
    float hx = (x + half_world) / config.tile_size;
    float hz = (z + half_world) / config.tile_size;
    
    // Clamp to valid range
    hx = Math::clamp(hx, 0.0f, (float)(size - 1));
    hz = Math::clamp(hz, 0.0f, (float)(size - 1));
    
    // Bilinear interpolation
    int x0 = (int)floor(hx);
    int z0 = (int)floor(hz);
    int x1 = Math::min(x0 + 1, size - 1);
    int z1 = Math::min(z0 + 1, size - 1);
    
    float fx = hx - x0;
    float fz = hz - z0;
    
    float h00 = heightmap[z0 * size + x0];
    float h10 = heightmap[z0 * size + x1];
    float h01 = heightmap[z1 * size + x0];
    float h11 = heightmap[z1 * size + x1];
    
    float h0 = h00 + fx * (h10 - h00);
    float h1 = h01 + fx * (h11 - h01);
    
    return (h0 + fz * (h1 - h0)) * config.max_height;
}

Vector3 TerrainGenerator::get_normal_at(float x, float z) const {
    float delta = config.tile_size;
    
    float hL = get_height_at(x - delta, z);
    float hR = get_height_at(x + delta, z);
    float hD = get_height_at(x, z - delta);
    float hU = get_height_at(x, z + delta);
    
    return Vector3(hL - hR, 2.0f * delta, hD - hU).normalized();
}

bool TerrainGenerator::is_water_at(float x, float z) const {
    return get_height_at(x, z) <= config.water_level;
}

bool TerrainGenerator::is_buildable_at(float x, float z) const {
    // Check bounds first
    if (!is_within_bounds(x, z)) return false;
    
    float height = get_height_at(x, z);
    
    // Not buildable in water
    if (height <= config.water_level + 0.5f) return false;
    
    // Check slope
    Vector3 normal = get_normal_at(x, z);
    float slope = 1.0f - normal.y;
    
    // Too steep to build
    if (slope > 0.3f) return false;
    
    return true;
}

bool TerrainGenerator::is_within_bounds(float x, float z) const {
    float half_world = (config.map_size * config.tile_size) * 0.5f;
    return (x >= -half_world && x <= half_world && z >= -half_world && z <= half_world);
}

// Setters and getters
void TerrainGenerator::set_map_size(int size) {
    config.map_size = Math::clamp(size, 32, 512);
}

int TerrainGenerator::get_map_size() const {
    return config.map_size;
}

void TerrainGenerator::set_tile_size(float size) {
    config.tile_size = Math::clamp(size, 0.5f, 4.0f);
}

float TerrainGenerator::get_tile_size() const {
    return config.tile_size;
}

void TerrainGenerator::set_max_height(float height) {
    config.max_height = Math::clamp(height, 5.0f, 100.0f);
}

float TerrainGenerator::get_max_height() const {
    return config.max_height;
}

void TerrainGenerator::set_water_level(float level) {
    config.water_level = Math::clamp(level, -20.0f, 20.0f);
}

float TerrainGenerator::get_water_level() const {
    return config.water_level;
}

void TerrainGenerator::set_seed(int seed) {
    config.seed = seed;
}

int TerrainGenerator::get_seed() const {
    return config.seed;
}

void TerrainGenerator::set_mountain_frequency(float freq) {
    config.mountain_frequency = Math::clamp(freq, 0.001f, 0.1f);
}

float TerrainGenerator::get_mountain_frequency() const {
    return config.mountain_frequency;
}

void TerrainGenerator::set_lake_count(int count) {
    config.lake_count = Math::clamp(count, 0, 10);
}

int TerrainGenerator::get_lake_count() const {
    return config.lake_count;
}

float TerrainGenerator::get_world_size() const {
    return config.map_size * config.tile_size;
}

Vector3 TerrainGenerator::get_world_center() const {
    return Vector3(0, 0, 0);
}

void TerrainGenerator::create_tree_mesh() {
    // Create shared materials for all trees
    tree_trunk_material.instantiate();
    tree_trunk_material->set_albedo(Color(0.4f, 0.25f, 0.15f)); // Brown trunk
    tree_trunk_material->set_roughness(0.9f);
    
    tree_leaves_material.instantiate();
    tree_leaves_material->set_albedo(Color(0.15f, 0.45f, 0.12f)); // Dark green leaves
    tree_leaves_material->set_roughness(0.8f);
}

bool TerrainGenerator::is_valid_tree_position(float x, float z) const {
    // Check if within bounds
    if (!is_within_bounds(x, z)) return false;
    
    // Check height - trees don't grow in water or on peaks
    float height = get_height_at(x, z);
    if (height < config.tree_min_height || height > config.tree_max_height) return false;
    
    // Check slope - trees don't grow on steep terrain
    Vector3 normal = get_normal_at(x, z);
    float slope = 1.0f - normal.y;
    if (slope > config.tree_max_slope) return false;
    
    // Keep center area clear for player base
    float dist_from_center = sqrt(x * x + z * z);
    if (dist_from_center < config.tree_center_clear) return false;
    
    return true;
}

void TerrainGenerator::generate_trees() {
    UtilityFunctions::print("TerrainGenerator: Generating trees...");
    
    // Create materials
    create_tree_mesh();
    
    // Create container for all trees
    trees_container = memnew(Node3D);
    trees_container->set_name("Trees");
    add_child(trees_container);
    
    // === PINE TREE MESHES ===
    // Trunk for pine trees (tall and thin)
    Ref<CylinderMesh> pine_trunk_mesh;
    pine_trunk_mesh.instantiate();
    pine_trunk_mesh->set_top_radius(0.12f);
    pine_trunk_mesh->set_bottom_radius(0.22f);
    pine_trunk_mesh->set_height(3.5f);
    pine_trunk_mesh->set_radial_segments(6);
    
    // Multiple cone layers for pine trees (bottom, middle, top)
    Ref<CylinderMesh> pine_cone_bottom;
    pine_cone_bottom.instantiate();
    pine_cone_bottom->set_top_radius(0.0f);
    pine_cone_bottom->set_bottom_radius(1.8f);
    pine_cone_bottom->set_height(2.5f);
    pine_cone_bottom->set_radial_segments(8);
    
    Ref<CylinderMesh> pine_cone_middle;
    pine_cone_middle.instantiate();
    pine_cone_middle->set_top_radius(0.0f);
    pine_cone_middle->set_bottom_radius(1.4f);
    pine_cone_middle->set_height(2.0f);
    pine_cone_middle->set_radial_segments(8);
    
    Ref<CylinderMesh> pine_cone_top;
    pine_cone_top.instantiate();
    pine_cone_top->set_top_radius(0.0f);
    pine_cone_top->set_bottom_radius(0.9f);
    pine_cone_top->set_height(1.5f);
    pine_cone_top->set_radial_segments(8);
    
    // === DECIDUOUS TREE MESHES ===
    // Trunk for deciduous trees (shorter, thicker)
    Ref<CylinderMesh> decid_trunk_mesh;
    decid_trunk_mesh.instantiate();
    decid_trunk_mesh->set_top_radius(0.18f);
    decid_trunk_mesh->set_bottom_radius(0.35f);
    decid_trunk_mesh->set_height(2.5f);
    decid_trunk_mesh->set_radial_segments(8);
    
    // Spherical canopy for deciduous (using capsule for rounded look)
    Ref<CapsuleMesh> decid_canopy;
    decid_canopy.instantiate();
    decid_canopy->set_radius(1.5f);
    decid_canopy->set_height(3.0f);
    decid_canopy->set_radial_segments(12);
    decid_canopy->set_rings(6);
    
    // Additional smaller spheres for fuller canopy
    Ref<CapsuleMesh> decid_canopy_small;
    decid_canopy_small.instantiate();
    decid_canopy_small->set_radius(0.9f);
    decid_canopy_small->set_height(1.8f);
    decid_canopy_small->set_radial_segments(8);
    decid_canopy_small->set_rings(4);
    
    // Create darker green material for pine trees - rich forest green
    Ref<StandardMaterial3D> pine_leaves_material;
    pine_leaves_material.instantiate();
    pine_leaves_material->set_albedo(Color(0.08f, 0.35f, 0.12f)); // Forest green
    pine_leaves_material->set_roughness(0.9f);
    pine_leaves_material->set_metallic(0.0f);
    
    // Create lighter green material for deciduous trees - vibrant
    Ref<StandardMaterial3D> decid_leaves_material;
    decid_leaves_material.instantiate();
    decid_leaves_material->set_albedo(Color(0.12f, 0.55f, 0.15f)); // Vibrant green
    decid_leaves_material->set_roughness(0.85f);
    decid_leaves_material->set_metallic(0.0f);
    
    float half_world = (config.map_size * config.tile_size) * 0.5f;
    
    // Store placed tree positions for spacing check
    std::vector<Vector2> placed_trees;
    placed_trees.reserve(config.tree_count);
    
    int attempts = 0;
    int max_attempts = config.tree_count * 10; // Prevent infinite loop
    int trees_placed = 0;
    int pine_count = 0;
    int decid_count = 0;
    
    while (trees_placed < config.tree_count && attempts < max_attempts) {
        attempts++;
        
        // Random position using seed
        int hash = (config.seed + attempts * 16807) % 2147483647;
        int hash2 = (hash * 48271) % 2147483647;
        
        float x = ((float)(hash % 10000) / 10000.0f) * half_world * 2.0f - half_world;
        float z = ((float)(hash2 % 10000) / 10000.0f) * half_world * 2.0f - half_world;
        
        // Check if valid position
        if (!is_valid_tree_position(x, z)) continue;
        
        // Check spacing from other trees
        bool too_close = false;
        Vector2 new_pos(x, z);
        for (const Vector2 &pos : placed_trees) {
            if (pos.distance_to(new_pos) < config.tree_min_spacing) {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;
        
        // Get terrain height at this position
        float terrain_height = get_height_at(x, z);
        
        // Random tree size variation
        int hash3 = (hash2 * 69621) % 2147483647;
        float scale = 0.7f + ((float)(hash3 % 1000) / 1000.0f) * 0.6f; // 0.7 to 1.3
        
        // Random rotation
        float rotation = ((float)(hash3 % 360));
        
        // Determine tree type based on height and randomness
        // Pine trees prefer higher elevations, deciduous prefer lower
        int hash4 = (hash3 * 45678) % 2147483647;
        float type_rand = (float)(hash4 % 100) / 100.0f;
        float height_factor = (terrain_height - config.water_level) / (config.ground_level - config.water_level);
        height_factor = Math::clamp(height_factor, 0.0f, 1.0f);
        
        // Higher terrain = more likely to be pine
        bool is_pine = type_rand < (0.3f + height_factor * 0.5f);
        
        // Create tree node
        Node3D *tree = memnew(Node3D);
        tree->set_position(Vector3(x, terrain_height, z));
        tree->set_rotation_degrees(Vector3(0, rotation, 0));
        tree->set_scale(Vector3(scale, scale, scale));
        
        if (is_pine) {
            // === PINE TREE ===
            pine_count++;
            
            // Add trunk
            MeshInstance3D *trunk = memnew(MeshInstance3D);
            trunk->set_mesh(pine_trunk_mesh);
            trunk->set_position(Vector3(0, 1.75f, 0));
            trunk->set_surface_override_material(0, tree_trunk_material);
            tree->add_child(trunk);
            
            // Add bottom cone layer
            MeshInstance3D *cone1 = memnew(MeshInstance3D);
            cone1->set_mesh(pine_cone_bottom);
            cone1->set_position(Vector3(0, 3.0f, 0));
            cone1->set_surface_override_material(0, pine_leaves_material);
            tree->add_child(cone1);
            
            // Add middle cone layer
            MeshInstance3D *cone2 = memnew(MeshInstance3D);
            cone2->set_mesh(pine_cone_middle);
            cone2->set_position(Vector3(0, 4.5f, 0));
            cone2->set_surface_override_material(0, pine_leaves_material);
            tree->add_child(cone2);
            
            // Add top cone layer
            MeshInstance3D *cone3 = memnew(MeshInstance3D);
            cone3->set_mesh(pine_cone_top);
            cone3->set_position(Vector3(0, 5.8f, 0));
            cone3->set_surface_override_material(0, pine_leaves_material);
            tree->add_child(cone3);
            
        } else {
            // === DECIDUOUS TREE ===
            decid_count++;
            
            // Add trunk
            MeshInstance3D *trunk = memnew(MeshInstance3D);
            trunk->set_mesh(decid_trunk_mesh);
            trunk->set_position(Vector3(0, 1.25f, 0));
            trunk->set_surface_override_material(0, tree_trunk_material);
            tree->add_child(trunk);
            
            // Add main canopy
            MeshInstance3D *canopy = memnew(MeshInstance3D);
            canopy->set_mesh(decid_canopy);
            canopy->set_position(Vector3(0, 3.8f, 0));
            canopy->set_surface_override_material(0, decid_leaves_material);
            tree->add_child(canopy);
            
            // Add secondary canopy lumps for fuller look
            int hash5 = (hash4 * 12345) % 2147483647;
            int lump_count = 2 + (hash5 % 3); // 2-4 extra lumps
            
            for (int i = 0; i < lump_count; i++) {
                int lump_hash = hash5 + i * 9876;
                float lump_angle = ((float)(lump_hash % 360)) * Math_PI / 180.0f;
                float lump_dist = 0.6f + ((float)((lump_hash >> 8) % 100) / 100.0f) * 0.5f;
                float lump_height = 3.2f + ((float)((lump_hash >> 16) % 100) / 100.0f) * 1.2f;
                
                MeshInstance3D *lump = memnew(MeshInstance3D);
                lump->set_mesh(decid_canopy_small);
                lump->set_position(Vector3(
                    cos(lump_angle) * lump_dist,
                    lump_height,
                    sin(lump_angle) * lump_dist
                ));
                lump->set_surface_override_material(0, decid_leaves_material);
                tree->add_child(lump);
            }
        }
        
        trees_container->add_child(tree);
        placed_trees.push_back(new_pos);
        trees_placed++;
    }
    
    UtilityFunctions::print("TerrainGenerator: Placed ", trees_placed, " trees (", pine_count, " pine, ", decid_count, " deciduous)");
}

void TerrainGenerator::generate_procedural_textures() {
    UtilityFunctions::print("TerrainGenerator: Generating procedural textures...");
    
    // Create noise-based textures for terrain
    int tex_size = 512;
    
    // Grass texture - vibrant greens
    grass_texture = create_noise_texture(tex_size, Color(0.15f, 0.5f, 0.1f), Color(0.25f, 0.65f, 0.15f), 32.0f, config.seed);
    
    // Sand texture - warm golden sand
    sand_texture = create_noise_texture(tex_size, Color(0.85f, 0.75f, 0.5f), Color(0.95f, 0.85f, 0.6f), 24.0f, config.seed + 1);
    
    // Dirt texture - rich brown
    dirt_texture = create_noise_texture(tex_size, Color(0.45f, 0.32f, 0.18f), Color(0.55f, 0.4f, 0.25f), 28.0f, config.seed + 2);
    
    // Rock texture - natural grey with some warmth
    rock_texture = create_noise_texture(tex_size, Color(0.45f, 0.43f, 0.4f), Color(0.6f, 0.58f, 0.55f), 16.0f, config.seed + 3);
    
    // Generate normal maps from the textures
    grass_normal_texture = create_normal_from_height(grass_texture, 1.0f);
    sand_normal_texture = create_normal_from_height(sand_texture, 0.5f);
    dirt_normal_texture = create_normal_from_height(dirt_texture, 0.8f);
    rock_normal_texture = create_normal_from_height(rock_texture, 1.5f);
    
    UtilityFunctions::print("TerrainGenerator: Procedural textures created");
}

Ref<ImageTexture> TerrainGenerator::create_noise_texture(int size, Color base_color, Color variation_color, float frequency, int seed) {
    Ref<Image> image = Image::create(size, size, false, Image::FORMAT_RGB8);
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            // Generate noise using simple hash-based approach
            int hash = (seed + x * 374761393 + y * 668265263) % 2147483647;
            hash = (hash ^ (hash >> 13)) * 1274126177;
            hash = hash ^ (hash >> 16);
            
            // Multi-octave noise
            float noise = 0.0f;
            float amplitude = 1.0f;
            float freq = frequency;
            float max_amp = 0.0f;
            
            for (int oct = 0; oct < 4; oct++) {
                int hx = (int)(x * freq / size) + seed + oct * 1000;
                int hy = (int)(y * freq / size) + seed + oct * 2000;
                int h = (hx * 374761393 + hy * 668265263) % 2147483647;
                h = (h ^ (h >> 13)) * 1274126177;
                h = h ^ (h >> 16);
                
                float n = ((float)(h % 10000) / 10000.0f);
                noise += n * amplitude;
                max_amp += amplitude;
                amplitude *= 0.5f;
                freq *= 2.0f;
            }
            noise /= max_amp;
            
            // Blend between base and variation color
            Color pixel = base_color.lerp(variation_color, noise);
            image->set_pixel(x, y, pixel);
        }
    }
    
    Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
    return texture;
}

Ref<ImageTexture> TerrainGenerator::create_normal_from_height(Ref<ImageTexture> height_tex, float strength) {
    if (!height_tex.is_valid()) {
        Ref<ImageTexture> empty;
        return empty;
    }
    
    Ref<Image> height_image = height_tex->get_image();
    if (!height_image.is_valid()) {
        Ref<ImageTexture> empty;
        return empty;
    }
    
    int width = height_image->get_width();
    int height = height_image->get_height();
    
    Ref<Image> normal_image = Image::create(width, height, false, Image::FORMAT_RGB8);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Sample neighboring pixels (wrap around)
            int xL = (x - 1 + width) % width;
            int xR = (x + 1) % width;
            int yU = (y - 1 + height) % height;
            int yD = (y + 1) % height;
            
            // Get heights (using luminance as height)
            float hL = height_image->get_pixel(xL, y).get_luminance();
            float hR = height_image->get_pixel(xR, y).get_luminance();
            float hU = height_image->get_pixel(x, yU).get_luminance();
            float hD = height_image->get_pixel(x, yD).get_luminance();
            
            // Calculate normal
            float dx = (hL - hR) * strength;
            float dy = (hU - hD) * strength;
            
            Vector3 normal = Vector3(dx, dy, 1.0f).normalized();
            
            // Convert to color (0-1 range, with 0.5 being center)
            Color normal_color = Color(
                normal.x * 0.5f + 0.5f,
                normal.y * 0.5f + 0.5f,
                normal.z * 0.5f + 0.5f
            );
            
            normal_image->set_pixel(x, y, normal_color);
        }
    }
    
    Ref<ImageTexture> texture = ImageTexture::create_from_image(normal_image);
    return texture;
}

void TerrainGenerator::setup_terrain_shader() {
    UtilityFunctions::print("TerrainGenerator: Setting up terrain shader...");
    
    // Check if shader file exists
    String shader_path = "res://shaders/terrain.gdshader";
    if (!FileAccess::file_exists(shader_path)) {
        UtilityFunctions::print("TerrainGenerator: Shader file not found at ", shader_path);
        return;
    }
    
    // Try to load the shader
    ResourceLoader *loader = ResourceLoader::get_singleton();
    if (!loader) {
        UtilityFunctions::print("TerrainGenerator: ResourceLoader not available");
        return;
    }
    
    terrain_shader = loader->load(shader_path);
    if (!terrain_shader.is_valid()) {
        UtilityFunctions::print("TerrainGenerator: Failed to load terrain shader");
        return;
    }
    
    // Create shader material
    terrain_shader_material.instantiate();
    terrain_shader_material->set_shader(terrain_shader);
    
    // Set shader parameters
    float world_size = config.map_size * config.tile_size;
    terrain_shader_material->set_shader_parameter("terrain_size", world_size);
    terrain_shader_material->set_shader_parameter("uv_scale", 0.05f);
    terrain_shader_material->set_shader_parameter("blend_sharpness", 4.0f);
    terrain_shader_material->set_shader_parameter("grass_height_max", 8.0f);
    terrain_shader_material->set_shader_parameter("sand_height_max", 4.0f);
    terrain_shader_material->set_shader_parameter("rock_slope_min", 0.6f);
    terrain_shader_material->set_shader_parameter("water_level", config.water_level);
    
    // Set textures if available
    if (grass_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("grass_albedo", grass_texture);
    }
    if (sand_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("sand_albedo", sand_texture);
    }
    if (dirt_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("dirt_albedo", dirt_texture);
    }
    if (rock_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("rock_albedo", rock_texture);
    }
    
    // Set normal maps if available
    if (grass_normal_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("grass_normal", grass_normal_texture);
    }
    if (sand_normal_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("sand_normal", sand_normal_texture);
    }
    if (dirt_normal_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("dirt_normal", dirt_normal_texture);
    }
    if (rock_normal_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("rock_normal", rock_normal_texture);
    }
    
    UtilityFunctions::print("TerrainGenerator: Terrain shader setup complete");
}

Ref<ArrayMesh> TerrainGenerator::create_grass_blade_mesh() {
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    
    // Create a simple grass blade shape (triangle fan)
    PackedVector3Array vertices;
    PackedVector3Array normals;
    PackedVector2Array uvs;
    PackedInt32Array indices;
    
    // Grass blade dimensions
    float blade_width = 0.08f;
    float blade_height = 0.6f;
    
    // Bottom left
    vertices.push_back(Vector3(-blade_width * 0.5f, 0.0f, 0.0f));
    normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
    uvs.push_back(Vector2(0.0f, 1.0f));
    
    // Bottom right
    vertices.push_back(Vector3(blade_width * 0.5f, 0.0f, 0.0f));
    normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
    uvs.push_back(Vector2(1.0f, 1.0f));
    
    // Middle left
    vertices.push_back(Vector3(-blade_width * 0.35f, blade_height * 0.5f, 0.02f));
    normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
    uvs.push_back(Vector2(0.15f, 0.5f));
    
    // Middle right
    vertices.push_back(Vector3(blade_width * 0.35f, blade_height * 0.5f, 0.02f));
    normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
    uvs.push_back(Vector2(0.85f, 0.5f));
    
    // Top (pointed tip)
    vertices.push_back(Vector3(0.0f, blade_height, 0.05f));
    normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
    uvs.push_back(Vector2(0.5f, 0.0f));
    
    // Indices for two triangles (front face)
    // Bottom triangle
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(2);
    
    // Top triangle
    indices.push_back(2);
    indices.push_back(3);
    indices.push_back(4);
    
    // Back faces (flip winding)
    indices.push_back(2);
    indices.push_back(1);
    indices.push_back(0);
    
    indices.push_back(2);
    indices.push_back(3);
    indices.push_back(1);
    
    indices.push_back(4);
    indices.push_back(3);
    indices.push_back(2);
    
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_NORMAL] = normals;
    arrays[Mesh::ARRAY_TEX_UV] = uvs;
    arrays[Mesh::ARRAY_INDEX] = indices;
    
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    
    return mesh;
}

bool TerrainGenerator::is_valid_grass_position(float x, float z) const {
    // Check bounds
    if (!is_within_bounds(x, z)) return false;
    
    // Get height
    float height = get_height_at(x, z);
    
    // No grass in water
    if (height <= config.water_level + 0.3f) return false;
    
    // No grass on very high mountains
    if (height > config.ground_level + 15.0f) return false;
    
    // Check slope - no grass on steep slopes
    Vector3 normal = get_normal_at(x, z);
    float slope = 1.0f - normal.y;
    if (slope > 0.4f) return false;
    
    return true;
}

void TerrainGenerator::generate_grass() {
    UtilityFunctions::print("TerrainGenerator: Generating grass...");
    
    // Create grass blade mesh
    grass_blade_mesh = create_grass_blade_mesh();
    
    // Create grass material with shader
    grass_material.instantiate();
    
    // Try to load grass shader
    ResourceLoader *loader = ResourceLoader::get_singleton();
    String grass_shader_path = "res://shaders/grass.gdshader";
    
    if (loader && FileAccess::file_exists(grass_shader_path)) {
        Ref<Shader> grass_shader = loader->load(grass_shader_path);
        if (grass_shader.is_valid()) {
            grass_material->set_shader(grass_shader);
            grass_material->set_shader_parameter("grass_color_base", Color(0.1f, 0.4f, 0.08f));
            grass_material->set_shader_parameter("grass_color_tip", Color(0.3f, 0.65f, 0.2f));
            grass_material->set_shader_parameter("wind_strength", 0.12f);
            grass_material->set_shader_parameter("wind_speed", 1.2f);
            UtilityFunctions::print("TerrainGenerator: Grass shader loaded");
        }
    }
    
    // Create MultiMesh
    grass_multimesh.instantiate();
    grass_multimesh->set_mesh(grass_blade_mesh);
    grass_multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
    grass_multimesh->set_use_colors(true);
    
    // Collect valid grass positions
    std::vector<Transform3D> grass_transforms;
    std::vector<Color> grass_colors;
    
    float half_world = (config.map_size * config.tile_size) * 0.5f;
    float grass_spacing = 1.5f; // Space between grass clumps
    int grass_per_clump = 5;    // Number of blades per clump
    
    int max_grass = 50000; // Limit for performance
    
    for (float z = -half_world + 5.0f; z < half_world - 5.0f && (int)grass_transforms.size() < max_grass; z += grass_spacing) {
        for (float x = -half_world + 5.0f; x < half_world - 5.0f && (int)grass_transforms.size() < max_grass; x += grass_spacing) {
            // Add some randomness to position
            int hash = (config.seed + (int)(x * 100) * 374761393 + (int)(z * 100) * 668265263) % 2147483647;
            float offset_x = ((float)(hash % 1000) / 1000.0f - 0.5f) * grass_spacing * 0.8f;
            float offset_z = ((float)((hash >> 10) % 1000) / 1000.0f - 0.5f) * grass_spacing * 0.8f;
            
            float px = x + offset_x;
            float pz = z + offset_z;
            
            if (!is_valid_grass_position(px, pz)) continue;
            
            float height = get_height_at(px, pz);
            
            // Create a small clump of grass blades
            for (int i = 0; i < grass_per_clump; i++) {
                int clump_hash = hash + i * 12345;
                float cx = px + ((float)(clump_hash % 100) / 100.0f - 0.5f) * 0.5f;
                float cz = pz + ((float)((clump_hash >> 8) % 100) / 100.0f - 0.5f) * 0.5f;
                float ch = get_height_at(cx, cz);
                
                // Random rotation and scale
                float rotation = ((float)((clump_hash >> 4) % 360));
                float scale = 0.6f + ((float)((clump_hash >> 12) % 100) / 100.0f) * 0.8f;
                
                Transform3D transform;
                transform = transform.scaled(Vector3(scale, scale, scale));
                transform = transform.rotated(Vector3(0, 1, 0), Math::deg_to_rad(rotation));
                transform.origin = Vector3(cx, ch, cz);
                
                grass_transforms.push_back(transform);
                
                // Slight color variation
                float color_var = ((float)((clump_hash >> 16) % 100) / 100.0f) * 0.2f;
                grass_colors.push_back(Color(0.9f + color_var, 1.0f, 0.9f + color_var));
            }
        }
    }
    
    if (grass_transforms.empty()) {
        UtilityFunctions::print("TerrainGenerator: No valid grass positions found");
        return;
    }
    
    // Set up MultiMesh instances
    int grass_count = (int)grass_transforms.size();
    grass_multimesh->set_instance_count(grass_count);
    
    for (int i = 0; i < grass_count; i++) {
        grass_multimesh->set_instance_transform(i, grass_transforms[i]);
        grass_multimesh->set_instance_color(i, grass_colors[i]);
    }
    
    // Create MultiMeshInstance3D
    grass_instance = memnew(MultiMeshInstance3D);
    grass_instance->set_name("Grass");
    grass_instance->set_multimesh(grass_multimesh);
    
    if (grass_material.is_valid()) {
        grass_instance->set_material_override(grass_material);
    }
    
    // Grass should cast shadows but be transparent
    grass_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_ON);
    
    add_child(grass_instance);
    
    UtilityFunctions::print("TerrainGenerator: Generated ", grass_count, " grass blades");
}

void TerrainGenerator::setup_environment() {
    UtilityFunctions::print("TerrainGenerator: Setting up environment...");
    
    // === CREATE SKY ===
    Ref<ProceduralSkyMaterial> sky_material;
    sky_material.instantiate();
    
    // Set sky colors for a vibrant sunny day
    sky_material->set_sky_top_color(Color(0.25f, 0.5f, 0.95f));       // Vivid blue at top
    sky_material->set_sky_horizon_color(Color(0.6f, 0.8f, 1.0f));     // Bright horizon
    sky_material->set_ground_bottom_color(Color(0.25f, 0.35f, 0.2f)); // Green-ish ground
    sky_material->set_ground_horizon_color(Color(0.45f, 0.55f, 0.4f)); // Greenish horizon
    sky_material->set_sun_angle_max(45.0f);
    sky_material->set_sun_curve(0.1f);
    
    // Create sky
    sky.instantiate();
    sky->set_material(sky_material);
    sky->set_radiance_size(Sky::RADIANCE_SIZE_256);
    
    // === CREATE ENVIRONMENT ===
    environment.instantiate();
    environment->set_sky(sky);
    environment->set_background(Environment::BG_SKY);
    
    // Ambient light settings - brighter and more colorful
    environment->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
    environment->set_ambient_light_color(Color(0.7f, 0.75f, 0.8f));
    environment->set_ambient_light_energy(0.6f);
    environment->set_ambient_light_sky_contribution(0.8f);
    
    // Reflected light from sky
    environment->set_reflection_source(Environment::REFLECTION_SOURCE_SKY);
    
    // Tonemap settings - boost saturation and brightness
    environment->set_tonemapper(Environment::TONE_MAPPER_ACES);
    environment->set_tonemap_exposure(1.1f);
    environment->set_tonemap_white(1.2f);
    
    // SSAO for ambient occlusion
    environment->set_ssao_enabled(true);
    environment->set_ssao_radius(1.5f);
    environment->set_ssao_intensity(2.0f);
    environment->set_ssao_power(1.5f);
    
    // Fog settings - subtle distance fog only
    environment->set_fog_enabled(true);
    environment->set_fog_light_color(Color(0.75f, 0.82f, 0.9f));    // Light blue-white fog
    environment->set_fog_light_energy(0.8f);
    environment->set_fog_sun_scatter(0.1f);                          // Subtle scattering
    
    // Fog density settings - much lighter fog
    float world_size = config.map_size * config.tile_size;
    environment->set_fog_density(0.0005f);                           // Very light fog
    environment->set_fog_aerial_perspective(0.2f);                   // Subtle aerial perspective
    environment->set_fog_sky_affect(0.1f);                           // Minimal sky affect
    
    // Height fog - only near water
    environment->set_fog_height(config.water_level - 2.0f);          // Below water level
    environment->set_fog_height_density(0.02f);                      // Light height fog
    
    // Glow/bloom for sun and bright areas (subtle)
    environment->set_glow_enabled(true);
    environment->set_glow_intensity(0.5f);
    environment->set_glow_strength(0.8f);
    environment->set_glow_bloom(0.1f);
    environment->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_SOFTLIGHT);
    
    // Create WorldEnvironment node
    world_environment = memnew(WorldEnvironment);
    world_environment->set_name("WorldEnvironment");
    world_environment->set_environment(environment);
    add_child(world_environment);
    
    // === CREATE SUN LIGHT ===
    sun_light = memnew(DirectionalLight3D);
    sun_light->set_name("Sun");
    
    // Sun direction - slightly angled for nice shadows
    sun_light->set_rotation_degrees(Vector3(-45.0f, -30.0f, 0.0f));
    
    // Sun color - bright warm light
    sun_light->set_color(Color(1.0f, 0.98f, 0.9f));
    sun_light->set_param(Light3D::PARAM_ENERGY, 1.2f);
    sun_light->set_param(Light3D::PARAM_INDIRECT_ENERGY, 1.0f);
    
    // Shadow settings
    sun_light->set_shadow(true);
    sun_light->set_param(Light3D::PARAM_SHADOW_OPACITY, 0.7f);
    sun_light->set_param(Light3D::PARAM_SHADOW_BLUR, 1.0f);
    sun_light->set_shadow_mode(DirectionalLight3D::SHADOW_PARALLEL_4_SPLITS);
    
    // Shadow distance based on map size
    float shadow_distance = Math::min(world_size * 0.5f, 500.0f);
    sun_light->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, shadow_distance);
    
    // Enable shadow for terrain and trees
    sun_light->set_param(Light3D::PARAM_SHADOW_NORMAL_BIAS, 1.0f);
    sun_light->set_param(Light3D::PARAM_SHADOW_BIAS, 0.05f);
    
    add_child(sun_light);
    
    UtilityFunctions::print("TerrainGenerator: Environment setup complete (fog, sky, sun)");
}

} // namespace rts
