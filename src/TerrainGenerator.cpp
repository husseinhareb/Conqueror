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
    
    // Load realistic PBR textures and setup shaders
    load_terrain_textures();
    setup_terrain_shader();
    apply_terrain_material();
    
    // Generate vegetation and rocks
    generate_trees();
    generate_mountain_rocks();
    generate_snow_caps();
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
    if (rocks_container) {
        rocks_container->queue_free();
        rocks_container = nullptr;
    }
    if (snow_container) {
        snow_container->queue_free();
        snow_container = nullptr;
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
            
            // Color based on height and slope (for basic visualization)
            // Heights typically range from ~0.5 to ~20+, with snow at high elevations
            Color color;
            float slope = 1.0f - normal.y;
            
            // Calculate snow coverage for this vertex
            float snow_factor = 0.0f;
            if (height >= config.snow_start_height) {
                if (height >= config.snow_full_height) {
                    // Full snow at peak, less on steep slopes
                    snow_factor = 1.0f - Math::clamp(slope * 0.4f, 0.0f, 0.3f);
                } else {
                    // Gradual snow transition
                    float height_blend = (height - config.snow_start_height) / (config.snow_full_height - config.snow_start_height);
                    float slope_reduction = Math::clamp(slope * 1.2f, 0.0f, 0.6f);
                    snow_factor = height_blend * (1.0f - slope_reduction);
                }
            }
            
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
                // High grass - rich green transitioning to rock
                float rock_blend = (height - 5.0f) / 2.0f;
                color = Color(0.2f, 0.55f, 0.15f).lerp(Color(0.45f, 0.42f, 0.38f), rock_blend * slope);
            } else if (height < 10.0f) {
                // Rocky transition zone - brown/gray mix
                float rock_blend = (height - 7.0f) / 3.0f;
                Color dirt = Color(0.55f, 0.45f, 0.3f);
                Color rock = Color(0.5f, 0.48f, 0.45f);
                color = dirt.lerp(rock, rock_blend + slope * 0.3f);
            } else if (height < config.snow_start_height) {
                // Mountain rock zone
                float rock_variation = noise2d(wx * 0.1f, wz * 0.1f, config.seed + 999);
                Color rock_base = Color(0.5f, 0.5f, 0.5f);
                Color rock_dark = Color(0.38f, 0.36f, 0.34f);
                color = rock_base.lerp(rock_dark, (rock_variation + 1.0f) * 0.25f + slope * 0.2f);
            } else {
                // Snow zone - already calculated snow_factor above
                Color rock = Color(0.48f, 0.47f, 0.46f);
                Color snow = Color(0.95f, 0.96f, 0.98f);
                color = rock.lerp(snow, snow_factor);
            }
            
            // Apply snow blending for high elevations even if below snow line
            if (snow_factor > 0.0f && height < config.snow_start_height + 2.0f) {
                // Patchy snow near snow line
                float noise_val = noise2d(wx * 0.2f, wz * 0.2f, config.seed + 777);
                if (noise_val > 0.3f) {
                    Color snow = Color(0.93f, 0.94f, 0.96f);
                    color = color.lerp(snow, snow_factor * (noise_val - 0.3f) * 1.5f);
                }
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
            
            // ================================================================
            // PERFECT WATER COLORS - Natural lake appearance
            // ================================================================
            water_shader_mat->set_shader_parameter("water_color_shallow", Color(0.08f, 0.32f, 0.45f));
            water_shader_mat->set_shader_parameter("water_color_deep", Color(0.015f, 0.08f, 0.15f));
            water_shader_mat->set_shader_parameter("water_color_fresnel", Color(0.1f, 0.28f, 0.4f));
            
            // ================================================================
            // GERSTNER WAVES - Gentle, realistic lake waves
            // ================================================================
            water_shader_mat->set_shader_parameter("wave_speed", 0.4f);  // Slower for calm lake
            water_shader_mat->set_shader_parameter("wave_scale", 0.012f);
            
            // Wave 1 - Primary gentle swell (longest wavelength)
            water_shader_mat->set_shader_parameter("wave1_amplitude", 0.18f);
            water_shader_mat->set_shader_parameter("wave1_frequency", 0.06f);
            water_shader_mat->set_shader_parameter("wave1_steepness", 0.35f);
            water_shader_mat->set_shader_parameter("wave1_direction", Vector2(1.0f, 0.15f));
            
            // Wave 2 - Secondary swell (different direction)
            water_shader_mat->set_shader_parameter("wave2_amplitude", 0.12f);
            water_shader_mat->set_shader_parameter("wave2_frequency", 0.11f);
            water_shader_mat->set_shader_parameter("wave2_steepness", 0.3f);
            water_shader_mat->set_shader_parameter("wave2_direction", Vector2(0.5f, 0.85f));
            
            // Wave 3 - Small chop
            water_shader_mat->set_shader_parameter("wave3_amplitude", 0.06f);
            water_shader_mat->set_shader_parameter("wave3_frequency", 0.25f);
            water_shader_mat->set_shader_parameter("wave3_steepness", 0.25f);
            water_shader_mat->set_shader_parameter("wave3_direction", Vector2(-0.35f, 0.92f));
            
            // Wave 4 - Micro ripples for detail
            water_shader_mat->set_shader_parameter("wave4_amplitude", 0.03f);
            water_shader_mat->set_shader_parameter("wave4_frequency", 0.5f);
            water_shader_mat->set_shader_parameter("wave4_steepness", 0.18f);
            water_shader_mat->set_shader_parameter("wave4_direction", Vector2(0.8f, -0.55f));
            
            // ================================================================
            // SURFACE PROPERTIES - Crystal clear lake water
            // ================================================================
            water_shader_mat->set_shader_parameter("roughness", 0.02f);  // Very smooth
            water_shader_mat->set_shader_parameter("metallic", 0.05f);
            water_shader_mat->set_shader_parameter("opacity", 0.92f);  // Slightly transparent
            
            // ================================================================
            // FRESNEL - Natural viewing angle color shift
            // ================================================================
            water_shader_mat->set_shader_parameter("fresnel_power", 5.0f);
            water_shader_mat->set_shader_parameter("fresnel_bias", 0.02f);
            
            // ================================================================
            // REFLECTION - Sky and environment reflection
            // ================================================================
            water_shader_mat->set_shader_parameter("reflection_strength", 0.85f);
            water_shader_mat->set_shader_parameter("sky_color", Color(0.55f, 0.75f, 0.98f));
            
            // ================================================================
            // FOAM - Very subtle for calm lake
            // ================================================================
            water_shader_mat->set_shader_parameter("foam_amount", 0.08f);
            water_shader_mat->set_shader_parameter("foam_cutoff", 0.88f);
            water_shader_mat->set_shader_parameter("foam_color", Color(0.97f, 0.99f, 1.0f));
            
            // ================================================================
            // SUBSURFACE SCATTERING - Light through water
            // ================================================================
            water_shader_mat->set_shader_parameter("sss_strength", 0.45f);
            water_shader_mat->set_shader_parameter("sss_color", Color(0.06f, 0.4f, 0.32f));
            
            // ================================================================
            // CAUSTICS - Light patterns on the bottom
            // ================================================================
            water_shader_mat->set_shader_parameter("caustic_strength", 0.18f);
            water_shader_mat->set_shader_parameter("caustic_scale", 0.02f);
            
            // ================================================================
            // NORMAL MAPS - Surface detail
            // ================================================================
            String normal1_path = "res://assets/textures/terrain/water/water_normal_1.png";
            String normal2_path = "res://assets/textures/terrain/water/water_normal_2.png";
            
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
            
            water_shader_mat->set_shader_parameter("normal_map_scale", 0.03f);
            water_shader_mat->set_shader_parameter("normal_map_strength", 0.6f);
            
            UtilityFunctions::print("TerrainGenerator: Perfect water shader configured");
        }
    }
    
    // Fallback material if shader not available
    Ref<StandardMaterial3D> water_mat_fallback;
    if (!water_shader_mat.is_valid()) {
        water_mat_fallback.instantiate();
        water_mat_fallback->set_albedo(Color(0.05f, 0.18f, 0.28f, 0.9f));
        water_mat_fallback->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
        water_mat_fallback->set_roughness(0.02f);
        water_mat_fallback->set_metallic(0.5f);
        water_mat_fallback->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    }
    
    // Create perfectly tessellated water mesh for each lake
    for (size_t i = 0; i < lake_positions.size(); i++) {
        const LakeData &lake = lake_positions[i];
        
        // Calculate optimal tessellation based on lake size
        // Larger lakes need more vertices for smooth appearance
        int base_segments = 128;  // High quality base
        int base_rings = 48;      // Many concentric rings
        
        // Scale tessellation with lake radius (more for bigger lakes)
        float size_factor = Math::clamp(lake.radius / 100.0f, 0.5f, 2.0f);
        int segments = (int)(base_segments * size_factor);
        int rings_count = (int)(base_rings * size_factor);
        
        // Generate ultra-high-quality water mesh
        Ref<ArrayMesh> lake_mesh_data = create_irregular_lake_mesh(
            lake.radius, 
            segments,
            rings_count,
            config.seed + (int)i * 1000
        );
        
        MeshInstance3D *lake_mesh = memnew(MeshInstance3D);
        lake_mesh->set_mesh(lake_mesh_data);
        
        // Apply material to both surfaces (water surface and skirt)
        if (water_shader_mat.is_valid()) {
            lake_mesh->set_surface_override_material(0, water_shader_mat);
            lake_mesh->set_surface_override_material(1, water_shader_mat);
        } else {
            lake_mesh->set_surface_override_material(0, water_mat_fallback);
            lake_mesh->set_surface_override_material(1, water_mat_fallback);
        }
        
        // Position water at perfect height
        lake_mesh->set_position(Vector3(lake.world_x, lake.water_height + 0.12f, lake.world_z));
        lake_mesh->set_name(String("Lake_") + String::num_int64(i));
        
        // Enable shadows for realism
        lake_mesh->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        
        lakes_container->add_child(lake_mesh);
    }
    
    UtilityFunctions::print("TerrainGenerator: Created ", lake_positions.size(), " perfect lake meshes");
}

