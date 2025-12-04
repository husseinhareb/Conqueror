shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_disabled, diffuse_burley, specular_schlick_ggx;

// ============================================================================
// REALISTIC WATER SHADER WITH GERSTNER WAVE PHYSICS
// ============================================================================

// Water colors
uniform vec3 water_color_shallow : source_color = vec3(0.12, 0.42, 0.55);
uniform vec3 water_color_deep : source_color = vec3(0.02, 0.12, 0.22);
uniform vec3 water_color_fresnel : source_color = vec3(0.15, 0.35, 0.45);

// Gerstner wave parameters (4 wave layers)
uniform float wave_speed : hint_range(0.0, 3.0) = 0.8;
uniform float wave_scale : hint_range(0.001, 0.1) = 0.02;

// Wave 1 - Large swell
uniform float wave1_amplitude : hint_range(0.0, 2.0) = 0.4;
uniform float wave1_frequency : hint_range(0.01, 1.0) = 0.12;
uniform float wave1_steepness : hint_range(0.0, 1.0) = 0.5;
uniform vec2 wave1_direction = vec2(1.0, 0.3);

// Wave 2 - Medium waves
uniform float wave2_amplitude : hint_range(0.0, 1.0) = 0.25;
uniform float wave2_frequency : hint_range(0.01, 2.0) = 0.22;
uniform float wave2_steepness : hint_range(0.0, 1.0) = 0.4;
uniform vec2 wave2_direction = vec2(0.7, 0.7);

// Wave 3 - Small choppy waves
uniform float wave3_amplitude : hint_range(0.0, 0.5) = 0.12;
uniform float wave3_frequency : hint_range(0.1, 3.0) = 0.45;
uniform float wave3_steepness : hint_range(0.0, 1.0) = 0.35;
uniform vec2 wave3_direction = vec2(-0.4, 0.9);

// Wave 4 - Micro ripples
uniform float wave4_amplitude : hint_range(0.0, 0.3) = 0.06;
uniform float wave4_frequency : hint_range(0.2, 5.0) = 0.9;
uniform float wave4_steepness : hint_range(0.0, 1.0) = 0.25;
uniform vec2 wave4_direction = vec2(0.9, -0.4);

// Surface properties
uniform float roughness : hint_range(0.0, 1.0) = 0.05;
uniform float metallic : hint_range(0.0, 1.0) = 0.1;
uniform float opacity : hint_range(0.0, 1.0) = 0.95;

// Fresnel
uniform float fresnel_power : hint_range(1.0, 10.0) = 4.0;
uniform float fresnel_bias : hint_range(0.0, 0.5) = 0.04;

// Reflection & Refraction
uniform float reflection_strength : hint_range(0.0, 1.0) = 0.8;
uniform vec3 sky_color : source_color = vec3(0.5, 0.7, 0.95);
uniform float refraction_strength : hint_range(0.0, 0.5) = 0.1;

// Normal map textures (for detail)
uniform sampler2D normal_map_1 : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D normal_map_2 : hint_normal, filter_linear_mipmap, repeat_enable;
uniform float normal_map_scale : hint_range(0.01, 0.2) = 0.05;
uniform float normal_map_strength : hint_range(0.0, 2.0) = 0.6;

// Foam
uniform float foam_amount : hint_range(0.0, 1.0) = 0.3;
uniform float foam_cutoff : hint_range(0.0, 1.0) = 0.7;
uniform vec3 foam_color : source_color = vec3(0.95, 0.98, 1.0);

// Subsurface scattering
uniform float sss_strength : hint_range(0.0, 1.0) = 0.4;
uniform vec3 sss_color : source_color = vec3(0.1, 0.5, 0.4);

// Caustics
uniform float caustic_strength : hint_range(0.0, 1.0) = 0.15;
uniform float caustic_scale : hint_range(0.01, 0.1) = 0.03;

varying vec3 world_pos;
varying vec3 world_normal;
varying float vertex_height;
varying float foam_factor;

