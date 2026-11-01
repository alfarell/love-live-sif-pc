#version 450
// shortcut: one fixed light and alpha test only; the game's 9 shader materials (docs/formats/shaders.md) come later
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec3 world_normal;
layout(location = 1) in vec2 tex_uv;
layout(location = 0) out vec4 color;

void main() {
    vec4 t = texture(tex, tex_uv);
    if (t.a < 0.5) discard;
    float light = 1.0;
    if (dot(world_normal, world_normal) > 1e-6) {  // some meshes have no normals
        light = 0.65 + 0.35 * max(dot(normalize(world_normal), normalize(vec3(0.3, -0.6, 0.75))), 0.0);  // Z-up world
    }
    color = vec4(t.rgb * light, 1.0);
}