// ============================================================================
// PERFECT WATER MESH GENERATION
// Creates an ultra-smooth, high-tessellation water surface with proper skirt
// ============================================================================
Ref<ArrayMesh> TerrainGenerator::create_irregular_lake_mesh(float radius, int radial_segments, int rings, int seed) {
    Ref<SurfaceTool> st;
    st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    
    // Ensure minimum quality for perfect appearance
    int actual_segments = Math::max(radial_segments, 96);
    int actual_rings = Math::max(rings, 40);
    
    // Slight radius expansion to ensure full coverage
    float padded_radius = radius * 1.05f;
    
    // Pre-calculate all vertices for the smooth circular mesh
    std::vector<std::vector<Vector3>> ring_vertices;
    std::vector<std::vector<Vector2>> ring_uvs;
    ring_vertices.resize(actual_rings + 1);
    ring_uvs.resize(actual_rings + 1);
    
    // Center vertex
    ring_vertices[0].push_back(Vector3(0, 0, 0));
    ring_uvs[0].push_back(Vector2(0.5f, 0.5f));
    
    // Generate perfectly smooth circular rings
    for (int ring = 1; ring <= actual_rings; ring++) {
        float ring_ratio = (float)ring / (float)actual_rings;
        float ring_radius = padded_radius * ring_ratio;
        
        ring_vertices[ring].resize(actual_segments);
        ring_uvs[ring].resize(actual_segments);
        
        for (int seg = 0; seg < actual_segments; seg++) {
            float angle = (float)seg / (float)actual_segments * Math_PI * 2.0f;
            
            float x = Math::cos(angle) * ring_radius;
            float z = Math::sin(angle) * ring_radius;
            
            ring_vertices[ring][seg] = Vector3(x, 0, z);
            
            // World-space UVs for proper normal map tiling
            float u = x / (padded_radius * 2.0f) + 0.5f;
            float v = z / (padded_radius * 2.0f) + 0.5f;
            ring_uvs[ring][seg] = Vector2(u, v);
        }
    }
    
    // All normals point straight up for flat water
    Vector3 up_normal = Vector3(0, 1, 0);
    
    // Build inner fan triangles (center to first ring)
    for (int seg = 0; seg < actual_segments; seg++) {
        int next_seg = (seg + 1) % actual_segments;
        
        st->set_normal(up_normal);
        st->set_uv(ring_uvs[0][0]);
        st->add_vertex(ring_vertices[0][0]);
        
        st->set_normal(up_normal);
        st->set_uv(ring_uvs[1][seg]);
        st->add_vertex(ring_vertices[1][seg]);
        
        st->set_normal(up_normal);
        st->set_uv(ring_uvs[1][next_seg]);
        st->add_vertex(ring_vertices[1][next_seg]);
    }
    
    // Build ring strips (ring by ring outward)
    for (int ring = 1; ring < actual_rings; ring++) {
        for (int seg = 0; seg < actual_segments; seg++) {
            int next_seg = (seg + 1) % actual_segments;
            
            Vector3 v0 = ring_vertices[ring][seg];
            Vector3 v1 = ring_vertices[ring][next_seg];
            Vector3 v2 = ring_vertices[ring + 1][seg];
            Vector3 v3 = ring_vertices[ring + 1][next_seg];
            
            Vector2 uv0 = ring_uvs[ring][seg];
            Vector2 uv1 = ring_uvs[ring][next_seg];
            Vector2 uv2 = ring_uvs[ring + 1][seg];
            Vector2 uv3 = ring_uvs[ring + 1][next_seg];
            
            // Quad as two triangles
            st->set_normal(up_normal);
            st->set_uv(uv0);
            st->add_vertex(v0);
            st->set_normal(up_normal);
            st->set_uv(uv2);
            st->add_vertex(v2);
            st->set_normal(up_normal);
            st->set_uv(uv1);
            st->add_vertex(v1);
            
            st->set_normal(up_normal);
            st->set_uv(uv1);
            st->add_vertex(v1);
            st->set_normal(up_normal);
            st->set_uv(uv2);
            st->add_vertex(v2);
            st->set_normal(up_normal);
            st->set_uv(uv3);
            st->add_vertex(v3);
        }
    }
    
    // Generate tangents for perfect normal mapping
    st->generate_tangents();
    
    Ref<ArrayMesh> water_mesh = st->commit();
    
    // ========================================================================
    // CREATE DEEP SKIRT - Hides all terrain/water intersection artifacts
    // ========================================================================
    float skirt_depth = 8.0f;  // Very deep to handle any terrain
    
    Ref<SurfaceTool> skirt_st;
    skirt_st.instantiate();
    skirt_st->begin(Mesh::PRIMITIVE_TRIANGLES);
    
    int outer_ring = actual_rings;
    
    for (int seg = 0; seg < actual_segments; seg++) {
        int next_seg = (seg + 1) % actual_segments;
        
        Vector3 top0 = ring_vertices[outer_ring][seg];
        Vector3 top1 = ring_vertices[outer_ring][next_seg];
        Vector3 bottom0 = Vector3(top0.x, top0.y - skirt_depth, top0.z);
        Vector3 bottom1 = Vector3(top1.x, top1.y - skirt_depth, top1.z);
        
        // Outward-facing normal for proper skirt lighting
        float angle = (float)seg / (float)actual_segments * Math_PI * 2.0f;
        Vector3 outward = Vector3(Math::cos(angle), 0, Math::sin(angle));
        
        // UVs for skirt
        Vector2 uv_top0 = ring_uvs[outer_ring][seg];
        Vector2 uv_top1 = ring_uvs[outer_ring][next_seg];
        Vector2 uv_bottom0 = Vector2(uv_top0.x, 1.0f);
        Vector2 uv_bottom1 = Vector2(uv_top1.x, 1.0f);
        
        // Two triangles per skirt segment
        skirt_st->set_normal(outward);
        skirt_st->set_uv(uv_top0);
        skirt_st->add_vertex(top0);
        skirt_st->set_normal(outward);
        skirt_st->set_uv(uv_bottom0);
        skirt_st->add_vertex(bottom0);
        skirt_st->set_normal(outward);
        skirt_st->set_uv(uv_top1);
        skirt_st->add_vertex(top1);
        
        skirt_st->set_normal(outward);
        skirt_st->set_uv(uv_top1);
        skirt_st->add_vertex(top1);
        skirt_st->set_normal(outward);
        skirt_st->set_uv(uv_bottom0);
        skirt_st->add_vertex(bottom0);
        skirt_st->set_normal(outward);
        skirt_st->set_uv(uv_bottom1);
        skirt_st->add_vertex(bottom1);
    }
    
    // Add skirt as second surface
    skirt_st->commit(water_mesh);
    
    return water_mesh;
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
    // Create shared materials for all trees with realistic textures
    tree_trunk_material.instantiate();
    
    // Try to load bark texture
    String bark_path = "res://assets/textures/vegetation/bark/bark_albedo.jpg";
    if (FileAccess::file_exists(bark_path)) {
        Ref<ImageTexture> bark_tex = load_texture_from_file(bark_path);
        if (bark_tex.is_valid()) {
            tree_trunk_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, bark_tex);
        }
        Ref<ImageTexture> bark_norm = load_texture_from_file("res://assets/textures/vegetation/bark/bark_normal.jpg");
        if (bark_norm.is_valid()) {
            tree_trunk_material->set_texture(StandardMaterial3D::TEXTURE_NORMAL, bark_norm);
            tree_trunk_material->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        }
    } else {
        tree_trunk_material->set_albedo(Color(0.4f, 0.25f, 0.15f)); // Brown trunk fallback
    }
    tree_trunk_material->set_roughness(0.9f);
    tree_trunk_material->set_uv1_scale(Vector3(2.0f, 4.0f, 1.0f)); // Scale UVs for bark
    
    tree_leaves_material.instantiate();
    
    // Try to load foliage texture
    String foliage_path = "res://assets/textures/vegetation/foliage/foliage_albedo.jpg";
    if (FileAccess::file_exists(foliage_path)) {
        Ref<ImageTexture> foliage_tex = load_texture_from_file(foliage_path);
        if (foliage_tex.is_valid()) {
            tree_leaves_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, foliage_tex);
        }
        Ref<ImageTexture> foliage_norm = load_texture_from_file("res://assets/textures/vegetation/foliage/foliage_normal.jpg");
        if (foliage_norm.is_valid()) {
            tree_leaves_material->set_texture(StandardMaterial3D::TEXTURE_NORMAL, foliage_norm);
            tree_leaves_material->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        }
    } else {
        tree_leaves_material->set_albedo(Color(0.15f, 0.45f, 0.12f)); // Dark green leaves fallback
    }
    tree_leaves_material->set_roughness(0.8f);
    tree_leaves_material->set_uv1_scale(Vector3(2.0f, 2.0f, 1.0f));
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

