shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

// ============================================================================
// REALISTIC TERRAIN SHADER WITH PBR TEXTURES
// Features: Grass, Sand, Dirt, Rock, Snow with height/slope blending
// ============================================================================

// Terrain texture layers - Albedo maps
group_uniforms albedo_textures;
uniform sampler2D grass_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D sand_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D dirt_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D rock_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D snow_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;

// Normal maps
group_uniforms normal_textures;
uniform sampler2D grass_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D sand_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D dirt_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D rock_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D snow_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;

// Roughness maps  
group_uniforms roughness_textures;
uniform sampler2D grass_roughness_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D sand_roughness_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D dirt_roughness_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D rock_roughness_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D snow_roughness_tex : filter_linear_mipmap_anisotropic, repeat_enable;

// AO maps
group_uniforms ao_textures;
uniform sampler2D grass_ao_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D sand_ao_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D dirt_ao_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D rock_ao_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D snow_ao_tex : filter_linear_mipmap_anisotropic, repeat_enable;

// Texture parameters
group_uniforms texture_settings;
uniform float texture_scale : hint_range(0.01, 0.5) = 0.08;
uniform float detail_scale : hint_range(0.1, 2.0) = 0.5;
uniform float normal_strength : hint_range(0.0, 2.0) = 1.0;
uniform float ao_intensity : hint_range(0.0, 1.0) = 0.5;

// Height-based blending thresholds
group_uniforms terrain_blend;
uniform float water_level : hint_range(-20.0, 20.0) = 1.0;
uniform float sand_height_max : hint_range(-10.0, 20.0) = 2.5;
uniform float grass_height_max : hint_range(0.0, 50.0) = 12.0;
uniform float rock_height_min : hint_range(0.0, 50.0) = 8.0;
uniform float snow_height_min : hint_range(0.0, 100.0) = 10.0;
uniform float snow_height_full : hint_range(0.0, 100.0) = 16.0;
uniform float rock_slope_threshold : hint_range(0.0, 1.0) = 0.5;
uniform float blend_sharpness : hint_range(1.0, 20.0) = 6.0;
uniform float height_noise_scale : hint_range(0.0, 1.0) = 0.3;

// Color adjustments
group_uniforms color_adjustments;
uniform vec3 grass_tint : source_color = vec3(0.9, 1.0, 0.85);
uniform vec3 sand_tint : source_color = vec3(1.0, 0.95, 0.85);
uniform vec3 dirt_tint : source_color = vec3(0.95, 0.9, 0.85);
uniform vec3 rock_tint : source_color = vec3(0.9, 0.9, 0.92);
uniform vec3 snow_tint : source_color = vec3(0.95, 0.97, 1.0);
uniform float saturation : hint_range(0.0, 2.0) = 1.1;
uniform float brightness : hint_range(0.5, 2.0) = 1.0;

// Material properties
group_uniforms material;
uniform float base_roughness : hint_range(0.0, 1.0) = 0.5;
uniform float metallic : hint_range(0.0, 1.0) = 0.0;
uniform float specular : hint_range(0.0, 1.0) = 0.5;

// Varying for world position and normal
varying vec3 world_position;
varying vec3 world_normal;
varying float vertex_height;

void vertex() {
    world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
    vertex_height = world_position.y;
}

// Triplanar mapping for seamless textures on all surfaces
vec4 triplanar_texture(sampler2D tex, vec3 world_pos, vec3 normal, float scale) {
    vec3 blend_weights = abs(normal);
    blend_weights = pow(blend_weights, vec3(4.0));
    blend_weights /= dot(blend_weights, vec3(1.0)) + 0.0001;
    
    vec4 x_proj = texture(tex, world_pos.zy * scale);
    vec4 y_proj = texture(tex, world_pos.xz * scale);
    vec4 z_proj = texture(tex, world_pos.xy * scale);
    
    return x_proj * blend_weights.x + y_proj * blend_weights.y + z_proj * blend_weights.z;
}

// Hash noise for variation
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth noise for blending
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Multi-octave noise
float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// Smooth height blend with noise
float smooth_blend(float value, float edge_min, float edge_max, vec2 world_xz) {
    float noise_offset = (fbm(world_xz * 0.02, 3) - 0.5) * height_noise_scale * (edge_max - edge_min);
    return smoothstep(edge_min + noise_offset, edge_max + noise_offset, value);
}