// ============================================================================
// GERSTNER WAVE FUNCTION - Physically accurate wave displacement
// ============================================================================
vec3 gerstner_wave(vec2 pos, float time, vec2 direction, float frequency, float amplitude, float steepness) {
    direction = normalize(direction);
    float k = 2.0 * PI * frequency;  // Wave number
    float c = sqrt(9.8 / k);          // Phase speed (deep water approximation)
    float f = k * (dot(direction, pos) - c * time);
    float a = amplitude;
    
    // Gerstner displacement
    float q = steepness / (k * a + 0.001);  // Steepness factor (added small value to prevent div by zero)
    q = clamp(q, 0.0, 1.0);
    
    return vec3(
        q * a * direction.x * cos(f),
        a * sin(f),
        q * a * direction.y * cos(f)
    );
}

// Gerstner wave normal calculation
vec3 gerstner_normal(vec2 pos, float time, vec2 direction, float frequency, float amplitude, float steepness) {
    direction = normalize(direction);
    float k = 2.0 * PI * frequency;
    float c = sqrt(9.8 / k);
    float f = k * (dot(direction, pos) - c * time);
    float a = amplitude;
    float q = steepness / (k * a + 0.001);
    q = clamp(q, 0.0, 1.0);
    
    return vec3(
        -direction.x * k * a * cos(f),
        1.0 - q * k * a * sin(f),
        -direction.y * k * a * cos(f)
    );
}

// Noise functions for detail
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

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

// Voronoi for caustics
float voronoi(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    
    float min_dist = 1.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = vec2(hash(i + neighbor), hash(i + neighbor + vec2(5.0, 3.0)));
            vec2 diff = neighbor + point - f;
            float dist = length(diff);
            min_dist = min(min_dist, dist);
        }
    }
    return min_dist;
}

void vertex() {
    world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    vec2 pos = world_pos.xz * wave_scale;
    float time = TIME * wave_speed;
    
    // Sum of Gerstner waves for realistic displacement
    vec3 displacement = vec3(0.0);
    displacement += gerstner_wave(pos, time, wave1_direction, wave1_frequency, wave1_amplitude, wave1_steepness);
    displacement += gerstner_wave(pos, time * 1.1, wave2_direction, wave2_frequency, wave2_amplitude, wave2_steepness);
    displacement += gerstner_wave(pos, time * 0.9, wave3_direction, wave3_frequency, wave3_amplitude, wave3_steepness);
    displacement += gerstner_wave(pos, time * 1.2, wave4_direction, wave4_frequency, wave4_amplitude, wave4_steepness);
    
    // Apply displacement
    VERTEX.x += displacement.x;
    VERTEX.y += displacement.y;
    VERTEX.z += displacement.z;
    
    vertex_height = displacement.y;
    
    // Calculate foam based on wave peaks
    float max_amp = wave1_amplitude + wave2_amplitude + wave3_amplitude + wave4_amplitude;
    float wave_peak = (displacement.y + max_amp) / (2.0 * max_amp + 0.001);
    foam_factor = smoothstep(foam_cutoff, 1.0, wave_peak);
    
    // Calculate wave normal from Gerstner
    vec3 normal = vec3(0.0, 1.0, 0.0);
    vec3 n1 = gerstner_normal(pos, time, wave1_direction, wave1_frequency, wave1_amplitude, wave1_steepness);
    vec3 n2 = gerstner_normal(pos, time * 1.1, wave2_direction, wave2_frequency, wave2_amplitude, wave2_steepness);
    vec3 n3 = gerstner_normal(pos, time * 0.9, wave3_direction, wave3_frequency, wave3_amplitude, wave3_steepness);
    vec3 n4 = gerstner_normal(pos, time * 1.2, wave4_direction, wave4_frequency, wave4_amplitude, wave4_steepness);
    
    // Blend normals properly
    normal = normalize(n1 + n2 + n3 + n4);
    
    world_normal = normal;
    NORMAL = normalize((MODEL_MATRIX * vec4(world_normal, 0.0)).xyz);
}