// ============================================================================
// MOUNTAIN ROCKS AND SNOW
// ============================================================================

bool TerrainGenerator::is_valid_rock_position(float x, float z) const {
    // Check if within bounds
    if (!is_within_bounds(x, z)) return false;
    
    // Check height - rocks spawn in mountain areas
    float height = get_height_at(x, z);
    if (height < config.rock_min_height) return false;
    
    // Check slope - rocks can be on steeper terrain than trees
    Vector3 normal = get_normal_at(x, z);
    float slope = 1.0f - normal.y;
    if (slope > config.rock_max_slope) return false;
    
    // Keep center area somewhat clear
    float dist_from_center = sqrt(x * x + z * z);
    if (dist_from_center < config.tree_center_clear * 0.7f) return false;
    
    return true;
}

bool TerrainGenerator::is_mountain_area(float x, float z) const {
    float height = get_height_at(x, z);
    return height >= config.rock_min_height;
}

float TerrainGenerator::get_snow_coverage(float height, float slope) const {
    // Snow coverage based on height and slope
    // Snow collects less on steep slopes
    if (height < config.snow_start_height) return 0.0f;
    if (height >= config.snow_full_height) {
        // At very high elevations, even steep slopes have some snow
        float base_coverage = 1.0f;
        float slope_reduction = Math::clamp(slope * 0.3f, 0.0f, 0.4f);
        return base_coverage - slope_reduction;
    }
    
    // Interpolate snow coverage based on height
    float height_factor = (height - config.snow_start_height) / (config.snow_full_height - config.snow_start_height);
    
    // Steeper slopes have less snow
    float slope_factor = 1.0f - Math::clamp(slope * 1.5f, 0.0f, 0.8f);
    
    return height_factor * slope_factor;
}