// Adjust saturation of color
vec3 adjust_saturation(vec3 color, float sat) {
    float gray = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(gray), color, sat);
}

void fragment() {
    vec3 normal = normalize(world_normal);
    float slope = 1.0 - abs(normal.y); // 0 = flat, 1 = vertical
    float height = vertex_height;
    vec2 world_xz = world_position.xz;
    
    // Noise for blending variation
    float blend_noise = fbm(world_xz * 0.05, 4);
    
    // ========================================================================
    // SAMPLE ALL TEXTURES WITH TRIPLANAR MAPPING
    // ========================================================================
    
    // Grass
    vec4 grass_col = triplanar_texture(grass_albedo, world_position, normal, texture_scale);
    vec3 grass_norm = triplanar_texture(grass_normal, world_position, normal, texture_scale).rgb * 2.0 - 1.0;
    float grass_rough = triplanar_texture(grass_roughness_tex, world_position, normal, texture_scale).r;
    float grass_ao = triplanar_texture(grass_ao_tex, world_position, normal, texture_scale).r;
    grass_col.rgb *= grass_tint;
    
    // Sand
    vec4 sand_col = triplanar_texture(sand_albedo, world_position, normal, texture_scale);
    vec3 sand_norm = triplanar_texture(sand_normal, world_position, normal, texture_scale).rgb * 2.0 - 1.0;
    float sand_rough = triplanar_texture(sand_roughness_tex, world_position, normal, texture_scale).r;
    float sand_ao = triplanar_texture(sand_ao_tex, world_position, normal, texture_scale).r;
    sand_col.rgb *= sand_tint;
    
    // Dirt
    vec4 dirt_col = triplanar_texture(dirt_albedo, world_position, normal, texture_scale);
    vec3 dirt_norm = triplanar_texture(dirt_normal, world_position, normal, texture_scale).rgb * 2.0 - 1.0;
    float dirt_rough = triplanar_texture(dirt_roughness_tex, world_position, normal, texture_scale).r;
    float dirt_ao = triplanar_texture(dirt_ao_tex, world_position, normal, texture_scale).r;
    dirt_col.rgb *= dirt_tint;
    
    // Rock
    vec4 rock_col = triplanar_texture(rock_albedo, world_position, normal, texture_scale * 0.7);
    vec3 rock_norm = triplanar_texture(rock_normal, world_position, normal, texture_scale * 0.7).rgb * 2.0 - 1.0;
    float rock_rough = triplanar_texture(rock_roughness_tex, world_position, normal, texture_scale * 0.7).r;
    float rock_ao = triplanar_texture(rock_ao_tex, world_position, normal, texture_scale * 0.7).r;
    rock_col.rgb *= rock_tint;
    
    // Snow
    vec4 snow_col = triplanar_texture(snow_albedo, world_position, normal, texture_scale * 1.2);
    vec3 snow_norm = triplanar_texture(snow_normal, world_position, normal, texture_scale * 1.2).rgb * 2.0 - 1.0;
    float snow_rough = triplanar_texture(snow_roughness_tex, world_position, normal, texture_scale * 1.2).r;
    float snow_ao = triplanar_texture(snow_ao_tex, world_position, normal, texture_scale * 1.2).r;
    snow_col.rgb *= snow_tint;
    
    // ========================================================================
    // CALCULATE BLEND WEIGHTS
    // ========================================================================
    
    // Base layer selection based on height
    float sand_weight = 1.0 - smooth_blend(height, water_level, sand_height_max, world_xz);
    float grass_weight = smooth_blend(height, water_level + 1.0, grass_height_max * 0.5, world_xz) * 
                        (1.0 - smooth_blend(height, grass_height_max * 0.7, grass_height_max, world_xz));
    float dirt_weight = smooth_blend(height, grass_height_max * 0.6, rock_height_min, world_xz) *
                       (1.0 - smooth_blend(height, rock_height_min * 0.8, rock_height_min * 1.2, world_xz));
    
    // Rock appears on steep slopes at any height
    float rock_slope_weight = smooth_blend(slope, rock_slope_threshold - 0.1, rock_slope_threshold + 0.2, world_xz);
    float rock_height_weight = smooth_blend(height, rock_height_min * 0.8, rock_height_min * 1.5, world_xz);
    float rock_weight = max(rock_slope_weight * 0.8, rock_height_weight * 0.6);
    
    // Snow on high elevations (less on steep slopes)
    float snow_slope_factor = 1.0 - slope * 0.7; // Less snow on slopes
    float snow_weight = smooth_blend(height, snow_height_min, snow_height_full, world_xz) * snow_slope_factor;
    
    // Add noise variation to weights
    sand_weight *= (0.8 + blend_noise * 0.4);
    grass_weight *= (0.7 + blend_noise * 0.6);
    dirt_weight *= (0.8 + (1.0 - blend_noise) * 0.4);
    rock_weight = min(rock_weight * (0.9 + blend_noise * 0.2), 1.0);
    snow_weight *= (0.85 + blend_noise * 0.3);
    
    // Ensure weights don't exceed 1.0
    sand_weight = clamp(sand_weight, 0.0, 1.0);
    grass_weight = clamp(grass_weight, 0.0, 1.0);
    dirt_weight = clamp(dirt_weight, 0.0, 1.0);
    rock_weight = clamp(rock_weight, 0.0, 1.0);
    snow_weight = clamp(snow_weight, 0.0, 1.0);
    
    // Apply rock weight to reduce others
    float non_rock = 1.0 - rock_weight;
    sand_weight *= non_rock;
    grass_weight *= non_rock;
    dirt_weight *= non_rock;
    
    // Snow on top of everything
    float non_snow = 1.0 - snow_weight;
    sand_weight *= non_snow;
    grass_weight *= non_snow;
    dirt_weight *= non_snow;
    rock_weight *= non_snow;
    
    // Normalize weights
    float total = sand_weight + grass_weight + dirt_weight + rock_weight + snow_weight;
    if (total > 0.001) {
        sand_weight /= total;
        grass_weight /= total;
        dirt_weight /= total;
        rock_weight /= total;
        snow_weight /= total;
    } else {
        // Default to grass
        grass_weight = 1.0;
    }
    
    // ========================================================================
    // BLEND ALL PROPERTIES
    // ========================================================================
    
    // Albedo blend
    vec3 final_color = grass_col.rgb * grass_weight +
                       sand_col.rgb * sand_weight +
                       dirt_col.rgb * dirt_weight +
                       rock_col.rgb * rock_weight +
                       snow_col.rgb * snow_weight;
    
    // Normal blend (using reoriented normal mapping for better quality)
    vec3 final_normal = normalize(
        grass_norm * grass_weight +
        sand_norm * sand_weight +
        dirt_norm * dirt_weight +
        rock_norm * rock_weight +
        snow_norm * snow_weight
    );
    
    // Roughness blend
    float final_roughness = grass_rough * grass_weight +
                           sand_rough * sand_weight +
                           dirt_rough * dirt_weight +
                           rock_rough * rock_weight +
                           snow_rough * snow_weight;
    final_roughness = mix(base_roughness, final_roughness, 0.8);
    
    // AO blend
    float final_ao = grass_ao * grass_weight +
                    sand_ao * sand_weight +
                    dirt_ao * dirt_weight +
                    rock_ao * rock_weight +
                    snow_ao * snow_weight;
    final_ao = mix(1.0, final_ao, ao_intensity);
    
    // ========================================================================
    // ADDITIONAL EFFECTS
    // ========================================================================
    
    // Height-based ambient occlusion (darker in valleys)
    float height_ao = smoothstep(-2.0, 8.0, height);
    final_ao *= 0.8 + height_ao * 0.2;
    
    // Micro-detail variation
    float micro_detail = fbm(world_xz * 2.0, 2) * 0.1 + 0.95;
    final_color *= micro_detail;
    
    // Apply saturation and brightness adjustments
    final_color = adjust_saturation(final_color, saturation);
    final_color *= brightness;
    
    // Apply AO
    final_color *= final_ao;
    
    // ========================================================================
    // OUTPUT
    // ========================================================================
    
    ALBEDO = final_color;
    NORMAL_MAP = final_normal * 0.5 + 0.5;
    NORMAL_MAP_DEPTH = normal_strength;
    ROUGHNESS = final_roughness;
    METALLIC = metallic;
    SPECULAR = specular;
    AO = final_ao;
}
