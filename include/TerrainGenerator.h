/**
 * TerrainGenerator.h
 * Procedural terrain generation for RTS maps.
 * Creates heightmaps with mountains, valleys, lakes, and texture blending.
 */

#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/height_map_shape3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <vector>

namespace rts {

/**
 * Terrain biome types for texture blending
 */
enum class TerrainBiome {
    GRASS,
    DIRT,
    ROCK,
    SAND,
    SNOW,
    WATER
};

/**
 * Configuration for terrain generation
 */
struct TerrainConfig {
    // Size settings
    int map_size = 256;              // Width/height in vertices
    float tile_size = 1.0f;          // World units per tile
    
    // Height settings
    float max_height = 15.0f;        // Maximum terrain height
    float water_level = -2.0f;       // Water surface height (below ground!)
    float snow_level = 14.0f;        // Height where snow starts
    float ground_level = 1.0f;       // Base ground level (well above water)
    
    // Noise settings for terrain shape
    float base_frequency = 0.01f;    // Base noise frequency (lower = smoother)
    float mountain_frequency = 0.006f; // Mountain noise frequency
    float detail_frequency = 0.1f;   // Fine detail frequency
    
    float base_amplitude = 0.3f;     // Base terrain contribution (more variation)
    float mountain_amplitude = 0.6f; // Mountain contribution
    float detail_amplitude = 0.05f;  // Detail contribution
    
    int octaves = 4;                 // Noise octaves for detail
    float persistence = 0.5f;        // Octave amplitude falloff
    float lacunarity = 2.0f;         // Octave frequency increase
    
    // Feature settings
    float mountain_threshold = 0.55f; // Height threshold for mountains (lower = more mountains)
    float lake_threshold = 0.25f;    // Height threshold for lakes
    int lake_count = 4;              // Number of lakes to carve
    float lake_size = 40.0f;         // Average lake radius (large natural lakes)
    float lake_max_size = 80.0f;     // Maximum lake size
    
    // Tree settings
    int tree_count = 2000;           // Number of trees to spawn
    float tree_min_height = 2.0f;    // Minimum terrain height for trees
    float tree_max_height = 12.0f;   // Maximum terrain height for trees
    float tree_max_slope = 0.4f;     // Maximum slope for tree placement
    float tree_min_spacing = 5.0f;   // Minimum distance between trees
    float tree_center_clear = 50.0f; // Keep center clear for player base
    
    // Random seed
    int seed = 12345;
};

/**
 * TerrainGenerator - Creates procedural terrain meshes
 */
class TerrainGenerator : public godot::Node3D {
    GDCLASS(TerrainGenerator, godot::Node3D)

private:
    // Configuration
    TerrainConfig config;
    
    // Generated data
    godot::PackedFloat32Array heightmap;
    godot::Ref<godot::Image> heightmap_image;
    godot::Ref<godot::Image> normalmap_image;
    godot::Ref<godot::Image> splatmap_image;  // RGBA for texture blending
    
    // Scene nodes
    godot::MeshInstance3D *terrain_mesh = nullptr;
    godot::StaticBody3D *terrain_body = nullptr;
    godot::CollisionShape3D *terrain_collision = nullptr;
    godot::MeshInstance3D *water_mesh = nullptr;
    godot::Node3D *trees_container = nullptr;
    godot::Node3D *lakes_container = nullptr;
    
    // Lake data (stored during carve_lakes for water plane creation)
    struct LakeData {
        float world_x, world_z;
        float radius;
        float water_height;
    };
    std::vector<LakeData> lake_positions;
    
    // Tree meshes (shared across all trees)
    godot::Ref<godot::ArrayMesh> tree_mesh;
    godot::Ref<godot::StandardMaterial3D> tree_trunk_material;
    godot::Ref<godot::StandardMaterial3D> tree_leaves_material;
    
    // Grass system
    godot::MultiMeshInstance3D *grass_instance = nullptr;
    godot::Ref<godot::MultiMesh> grass_multimesh;
    godot::Ref<godot::ArrayMesh> grass_blade_mesh;
    godot::Ref<godot::ShaderMaterial> grass_material;
    