Ref<ArrayMesh> TerrainGenerator::create_rock_mesh(float size, int detail_level, int seed) {
    // Create an irregular rocky mesh using deformed icosphere approach
    Ref<SurfaceTool> st;
    st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    
    // Generate deformed sphere vertices
    std::vector<Vector3> vertices;
    std::vector<int> indices;
    
    // Simple approach: create a deformed box/polyhedron
    // Base vertices of a rough cube
    float s = size * 0.5f;
    
    // Create multiple deformed vertices
    auto deform_vertex = [seed, size](Vector3 v, int vertex_idx) -> Vector3 {
        int h = (seed + vertex_idx * 374761393) % 2147483647;
        h = (h ^ (h >> 13)) * 1274126177;
        float noise_x = ((float)((h >> 0) % 1000) / 1000.0f - 0.5f) * size * 0.3f;
        float noise_y = ((float)((h >> 10) % 1000) / 1000.0f - 0.5f) * size * 0.25f;
        float noise_z = ((float)((h >> 20) % 1000) / 1000.0f - 0.5f) * size * 0.3f;
        return v + Vector3(noise_x, noise_y, noise_z);
    };
    
    // Create a rough dodecahedron-like shape for more natural rocks
    // Top vertices
    vertices.push_back(deform_vertex(Vector3(0, s * 1.1f, 0), 0)); // Top peak
    vertices.push_back(deform_vertex(Vector3(s * 0.6f, s * 0.7f, s * 0.4f), 1));
    vertices.push_back(deform_vertex(Vector3(-s * 0.5f, s * 0.7f, s * 0.6f), 2));
    vertices.push_back(deform_vertex(Vector3(-s * 0.6f, s * 0.7f, -s * 0.3f), 3));
    vertices.push_back(deform_vertex(Vector3(s * 0.4f, s * 0.7f, -s * 0.6f), 4));
    
    // Middle ring
    vertices.push_back(deform_vertex(Vector3(s * 0.9f, 0, s * 0.3f), 5));
    vertices.push_back(deform_vertex(Vector3(s * 0.3f, 0, s * 0.9f), 6));
    vertices.push_back(deform_vertex(Vector3(-s * 0.7f, 0, s * 0.7f), 7));
    vertices.push_back(deform_vertex(Vector3(-s * 0.9f, 0, -s * 0.2f), 8));
    vertices.push_back(deform_vertex(Vector3(-s * 0.2f, 0, -s * 0.9f), 9));
    vertices.push_back(deform_vertex(Vector3(s * 0.7f, 0, -s * 0.7f), 10));
    
    // Bottom vertices
    vertices.push_back(deform_vertex(Vector3(s * 0.4f, -s * 0.6f, s * 0.5f), 11));
    vertices.push_back(deform_vertex(Vector3(-s * 0.5f, -s * 0.5f, s * 0.4f), 12));
    vertices.push_back(deform_vertex(Vector3(-s * 0.4f, -s * 0.6f, -s * 0.5f), 13));
    vertices.push_back(deform_vertex(Vector3(s * 0.5f, -s * 0.5f, -s * 0.4f), 14));
    vertices.push_back(deform_vertex(Vector3(0, -s * 0.8f, 0), 15)); // Bottom
    
    // Define triangular faces connecting these vertices
    // Top cap
    int top_tris[] = {
        0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
        // Upper-middle connections
        1, 5, 6, 1, 6, 2, 2, 6, 7, 2, 7, 3,
        3, 7, 8, 3, 8, 9, 3, 9, 4, 4, 9, 10,
        4, 10, 5, 4, 5, 1,
        // Middle band
        5, 11, 6, 6, 11, 12, 6, 12, 7, 7, 12, 8,
        8, 12, 13, 8, 13, 9, 9, 13, 14, 9, 14, 10,
        10, 14, 11, 10, 11, 5,
        // Bottom cap
        15, 12, 11, 15, 13, 12, 15, 14, 13, 15, 11, 14
    };
    
    for (int i = 0; i < 60; i += 3) {
        indices.push_back(top_tris[i]);
        indices.push_back(top_tris[i + 1]);
        indices.push_back(top_tris[i + 2]);
    }
    
    // Add vertices and indices to surface tool
    for (size_t i = 0; i < indices.size(); i += 3) {
        Vector3 v0 = vertices[indices[i]];
        Vector3 v1 = vertices[indices[i + 1]];
        Vector3 v2 = vertices[indices[i + 2]];
        
        // Calculate face normal
        Vector3 edge1 = v1 - v0;
        Vector3 edge2 = v2 - v0;
        Vector3 normal = edge1.cross(edge2).normalized();
        
        st->set_normal(normal);
        st->set_uv(Vector2(0.5f, 0.5f));
        st->add_vertex(v0);
        
        st->set_normal(normal);
        st->set_uv(Vector2(0.0f, 1.0f));
        st->add_vertex(v1);
        
        st->set_normal(normal);
        st->set_uv(Vector2(1.0f, 1.0f));
        st->add_vertex(v2);
    }
    
    st->generate_normals();
    return st->commit();
}

