shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_disabled, diffuse_burley, specular_schlick_ggx;

// Grass properties
group_uniforms grass;
uniform vec3 grass_color_base : source_color = vec3(0.2, 0.45, 0.15);
uniform vec3 grass_color_tip : source_color = vec3(0.4, 0.65, 0.2);
uniform float grass_height : hint_range(0.1, 3.0) = 0.8;

// Wind settings
group_uniforms wind;
uniform float wind_strength : hint_range(0.0, 2.0) = 0.5;
uniform float wind_speed : hint_range(0.0, 5.0) = 1.5;
uniform vec2 wind_direction = vec2(1.0, 0.5);
uniform float wind_turbulence : hint_range(0.0, 1.0) = 0.3;

// Rendering
group_uniforms rendering;
uniform float alpha_scissor : hint_range(0.0, 1.0) = 0.5;
uniform float fade_distance : hint_range(10.0, 200.0) = 80.0;
uniform float fade_range : hint_range(5.0, 50.0) = 20.0;

varying float height_factor;
varying vec3 world_pos;

void vertex() {
    world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    
    // Height factor for color gradient and wind (0 at base, 1 at tip)
    height_factor = UV.y;
    
    // Wind animation
    if (height_factor > 0.1) {
        float time = TIME * wind_speed;
        vec2 wind_dir = normalize(wind_direction);
        
        // Main wind wave
        float wind_wave = sin(dot(world_pos.xz, wind_dir) * 0.5 + time);
        
        // Turbulence (higher frequency noise)
        float turb = sin(world_pos.x * 2.0 + time * 2.3) * sin(world_pos.z * 2.5 + time * 1.7);
        turb *= wind_turbulence;
        
        // Apply wind displacement (stronger at tip)
        float wind_factor = height_factor * height_factor * wind_strength;
        VERTEX.x += (wind_wave + turb) * wind_factor * wind_dir.x;
        VERTEX.z += (wind_wave + turb) * wind_factor * wind_dir.y;
        
        // Slight vertical compression when bent
        VERTEX.y -= abs(wind_wave) * wind_factor * 0.1;
    }
}

void fragment() {
    // Color gradient from base to tip
    vec3 color = mix(grass_color_base, grass_color_tip, height_factor);
    
    // Add some color variation based on position
    float variation = fract(sin(dot(world_pos.xz, vec2(12.9898, 78.233))) * 43758.5453);
    color *= 0.9 + variation * 0.2;
    
    // Slight darkening at base (ambient occlusion simulation)
    float ao = smoothstep(0.0, 0.3, height_factor);
    color *= 0.7 + ao * 0.3;
    
    // Distance fade
    float dist = length(world_pos - CAMERA_POSITION_WORLD);
    float fade = 1.0 - smoothstep(fade_distance - fade_range, fade_distance, dist);
    
    ALBEDO = color;
    ROUGHNESS = 0.9;
    METALLIC = 0.0;
    
    // Alpha for grass blade shape
    float alpha = fade;
    if (alpha < alpha_scissor) {
        discard;
    }
    
    // Subsurface scattering approximation (grass glows when backlit)
    BACKLIGHT = vec3(0.3, 0.5, 0.2) * height_factor;
}