    // Environment
    godot::WorldEnvironment *world_environment = nullptr;
    godot::DirectionalLight3D *sun_light = nullptr;
    godot::Ref<godot::Environment> environment;
    godot::Ref<godot::Sky> sky;
    
    // Terrain shader material
    godot::Ref<godot::ShaderMaterial> terrain_shader_material;
    godot::Ref<godot::Shader> terrain_shader;
    
    // Textures
    godot::Ref<godot::ImageTexture> heightmap_texture;
    godot::Ref<godot::ImageTexture> normalmap_texture;
    godot::Ref<godot::ImageTexture> splatmap_texture;
    
    // Procedural textures
    godot::Ref<godot::ImageTexture> grass_texture;
    godot::Ref<godot::ImageTexture> grass_normal_texture;
    godot::Ref<godot::ImageTexture> sand_texture;
    godot::Ref<godot::ImageTexture> sand_normal_texture;
    godot::Ref<godot::ImageTexture> dirt_texture;
    godot::Ref<godot::ImageTexture> dirt_normal_texture;
    godot::Ref<godot::ImageTexture> rock_texture;
    godot::Ref<godot::ImageTexture> rock_normal_texture;
    
    // Noise generation helpers
    float noise2d(float x, float y, int seed);
    float fbm_noise(float x, float y, int octaves, float persistence, float lacunarity, float frequency, int seed);
    float smoothstep(float edge0, float edge1, float x);
    float lerp(float a, float b, float t);
    
    // Terrain generation steps
    void generate_base_heightmap();
    void apply_mountains();
    void carve_lakes();
    void smooth_terrain(int iterations);
    void generate_normalmap();
    void generate_splatmap();
    
    // Mesh creation
    void create_terrain_mesh();
    void create_terrain_collision();
    void create_water_plane();
    godot::Ref<godot::ArrayMesh> create_irregular_lake_mesh(float radius, int radial_segments, int rings, int seed);
    void apply_terrain_material();
    void generate_trees();
    void create_tree_mesh();
    bool is_valid_tree_position(float x, float z) const;
    
    // Grass system
    void generate_grass();
    godot::Ref<godot::ArrayMesh> create_grass_blade_mesh();
    bool is_valid_grass_position(float x, float z) const;
    
    // Texture generation
    void generate_procedural_textures();
    godot::Ref<godot::ImageTexture> create_noise_texture(int size, godot::Color base_color, godot::Color variation_color, float frequency, int seed);
    godot::Ref<godot::ImageTexture> create_normal_from_height(godot::Ref<godot::ImageTexture> height_tex, float strength);
    
    // Shader setup
    void setup_terrain_shader();
    
    // Environment setup
    void setup_environment();

protected:
    static void _bind_methods();

public:
    TerrainGenerator();
    ~TerrainGenerator();

    void _ready() override;
    
    // Main generation function
    void generate_terrain();
    void generate_terrain_with_seed(int seed);
    void clear_terrain();
    
    // Height queries
    float get_height_at(float x, float z) const;
    godot::Vector3 get_normal_at(float x, float z) const;
    bool is_water_at(float x, float z) const;
    bool is_buildable_at(float x, float z) const;
    bool is_within_bounds(float x, float z) const;
    
    // Configuration setters/getters
    void set_map_size(int size);
    int get_map_size() const;
    
    void set_tile_size(float size);
    float get_tile_size() const;
    
    void set_max_height(float height);
    float get_max_height() const;
    
    void set_water_level(float level);
    float get_water_level() const;
    
    void set_seed(int seed);
    int get_seed() const;
    
    void set_mountain_frequency(float freq);
    float get_mountain_frequency() const;
    
    void set_lake_count(int count);
    int get_lake_count() const;
    
    // Get world bounds
    float get_world_size() const;
    godot::Vector3 get_world_center() const;
};

} // namespace rts

#endif // TERRAIN_GENERATOR_H
