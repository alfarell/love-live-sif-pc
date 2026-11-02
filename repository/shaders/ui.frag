#version 450
// UI quads (ui2 layouts): the texture with straight alpha, times a tint (fades), with a UV offset (label atlases:
// ANMT channel types 4/5, e.g. a part's "switch" clip picking one cell), cut off past a mesh U (fill gauges), plus an
// added colour (.fad colour transforms: result = texture x tint + add).
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 3, binding = 0) uniform Tint {
    vec4 tint;
    vec4 uv_offset;  // xy: offset; z: the mesh U past which nothing is drawn
    vec4 add;
};
layout(location = 0) in vec3 world_normal;
layout(location = 1) in vec2 tex_uv;
layout(location = 0) out vec4 color;

void main() {
    if (tex_uv.x > uv_offset.z) discard;
    color = clamp(texture(tex, tex_uv + uv_offset.xy) * tint + add, 0.0, 1.0);
}
