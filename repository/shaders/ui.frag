#version 450
// UI quads (ui2 layouts): the texture with straight alpha, times a tint (fades), with a UV offset (label atlases:
// ANMT channel types 4/5, e.g. a part's "switch" clip picking one cell).
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 3, binding = 0) uniform Tint {
    vec4 tint;
    vec4 uv_offset;  // xy
};
layout(location = 0) in vec3 world_normal;
layout(location = 1) in vec2 tex_uv;
layout(location = 0) out vec4 color;

void main() {
    color = texture(tex, tex_uv + uv_offset.xy) * tint;
}
