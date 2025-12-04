shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

// Terrain texture layers
group_uniforms textures;
uniform sampler2D grass_albedo : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D grass_normal : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D sand_albedo : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D sand_normal : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D dirt_albedo : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D dirt_normal : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D rock_albedo : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D rock_normal : hint_normal, filter_linear_mipmap, repeat_enable;

// Texture parameters
group_uniforms texture_settings;
uniform float texture_scale : hint_range(0.01, 1.0) = 0.1;
uniform float normal_strength : hint_range(0.0, 2.0) = 1.0;

// Terrain blend parameters
group_uniforms terrain_blend;
uniform float grass_height_min : hint_range(-50.0, 50.0) = 0.0;
uniform float grass_height_max : hint_range(-50.0, 100.0) = 15.0;
uniform float sand_height_min : hint_range(-50.0, 50.0) = -5.0;
uniform float sand_height_max : hint_range(-50.0, 50.0) = 3.0;
uniform float rock_slope_threshold : hint_range(0.0, 1.0) = 0.7;
uniform float blend_sharpness : hint_range(1.0, 20.0) = 8.0;

// Material properties
group_uniforms material;
uniform float grass_roughness : hint_range(0.0, 1.0) = 0.85;
uniform float sand_roughness : hint_range(0.0, 1.0) = 0.95;
uniform float dirt_roughness : hint_range(0.0, 1.0) = 0.9;
uniform float rock_roughness : hint_range(0.0, 1.0) = 0.75;

// Additional effects
group_uniforms effects;
uniform float ao_strength : hint_range(0.0, 1.0) = 0.3;
uniform vec3 grass_tint : source_color = vec3(0.85, 1.0, 0.85);
uniform vec3 sand_tint : source_color = vec3(1.0, 0.95, 0.85);

// Varying for world position and normal
varying vec3 world_position;
varying vec3 world_normal;
varying float vertex_height;

void vertex() {
    world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
    vertex_height = world_position.y;
}

// Triplanar mapping for seamless textures on slopes
vec4 triplanar_texture(sampler2D tex, vec3 world_pos, vec3 normal, float scale) {
    vec3 blend_weights = abs(normal);
    blend_weights = pow(blend_weights, vec3(4.0));
    blend_weights /= dot(blend_weights, vec3(1.0));
    
    vec4 x_proj = texture(tex, world_pos.zy * scale);
    vec4 y_proj = texture(tex, world_pos.xz * scale);
    vec4 z_proj = texture(tex, world_pos.xy * scale);
    
    return x_proj * blend_weights.x + y_proj * blend_weights.y + z_proj * blend_weights.z;
}

// Height-based blend factor with smooth transitions
float height_blend(float height, float min_h, float max_h, float sharpness) {
    float mid = (min_h + max_h) * 0.5;
    float range = (max_h - min_h) * 0.5;
    float t = clamp((height - mid) / range, -1.0, 1.0);
    return smoothstep(-1.0, 1.0, t * sharpness / blend_sharpness);
}

void fragment() {
    vec3 normal = normalize(world_normal);
    float slope = 1.0 - abs(normal.y); // 0 = flat, 1 = vertical
    float height = vertex_height;
    
    // Calculate texture coordinates with triplanar mapping
    vec2 uv = world_position.xz * texture_scale;
    
    // Sample all textures
    vec4 grass_color = triplanar_texture(grass_albedo, world_position, normal, texture_scale);
    vec4 sand_color = triplanar_texture(sand_albedo, world_position, normal, texture_scale);
    vec4 dirt_color = triplanar_texture(dirt_albedo, world_position, normal, texture_scale);
    vec4 rock_color = triplanar_texture(rock_albedo, world_position, normal, texture_scale);
    
    // Apply tints
    grass_color.rgb *= grass_tint;
    sand_color.rgb *= sand_tint;
    
    // Sample normal maps
    vec3 grass_norm = triplanar_texture(grass_normal, world_position, normal, texture_scale).rgb * 2.0 - 1.0;
    vec3 sand_norm = triplanar_texture(sand_normal, world_position, normal, texture_scale).rgb * 2.0 - 1.0;
    vec3 dirt_norm = triplanar_texture(dirt_normal, world_position, normal, texture_scale).rgb * 2.0 - 1.0;
    vec3 rock_norm = triplanar_texture(rock_normal, world_position, normal, texture_scale).rgb * 2.0 - 1.0;
    
    // Calculate blend weights based on height and slope
    float sand_weight = smoothstep(sand_height_max, sand_height_min, height);
    float grass_weight = smoothstep(grass_height_min, grass_height_max, height) * (1.0 - sand_weight);
    float rock_weight = smoothstep(rock_slope_threshold - 0.1, rock_slope_threshold + 0.1, slope);
    float dirt_weight = 1.0 - grass_weight - sand_weight;
    dirt_weight = max(dirt_weight, 0.0);
    
    // Rock overrides based on slope
    float non_rock = 1.0 - rock_weight;
    sand_weight *= non_rock;
    grass_weight *= non_rock;
    dirt_weight *= non_rock;
    
    // Add dirt in transition zones
    float transition_noise = fract(sin(dot(world_position.xz, vec2(12.9898, 78.233))) * 43758.5453);
    dirt_weight += (1.0 - abs(grass_weight - 0.5) * 2.0) * 0.3 * transition_noise * non_rock;
    
    // Normalize weights
    float total = sand_weight + grass_weight + dirt_weight + rock_weight;
    if (total > 0.0) {
        sand_weight /= total;
        grass_weight /= total;
        dirt_weight /= total;
        rock_weight /= total;
    }
    
    // Blend colors
    vec3 final_color = grass_color.rgb * grass_weight +
                       sand_color.rgb * sand_weight +
                       dirt_color.rgb * dirt_weight +
                       rock_color.rgb * rock_weight;
    
    // Blend normals
    vec3 final_normal = normalize(
        grass_norm * grass_weight +
        sand_norm * sand_weight +
        dirt_norm * dirt_weight +
        rock_norm * rock_weight
    );
    
    // Blend roughness
    float final_roughness = grass_roughness * grass_weight +
                           sand_roughness * sand_weight +
                           dirt_roughness * dirt_weight +
                           rock_roughness * rock_weight;
    
    // Simple ambient occlusion based on height variation
    float ao = 1.0 - ao_strength * (1.0 - smoothstep(-2.0, 5.0, height));
    
    // Output
    ALBEDO = final_color * ao;
    NORMAL_MAP = final_normal * 0.5 + 0.5;
    NORMAL_MAP_DEPTH = normal_strength;
    ROUGHNESS = final_roughness;
    METALLIC = 0.0;
}