void TerrainGenerator::generate_mountain_rocks() {
    UtilityFunctions::print("TerrainGenerator: Generating mountain rocks...");
    
    // Create container for all rocks
    rocks_container = memnew(Node3D);
    rocks_container->set_name("MountainRocks");
    add_child(rocks_container);
    
    // Load rock textures for enhanced materials
    Ref<ImageTexture> rock_albedo_tex = load_texture_from_file("res://assets/textures/terrain/rock/rock_albedo.jpg");
    Ref<ImageTexture> rock_normal_tex = load_texture_from_file("res://assets/textures/terrain/rock/rock_normal.jpg");
    Ref<ImageTexture> moss_albedo_tex = load_texture_from_file("res://assets/textures/terrain/moss/moss_albedo.jpg");
    Ref<ImageTexture> moss_normal_tex = load_texture_from_file("res://assets/textures/terrain/moss/moss_normal.jpg");
    Ref<ImageTexture> snow_albedo_tex = load_texture_from_file("res://assets/textures/terrain/snow/snow_albedo.jpg");
    Ref<ImageTexture> snow_normal_tex = load_texture_from_file("res://assets/textures/terrain/snow/snow_normal.jpg");
    
    // Create rock materials with textures
    // Gray granite-like rock
    rock_material_gray.instantiate();
    if (rock_albedo_tex.is_valid()) {
        rock_material_gray->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, rock_albedo_tex);
        if (rock_normal_tex.is_valid()) {
            rock_material_gray->set_texture(StandardMaterial3D::TEXTURE_NORMAL, rock_normal_tex);
            rock_material_gray->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        }
    } else {
        rock_material_gray->set_albedo(Color(0.55f, 0.53f, 0.5f));
    }
    rock_material_gray->set_roughness(0.85f);
    rock_material_gray->set_metallic(0.05f);
    rock_material_gray->set_uv1_scale(Vector3(0.5f, 0.5f, 0.5f));
    
    // Darker rock for contrast (uses same texture with tint)
    rock_material_dark.instantiate();
    if (rock_albedo_tex.is_valid()) {
        rock_material_dark->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, rock_albedo_tex);
        if (rock_normal_tex.is_valid()) {
            rock_material_dark->set_texture(StandardMaterial3D::TEXTURE_NORMAL, rock_normal_tex);
            rock_material_dark->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        }
        // Darken the rock by applying a dark modulate
        rock_material_dark->set_albedo(Color(0.6f, 0.58f, 0.55f));
    } else {
        rock_material_dark->set_albedo(Color(0.35f, 0.33f, 0.3f));
    }
    rock_material_dark->set_roughness(0.9f);
    rock_material_dark->set_metallic(0.02f);
    rock_material_dark->set_uv1_scale(Vector3(0.5f, 0.5f, 0.5f));
    
    // Mossy rock for lower elevations
    rock_material_mossy.instantiate();
    if (moss_albedo_tex.is_valid()) {
        rock_material_mossy->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, moss_albedo_tex);
        if (moss_normal_tex.is_valid()) {
            rock_material_mossy->set_texture(StandardMaterial3D::TEXTURE_NORMAL, moss_normal_tex);
            rock_material_mossy->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        }
    } else {
        rock_material_mossy->set_albedo(Color(0.4f, 0.45f, 0.35f));
    }
    rock_material_mossy->set_roughness(0.88f);
    rock_material_mossy->set_metallic(0.0f);
    rock_material_mossy->set_uv1_scale(Vector3(0.8f, 0.8f, 0.8f));
    
    // Snow-covered rock for high elevations
    snow_rock_material.instantiate();
    if (snow_albedo_tex.is_valid()) {
        snow_rock_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, snow_albedo_tex);
        if (snow_normal_tex.is_valid()) {
            snow_rock_material->set_texture(StandardMaterial3D::TEXTURE_NORMAL, snow_normal_tex);
            snow_rock_material->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        }
    } else {
        snow_rock_material->set_albedo(Color(0.92f, 0.94f, 0.96f));
    }
    snow_rock_material->set_roughness(0.7f);
    snow_rock_material->set_metallic(0.0f);
    snow_rock_material->set_specular(0.6f);
    snow_rock_material->set_uv1_scale(Vector3(0.6f, 0.6f, 0.6f));
    
    float half_world = (config.map_size * config.tile_size) * 0.5f;
    
    // Store placed rock positions for spacing check
    std::vector<Vector2> placed_rocks;
    placed_rocks.reserve(config.rock_count);
    
    int attempts = 0;
    int max_attempts = config.rock_count * 15;
    int rocks_placed = 0;
    int snow_rocks = 0;
    int mossy_rocks = 0;
    int gray_rocks = 0;
    
    while (rocks_placed < config.rock_count && attempts < max_attempts) {
        attempts++;
        
        // Random position using seed
        int hash = (config.seed + attempts * 48271 + 99999) % 2147483647;
        int hash2 = (hash * 16807) % 2147483647;
        
        float x = ((float)(hash % 10000) / 10000.0f) * half_world * 2.0f - half_world;
        float z = ((float)(hash2 % 10000) / 10000.0f) * half_world * 2.0f - half_world;
        
        // Check if valid position
        if (!is_valid_rock_position(x, z)) continue;
        
        // Check spacing from other rocks
        bool too_close = false;
        Vector2 new_pos(x, z);
        for (const Vector2 &pos : placed_rocks) {
            if (pos.distance_to(new_pos) < config.rock_min_spacing) {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;
        
        // Get terrain data at this position
        float terrain_height = get_height_at(x, z);
        Vector3 normal = get_normal_at(x, z);
        float slope = 1.0f - normal.y;
        
        // Calculate snow coverage for this rock
        float snow_coverage = get_snow_coverage(terrain_height, slope);
        
        // Random rock size and type
        int hash3 = (hash2 * 69621) % 2147483647;
        int hash4 = (hash3 * 12345) % 2147483647;
        float base_size = 0.5f + ((float)(hash3 % 1000) / 1000.0f) * 2.5f; // 0.5 to 3.0
        
        // Larger rocks are rarer
        if ((hash4 % 100) < 20) {
            base_size *= 1.5f; // 20% chance of larger rock
        }
        if ((hash4 % 100) < 5) {
            base_size *= 1.8f; // 5% chance of boulder
        }
        
        // Random rotation
        float rotation_y = ((float)(hash3 % 360));
        float rotation_x = ((float)((hash3 >> 10) % 30)) - 15.0f; // Slight tilt
        float rotation_z = ((float)((hash3 >> 20) % 30)) - 15.0f;
        
        // Create rock mesh
        Ref<ArrayMesh> rock_mesh = create_rock_mesh(base_size, 1, hash4);
        
        // Determine material based on height and snow coverage
        Ref<StandardMaterial3D> material;
        if (snow_coverage > 0.7f) {
            material = snow_rock_material;
            snow_rocks++;
        } else if (snow_coverage > 0.3f) {
            // Partially snow-covered - use gray with slight tint
            Ref<StandardMaterial3D> partial_snow;
            partial_snow.instantiate();
            Color base = Color(0.55f, 0.53f, 0.5f);
            Color snow = Color(0.92f, 0.94f, 0.96f);
            partial_snow->set_albedo(base.lerp(snow, snow_coverage));
            partial_snow->set_roughness(0.75f);
            partial_snow->set_metallic(0.03f);
            material = partial_snow;
            snow_rocks++;
        } else if (terrain_height < 8.0f) {
            // Lower mountain areas - mossy rocks
            material = rock_material_mossy;
            mossy_rocks++;
        } else if ((hash4 % 100) < 40) {
            // Mix of gray and dark rocks
            material = rock_material_dark;
            gray_rocks++;
        } else {
            material = rock_material_gray;
            gray_rocks++;
        }
        
        // Create rock instance
        MeshInstance3D *rock = memnew(MeshInstance3D);
        rock->set_mesh(rock_mesh);
        rock->set_surface_override_material(0, material);
        
        // Position the rock, partially buried in terrain
        float bury_depth = base_size * 0.2f;
        rock->set_position(Vector3(x, terrain_height - bury_depth, z));
        rock->set_rotation_degrees(Vector3(rotation_x, rotation_y, rotation_z));
        
        // Add slight scale variation
        float scale_var = 0.85f + ((float)((hash4 >> 8) % 100) / 100.0f) * 0.3f;
        float y_scale = 0.7f + ((float)((hash4 >> 16) % 100) / 100.0f) * 0.4f; // Flatter or taller
        rock->set_scale(Vector3(scale_var, y_scale, scale_var));
        
        // Add rock clusters sometimes
        if ((hash4 % 100) < 30 && rocks_placed < config.rock_count - 3) {
            // Create 2-4 smaller rocks around this one
            int cluster_count = 2 + (hash4 % 3);
            for (int c = 0; c < cluster_count; c++) {
                int cluster_hash = hash4 + c * 54321;
                float angle = ((float)(cluster_hash % 360)) * Math_PI / 180.0f;
                float dist = base_size * (0.8f + ((float)((cluster_hash >> 8) % 100) / 100.0f) * 0.5f);
                float cx = x + cos(angle) * dist;
                float cz = z + sin(angle) * dist;
                
                if (!is_within_bounds(cx, cz)) continue;
                
                float c_height = get_height_at(cx, cz);
                float c_size = base_size * (0.3f + ((float)((cluster_hash >> 16) % 100) / 100.0f) * 0.4f);
                
                Ref<ArrayMesh> cluster_rock_mesh = create_rock_mesh(c_size, 0, cluster_hash);
                MeshInstance3D *cluster_rock = memnew(MeshInstance3D);
                cluster_rock->set_mesh(cluster_rock_mesh);
                cluster_rock->set_surface_override_material(0, material);
                cluster_rock->set_position(Vector3(cx, c_height - c_size * 0.15f, cz));
                cluster_rock->set_rotation_degrees(Vector3(
                    ((float)((cluster_hash >> 4) % 40)) - 20.0f,
                    ((float)(cluster_hash % 360)),
                    ((float)((cluster_hash >> 12) % 40)) - 20.0f
                ));
                
                rocks_container->add_child(cluster_rock);
            }
        }
        
        rocks_container->add_child(rock);
        placed_rocks.push_back(new_pos);
        rocks_placed++;
    }
    
    UtilityFunctions::print("TerrainGenerator: Placed ", rocks_placed, " rocks (", 
                            snow_rocks, " snowy, ", mossy_rocks, " mossy, ", gray_rocks, " gray)");
}

// ============================================================================
// SNOW CAP GENERATION - Creates snow blankets on mountain peaks
// ============================================================================

Ref<ArrayMesh> TerrainGenerator::create_snow_patch_mesh(float center_x, float center_z, float radius, int segments) {
    // Create a snow patch that follows the terrain contour
    Ref<SurfaceTool> st;
    st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    
    // Create vertices in a radial pattern following terrain
    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    
    // Center vertex
    float center_height = get_height_at(center_x, center_z) + 0.08f; // Slightly above terrain
    Vector3 center_normal = get_normal_at(center_x, center_z);
    vertices.push_back(Vector3(center_x, center_height, center_z));
    normals.push_back(center_normal);
    
    // Create rings of vertices
    int rings = 4;
    for (int ring = 1; ring <= rings; ring++) {
        float ring_radius = radius * (float)ring / rings;
        int ring_segments = segments * ring / 2;
        if (ring_segments < 6) ring_segments = 6;
        
        for (int seg = 0; seg < ring_segments; seg++) {
            float angle = (float)seg / ring_segments * Math_PI * 2.0f;
            
            // Add some noise to make edges irregular
            int hash = (config.seed + (int)(center_x * 100) + (int)(center_z * 100) + ring * 1000 + seg * 100) % 2147483647;
            float noise = ((float)(hash % 1000) / 1000.0f - 0.5f) * 0.3f;
            float actual_radius = ring_radius * (1.0f + noise);
            
            float vx = center_x + cos(angle) * actual_radius;
            float vz = center_z + sin(angle) * actual_radius;
            
            // Check if within bounds
            if (!is_within_bounds(vx, vz)) {
                vx = center_x + cos(angle) * ring_radius * 0.5f;
                vz = center_z + sin(angle) * ring_radius * 0.5f;
            }
            
            float vh = get_height_at(vx, vz);
            
            // Only include if above snow line (with some tolerance)
            if (vh < config.snow_start_height - 1.0f) {
                // Use a point closer to center
                vx = center_x + cos(angle) * ring_radius * 0.3f;
                vz = center_z + sin(angle) * ring_radius * 0.3f;
                vh = get_height_at(vx, vz);
            }
            
            // Snow sits slightly above terrain
            float snow_offset = 0.05f + 0.03f * (1.0f - (float)ring / rings);
            vertices.push_back(Vector3(vx, vh + snow_offset, vz));
            normals.push_back(get_normal_at(vx, vz));
        }
    }
    
    // Create triangles - fan from center to first ring
    int first_ring_start = 1;
    int first_ring_count = segments / 2;
    if (first_ring_count < 6) first_ring_count = 6;
    
    for (int i = 0; i < first_ring_count; i++) {
        int next = (i + 1) % first_ring_count;
        
        st->set_normal(normals[0]);
        st->set_uv(Vector2(0.5f, 0.5f));
        st->add_vertex(vertices[0]);
        
        st->set_normal(normals[first_ring_start + i]);
        st->set_uv(Vector2(0.5f + 0.25f * cos((float)i / first_ring_count * Math_PI * 2.0f),
                           0.5f + 0.25f * sin((float)i / first_ring_count * Math_PI * 2.0f)));
        st->add_vertex(vertices[first_ring_start + i]);
        
        st->set_normal(normals[first_ring_start + next]);
        st->set_uv(Vector2(0.5f + 0.25f * cos((float)next / first_ring_count * Math_PI * 2.0f),
                           0.5f + 0.25f * sin((float)next / first_ring_count * Math_PI * 2.0f)));
        st->add_vertex(vertices[first_ring_start + next]);
    }
    
    // Connect rings together
    int prev_ring_start = first_ring_start;
    int prev_ring_count = first_ring_count;
    
    for (int ring = 2; ring <= rings; ring++) {
        int ring_count = segments * ring / 2;
        if (ring_count < 6) ring_count = 6;
        int ring_start = prev_ring_start + prev_ring_count;
        
        // Connect vertices between rings
        for (int i = 0; i < ring_count; i++) {
            int next = (i + 1) % ring_count;
            
            // Find corresponding vertex in previous ring
            int prev_i = (i * prev_ring_count) / ring_count;
            int prev_next = ((i + 1) * prev_ring_count) / ring_count;
            if (prev_next >= prev_ring_count) prev_next = 0;
            
            // Triangle 1
            if (ring_start + i < (int)vertices.size() && prev_ring_start + prev_i < (int)vertices.size()) {
                st->set_normal(normals[prev_ring_start + prev_i]);
                st->add_vertex(vertices[prev_ring_start + prev_i]);
                
                st->set_normal(normals[ring_start + i]);
                st->add_vertex(vertices[ring_start + i]);
                
                st->set_normal(normals[ring_start + next]);
                st->add_vertex(vertices[ring_start + next]);
            }
            
            // Triangle 2 (when rings have different vertex counts)
            if (prev_i != prev_next && ring_start + next < (int)vertices.size()) {
                st->set_normal(normals[prev_ring_start + prev_i]);
                st->add_vertex(vertices[prev_ring_start + prev_i]);
                
                st->set_normal(normals[ring_start + next]);
                st->add_vertex(vertices[ring_start + next]);
                
                st->set_normal(normals[prev_ring_start + prev_next]);
                st->add_vertex(vertices[prev_ring_start + prev_next]);
            }
        }
        
        prev_ring_start = ring_start;
        prev_ring_count = ring_count;
    }
    
    st->generate_normals();
    return st->commit();
}

void TerrainGenerator::generate_snow_caps() {
    UtilityFunctions::print("TerrainGenerator: Generating snow caps...");
    
    // Create container for snow meshes
    snow_container = memnew(Node3D);
    snow_container->set_name("SnowCaps");
    add_child(snow_container);
    
    // Create snow material with realistic texture
    snow_material.instantiate();
    
    // Try to load snow texture
    Ref<ImageTexture> snow_tex = load_texture_from_file("res://assets/textures/terrain/snow/snow_albedo.jpg");
    Ref<ImageTexture> snow_norm = load_texture_from_file("res://assets/textures/terrain/snow/snow_normal.jpg");
    
    if (snow_tex.is_valid()) {
        snow_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, snow_tex);
        if (snow_norm.is_valid()) {
            snow_material->set_texture(StandardMaterial3D::TEXTURE_NORMAL, snow_norm);
            snow_material->set_feature(StandardMaterial3D::FEATURE_NORMAL_MAPPING, true);
        }
    } else {
        snow_material->set_albedo(Color(0.95f, 0.97f, 1.0f)); // Fallback slightly blue-white
    }
    
    snow_material->set_roughness(0.85f); // Matte snow surface
    snow_material->set_metallic(0.0f);
    snow_material->set_specular(0.3f);
    snow_material->set_uv1_scale(Vector3(0.3f, 0.3f, 0.3f)); // Scale for snow patches
    // Add subtle emission for that bright snow look
    snow_material->set_emission(Color(0.05f, 0.05f, 0.07f));
    snow_material->set_emission_energy_multiplier(0.1f);
    
    int size = config.map_size;
    float half_world = (size * config.tile_size) * 0.5f;
    
    // Find all high-elevation areas and create snow patches
    int snow_patches = 0;
    float scan_step = config.tile_size * 6.0f; // Scan every 6 tiles for snow coverage
    
    std::vector<Vector2> snow_centers;
    
    // First pass: identify mountain peaks and high areas
    for (float z = -half_world + scan_step; z < half_world - scan_step; z += scan_step) {
        for (float x = -half_world + scan_step; x < half_world - scan_step; x += scan_step) {
            float height = get_height_at(x, z);
            
            // Check if this area qualifies for snow
            if (height >= config.snow_start_height) {
                Vector3 normal = get_normal_at(x, z);
                float slope = 1.0f - normal.y;
                
                // Snow accumulates more on flatter surfaces
                float snow_threshold = config.snow_start_height + slope * 4.0f;
                
                if (height >= snow_threshold) {
                    // Check if too close to existing snow patch
                    bool too_close = false;
                    Vector2 new_pos(x, z);
                    for (const Vector2 &pos : snow_centers) {
                        if (pos.distance_to(new_pos) < scan_step * 0.8f) {
                            too_close = true;
                            break;
                        }
                    }
                    
                    if (!too_close) {
                        snow_centers.push_back(new_pos);
                    }
                }
            }
        }
    }
    
    // Second pass: create snow patch meshes
    for (const Vector2 &center : snow_centers) {
        float height = get_height_at(center.x, center.y);
        Vector3 normal = get_normal_at(center.x, center.y);
        float slope = 1.0f - normal.y;
        
        // Calculate snow patch size based on height and slope
        float height_factor = (height - config.snow_start_height) / (config.snow_full_height - config.snow_start_height);
        height_factor = Math::clamp(height_factor, 0.2f, 1.0f);
        
        float slope_factor = 1.0f - Math::clamp(slope * 1.5f, 0.0f, 0.7f);
        
        // Snow patch radius
        float base_radius = scan_step * 0.6f;
        float radius = base_radius * height_factor * slope_factor;
        
        if (radius < 2.0f) radius = 2.0f;
        
        // Create snow patch mesh
        Ref<ArrayMesh> snow_mesh = create_snow_patch_mesh(center.x, center.y, radius, 16);
        
        if (snow_mesh.is_valid()) {
            MeshInstance3D *snow_instance = memnew(MeshInstance3D);
            snow_instance->set_mesh(snow_mesh);
            snow_instance->set_surface_override_material(0, snow_material);
            snow_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            
            snow_container->add_child(snow_instance);
            snow_patches++;
        }
    }
    
    // Also create larger connected snow fields on the highest peaks
    // Find the highest points and create big snow blankets
    std::vector<std::pair<Vector2, float>> peak_candidates;
    
    for (float z = -half_world + scan_step * 2; z < half_world - scan_step * 2; z += scan_step * 2) {
        for (float x = -half_world + scan_step * 2; x < half_world - scan_step * 2; x += scan_step * 2) {
            float height = get_height_at(x, z);
            
            if (height >= config.snow_full_height * 0.85f) {
                // This is a high peak - check if it's a local maximum
                bool is_peak = true;
                float check_dist = scan_step;
                
                for (int dz = -1; dz <= 1 && is_peak; dz++) {
                    for (int dx = -1; dx <= 1 && is_peak; dx++) {
                        if (dx == 0 && dz == 0) continue;
                        float neighbor_height = get_height_at(x + dx * check_dist, z + dz * check_dist);
                        if (neighbor_height > height + 0.5f) {
                            is_peak = false;
                        }
                    }
                }
                
                if (is_peak) {
                    peak_candidates.push_back({Vector2(x, z), height});
                }
            }
        }
    }
    
    // Create large snow blankets on peaks
    for (const auto &peak : peak_candidates) {
        float large_radius = scan_step * 2.5f;
        
        // Create a larger, more detailed snow mesh for peaks
        Ref<ArrayMesh> peak_snow = create_snow_patch_mesh(peak.first.x, peak.first.y, large_radius, 24);
        
        if (peak_snow.is_valid()) {
            MeshInstance3D *snow_instance = memnew(MeshInstance3D);
            snow_instance->set_mesh(peak_snow);
            snow_instance->set_surface_override_material(0, snow_material);
            snow_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            
            snow_container->add_child(snow_instance);
            snow_patches++;
        }
    }
    
    UtilityFunctions::print("TerrainGenerator: Created ", snow_patches, " snow patches on mountain peaks");
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

Ref<ImageTexture> TerrainGenerator::load_texture_from_file(const String &path) {
    // Check if file exists
    if (!FileAccess::file_exists(path)) {
        UtilityFunctions::print("TerrainGenerator: Texture file not found: ", path);
        Ref<ImageTexture> empty;
        return empty;
    }
    
    // Load image from file
    Ref<Image> image = Image::load_from_file(path);
    if (!image.is_valid()) {
        UtilityFunctions::print("TerrainGenerator: Failed to load texture: ", path);
        Ref<ImageTexture> empty;
        return empty;
    }
    
    // Generate mipmaps for better quality at distance
    image->generate_mipmaps();
    
    // Create texture from image
    Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
    return texture;
}

void TerrainGenerator::load_terrain_textures() {
    UtilityFunctions::print("TerrainGenerator: Loading realistic terrain textures...");
    
    String base_path = "res://assets/textures/terrain/";
    
    // Load grass textures
    String grass_path = base_path + "grass/";
    grass_texture = load_texture_from_file(grass_path + "grass_albedo.jpg");
    grass_normal_texture = load_texture_from_file(grass_path + "grass_normal.jpg");
    grass_roughness_texture = load_texture_from_file(grass_path + "grass_roughness.jpg");
    grass_ao_texture = load_texture_from_file(grass_path + "grass_ao.jpg");
    
    // Load sand textures
    String sand_path = base_path + "sand/";
    sand_texture = load_texture_from_file(sand_path + "sand_albedo.jpg");
    sand_normal_texture = load_texture_from_file(sand_path + "sand_normal.jpg");
    sand_roughness_texture = load_texture_from_file(sand_path + "sand_roughness.jpg");
    sand_ao_texture = load_texture_from_file(sand_path + "sand_ao.jpg");
    
    // Load dirt textures
    String dirt_path = base_path + "dirt/";
    dirt_texture = load_texture_from_file(dirt_path + "dirt_albedo.jpg");
    dirt_normal_texture = load_texture_from_file(dirt_path + "dirt_normal.jpg");
    dirt_roughness_texture = load_texture_from_file(dirt_path + "dirt_roughness.jpg");
    dirt_ao_texture = load_texture_from_file(dirt_path + "dirt_ao.jpg");
    
    // Load rock textures
    String rock_path = base_path + "rock/";
    rock_texture = load_texture_from_file(rock_path + "rock_albedo.jpg");
    rock_normal_texture = load_texture_from_file(rock_path + "rock_normal.jpg");
    rock_roughness_texture = load_texture_from_file(rock_path + "rock_roughness.jpg");
    rock_ao_texture = load_texture_from_file(rock_path + "rock_ao.jpg");
    
    // Load snow textures
    String snow_path = base_path + "snow/";
    snow_texture = load_texture_from_file(snow_path + "snow_albedo.jpg");
    snow_normal_texture = load_texture_from_file(snow_path + "snow_normal.jpg");
    snow_roughness_texture = load_texture_from_file(snow_path + "snow_roughness.jpg");
    snow_ao_texture = load_texture_from_file(snow_path + "snow_ao.jpg");
    
    // Count loaded textures
    int loaded = 0;
    if (grass_texture.is_valid()) loaded++;
    if (sand_texture.is_valid()) loaded++;
    if (dirt_texture.is_valid()) loaded++;
    if (rock_texture.is_valid()) loaded++;
    if (snow_texture.is_valid()) loaded++;
    
    UtilityFunctions::print("TerrainGenerator: Loaded ", loaded, "/5 terrain texture sets");
    
    // Fall back to procedural textures if files not found
    if (loaded == 0) {
        UtilityFunctions::print("TerrainGenerator: No textures found, generating procedural fallback...");
        generate_procedural_textures();
    }
}

void TerrainGenerator::generate_procedural_textures() {
    UtilityFunctions::print("TerrainGenerator: Generating procedural fallback textures...");
    
    // Create noise-based textures for terrain (fallback if real textures not found)
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
    UtilityFunctions::print("TerrainGenerator: Setting up terrain shader with PBR textures...");
    
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
    
    // Set shader parameters - Texture settings
    terrain_shader_material->set_shader_parameter("texture_scale", 0.05f);
    terrain_shader_material->set_shader_parameter("normal_strength", 1.2f);
    terrain_shader_material->set_shader_parameter("ao_intensity", 0.6f);
    
    // Set shader parameters - Height blending thresholds
    terrain_shader_material->set_shader_parameter("water_level", config.water_level);
    terrain_shader_material->set_shader_parameter("sand_height_max", config.water_level + 2.0f);
    terrain_shader_material->set_shader_parameter("grass_height_max", 10.0f);
    terrain_shader_material->set_shader_parameter("rock_height_min", 8.0f);
    terrain_shader_material->set_shader_parameter("snow_height_min", config.snow_start_height);
    terrain_shader_material->set_shader_parameter("snow_height_full", config.snow_full_height);
    terrain_shader_material->set_shader_parameter("rock_slope_threshold", 0.5f);
    terrain_shader_material->set_shader_parameter("blend_sharpness", 6.0f);
    terrain_shader_material->set_shader_parameter("height_noise_scale", 0.25f);
    
    // Set shader parameters - Color adjustments  
    terrain_shader_material->set_shader_parameter("grass_tint", Vector3(0.9f, 1.0f, 0.85f));
    terrain_shader_material->set_shader_parameter("sand_tint", Vector3(1.0f, 0.95f, 0.85f));
    terrain_shader_material->set_shader_parameter("dirt_tint", Vector3(0.95f, 0.9f, 0.85f));
    terrain_shader_material->set_shader_parameter("rock_tint", Vector3(0.9f, 0.9f, 0.92f));
    terrain_shader_material->set_shader_parameter("snow_tint", Vector3(0.95f, 0.97f, 1.0f));
    terrain_shader_material->set_shader_parameter("saturation", 1.1f);
    terrain_shader_material->set_shader_parameter("brightness", 1.0f);
    
    // Set shader parameters - Material properties
    terrain_shader_material->set_shader_parameter("base_roughness", 0.6f);
    terrain_shader_material->set_shader_parameter("metallic", 0.0f);
    terrain_shader_material->set_shader_parameter("specular", 0.5f);
    
    // ============================================================
    // Set all albedo textures
    // ============================================================
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
    if (snow_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("snow_albedo", snow_texture);
    }
    
    // ============================================================
    // Set all normal maps
    // ============================================================
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
    if (snow_normal_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("snow_normal", snow_normal_texture);
    }
    
    // ============================================================
    // Set all roughness maps
    // ============================================================
    if (grass_roughness_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("grass_roughness_tex", grass_roughness_texture);
    }
    if (sand_roughness_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("sand_roughness_tex", sand_roughness_texture);
    }
    if (dirt_roughness_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("dirt_roughness_tex", dirt_roughness_texture);
    }
    if (rock_roughness_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("rock_roughness_tex", rock_roughness_texture);
    }
    if (snow_roughness_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("snow_roughness_tex", snow_roughness_texture);
    }
    
    // ============================================================
    // Set all ambient occlusion maps
    // ============================================================
    if (grass_ao_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("grass_ao_tex", grass_ao_texture);
    }
    if (sand_ao_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("sand_ao_tex", sand_ao_texture);
    }
    if (dirt_ao_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("dirt_ao_tex", dirt_ao_texture);
    }
    if (rock_ao_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("rock_ao_tex", rock_ao_texture);
    }
    if (snow_ao_texture.is_valid()) {
        terrain_shader_material->set_shader_parameter("snow_ao_tex", snow_ao_texture);
    }
    
    UtilityFunctions::print("TerrainGenerator: Terrain PBR shader setup complete");
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
