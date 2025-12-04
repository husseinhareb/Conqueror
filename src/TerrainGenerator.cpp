/**
 * TerrainGenerator.cpp
 * Procedural terrain generation implementation.
 */

#include "TerrainGenerator.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

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
    UtilityFunctions::print("TerrainGenerator: Height range: min=", min_h, " max=", max_h, " avg=", avg_h, " water_level=", config.water_level);
    
    // Create visual mesh and collision
    create_terrain_mesh();
    create_terrain_collision();
    create_water_plane();
    apply_terrain_material();
    
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
    terrain_collision = nullptr;
    heightmap.clear();
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
    
    // Water level as normalized value - but we need lakes to go TO water level
    // If water_level is negative, lakes need to carve down to 0 or slightly below
    float lake_bottom = Math::max(0.0f, config.water_level / config.max_height);
    
    // Use seed to determine lake positions - keep lakes small and scattered
    for (int i = 0; i < config.lake_count; i++) {
        // Pseudo-random lake center based on seed
        int hash1 = (config.seed * (i + 1) * 16807) % 2147483647;
        int hash2 = (hash1 * 16807) % 2147483647;
        int hash3 = (hash2 * 16807) % 2147483647;
        
        float lake_x = (float)(hash1 % size);
        float lake_z = (float)(hash2 % size);
        
        // Keep lakes away from edges and center (leave center for base building)
        lake_x = Math::clamp(lake_x, size * 0.25f, size * 0.75f);
        lake_z = Math::clamp(lake_z, size * 0.25f, size * 0.75f);
        
        // Avoid placing lakes too close to map center (player start area)
        float center_dist = sqrt(pow(lake_x - half_size, 2) + pow(lake_z - half_size, 2));
        if (center_dist < size * 0.2f) {
            // Push lake away from center
            float angle = atan2(lake_z - half_size, lake_x - half_size);
            lake_x = half_size + cos(angle) * size * 0.3f;
            lake_z = half_size + sin(angle) * size * 0.3f;
        }
        
        // Lake size varies but stays within limits
        float size_variation = (float)(hash3 % 100) / 100.0f; // 0-1
        float lake_radius = config.lake_size + size_variation * (config.lake_max_size - config.lake_size);
        lake_radius = Math::min(lake_radius, config.lake_max_size); // Enforce max size
        
        // Carve the lake - create a depression at water level
        for (int z = 0; z < size; z++) {
            for (int x = 0; x < size; x++) {
                float dx = x - lake_x;
                float dz = z - lake_z;
                float dist = sqrt(dx * dx + dz * dz);
                
                if (dist < lake_radius) {
                    // Smooth falloff from edge to center
                    float depth_factor = 1.0f - (dist / lake_radius);
                    depth_factor = depth_factor * depth_factor; // Quadratic for smooth bowl
                    
                    float current = heightmap[z * size + x];
                    // Target is at lake_bottom (at or slightly above water_level)
                    float target = lake_bottom + 0.01f;
                    
                    heightmap[z * size + x] = lerp(current, target, depth_factor);
                }
            }
        }
    }
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
            
            // Calculate normal
            Vector3 normal = get_normal_at(wx + half_world, wz + half_world);
            
            // Color based on height (for basic visualization)
            Color color;
            float normalized_height = height / config.max_height;
            if (height <= config.water_level) {
                color = Color(0.1f, 0.2f, 0.6f); // Deep water blue
            } else if (normalized_height < 0.15f) {
                // Low ground - bright grass green
                color = Color(0.2f, 0.6f, 0.15f);
            } else if (normalized_height < 0.4f) {
                // Medium ground - grass/dirt mix
                color = Color(0.3f, 0.5f, 0.2f);
            } else if (normalized_height < 0.65f) {
                // Higher ground - dirt/rock
                color = Color(0.5f, 0.4f, 0.3f);
            } else if (normalized_height < 0.85f) {
                // Mountain - rock gray
                color = Color(0.5f, 0.5f, 0.5f);
            } else {
                // Peak - snow white
                color = Color(0.95f, 0.95f, 0.95f);
            }
            
            st->set_color(color);
            st->set_uv(Vector2(u, v));
            st->set_normal(normal);
            st->add_vertex(Vector3(wx, height, wz));
        }
    }
    
    // Create triangles
    for (int z = 0; z < size - 1; z++) {
        for (int x = 0; x < size - 1; x++) {
            int i = z * size + x;
            
            // First triangle
            st->add_index(i);
            st->add_index(i + size);
            st->add_index(i + 1);
            
            // Second triangle
            st->add_index(i + 1);
            st->add_index(i + size);
            st->add_index(i + size + 1);
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
    // Only create water plane if water level is positive (visible)
    if (config.water_level < 0) {
        // No global water plane - lakes will just be depressions
        return;
    }
    
    int size = config.map_size;
    float world_size = size * config.tile_size;
    
    // Create water plane mesh
    Ref<PlaneMesh> water_plane;
    water_plane.instantiate();
    water_plane->set_size(Vector2(world_size, world_size));
    water_plane->set_subdivide_width(4);
    water_plane->set_subdivide_depth(4);
    
    // Create material for water
    Ref<StandardMaterial3D> water_mat;
    water_mat.instantiate();
    water_mat->set_albedo(Color(0.2f, 0.4f, 0.6f, 0.7f));
    water_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    water_mat->set_roughness(0.1f);
    water_mat->set_metallic(0.3f);
    water_mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    
    water_mesh = memnew(MeshInstance3D);
    water_mesh->set_mesh(water_plane);
    water_mesh->set_surface_override_material(0, water_mat);
    water_mesh->set_position(Vector3(0, config.water_level, 0));
    water_mesh->set_name("WaterPlane");
    add_child(water_mesh);
}

void TerrainGenerator::apply_terrain_material() {
    if (!terrain_mesh) return;
    
    // Create a basic material with vertex colors
    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_roughness(0.9f);
    mat->set_metallic(0.0f);
    
    terrain_mesh->set_surface_override_material(0, mat);
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

} // namespace rts