void fragment() {
    float time = TIME * wave_speed;
    vec2 pos = world_pos.xz;
    
    // ========================================================================
    // NORMAL MAP SAMPLING (for surface detail)
    // ========================================================================
    // Animate UVs for flowing effect
    vec2 uv1 = pos * normal_map_scale + vec2(time * 0.02, time * 0.01);
    vec2 uv2 = pos * normal_map_scale * 1.5 - vec2(time * 0.015, time * 0.02);
    
    // Sample normal maps
    vec3 norm1 = texture(normal_map_1, uv1).rgb * 2.0 - 1.0;
    vec3 norm2 = texture(normal_map_2, uv2).rgb * 2.0 - 1.0;
    
    // Blend normal maps with Gerstner normal
    vec3 blended_normal = normalize(vec3(
        world_normal.x + (norm1.x + norm2.x * 0.5) * normal_map_strength,
        world_normal.y,
        world_normal.z + (norm1.y + norm2.y * 0.5) * normal_map_strength
    ));
    
    // Convert to view space normal
    NORMAL = normalize(blended_normal);
    
    // ========================================================================
    // FRESNEL CALCULATION
    // ========================================================================
    float NdotV = max(dot(NORMAL, VIEW), 0.0);
    float fresnel = fresnel_bias + (1.0 - fresnel_bias) * pow(1.0 - NdotV, fresnel_power);
    fresnel = clamp(fresnel, 0.0, 1.0);
    
    // ========================================================================
    // WATER COLOR WITH DEPTH SIMULATION
    // ========================================================================
    // Simulate depth using noise
    float depth_factor = fbm(pos * 0.003, 4);
    depth_factor = smoothstep(0.3, 0.7, depth_factor);
    
    vec3 water_color = mix(water_color_shallow, water_color_deep, depth_factor);
    
    // ========================================================================
    // SUBSURFACE SCATTERING (light passing through wave peaks)
    // ========================================================================
    vec3 light_dir = normalize(vec3(0.5, 0.8, 0.3));  // Sun direction
    float sss = pow(max(dot(VIEW, -light_dir), 0.0), 4.0);
    float max_amp = wave1_amplitude + wave2_amplitude;
    sss *= max(vertex_height / (max_amp + 0.001), 0.0);
    vec3 sss_contribution = sss_color * sss * sss_strength;
    
    water_color += sss_contribution;
    
    // ========================================================================
    // CAUSTICS
    // ========================================================================
    float caustic_time = time * 0.5;
    vec2 caustic_uv = pos * caustic_scale;
    float caustic1 = voronoi(caustic_uv + vec2(caustic_time * 0.3, caustic_time * 0.2));
    float caustic2 = voronoi(caustic_uv * 1.3 - vec2(caustic_time * 0.2, caustic_time * 0.35));
    float caustics = pow(caustic1 * caustic2, 2.0) * 2.0;
    caustics *= (1.0 - depth_factor) * caustic_strength;  // More visible in shallow areas
    
    water_color += vec3(caustics) * water_color_shallow;
    
    // ========================================================================
    // REFLECTION
    // ========================================================================
    // Simple sky reflection based on normal
    vec3 reflect_dir = reflect(-VIEW, NORMAL);
    float sky_factor = smoothstep(-0.1, 0.5, reflect_dir.y);
    vec3 reflected_color = mix(water_color_fresnel, sky_color, sky_factor);
    
    water_color = mix(water_color, reflected_color, fresnel * reflection_strength);
    
    // ========================================================================
    // FOAM
    // ========================================================================
    float foam_noise = fbm(pos * 0.1 + vec2(time * 0.1), 3);
    float foam = foam_factor * foam_amount;
    foam += foam_noise * 0.1 * foam_amount;
    foam = smoothstep(0.0, 0.5, foam);
    
    water_color = mix(water_color, foam_color, foam * 0.7);
    
    // ========================================================================
    // SPECULAR HIGHLIGHTS
    // ========================================================================
    vec3 half_vec = normalize(light_dir + VIEW);
    float spec = pow(max(dot(NORMAL, half_vec), 0.0), 512.0);
    
    // Sun glitter/sparkle
    float glitter_noise = fbm(pos * 3.0 + vec2(time * 0.5), 2);
    float glitter = pow(glitter_noise, 6.0) * fresnel * 0.5;
    spec += glitter;
    
    water_color += vec3(1.0, 0.98, 0.9) * spec * 0.6;
    
    // ========================================================================
    // FINAL OUTPUT
    // ========================================================================
    ALBEDO = water_color;
    ROUGHNESS = roughness + foam * 0.3;  // Foam is rougher
    METALLIC = metallic * (1.0 - foam);  // Foam is not metallic
    ALPHA = opacity;
    
    // Subtle emission for that underwater glow
    EMISSION = sss_contribution * 0.2;
}
