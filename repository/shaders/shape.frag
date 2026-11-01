#version 450
// Filled disc or ring with an anti-aliased edge, drawn on the blit quad (uv 0..1 across the bounding square).
layout(set = 3, binding = 0) uniform Shape {
    vec4 color;   // straight alpha
    vec4 params;  // x: inner radius / outer radius (0 = filled disc), y: edge softness in uv units
};
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

void main() {
    float d = length(uv * 2.0 - 1.0);
    float soft = max(params.y, 1e-4);
    float outer = 1.0 - smoothstep(1.0 - soft, 1.0, d);
    float inner = params.x > 0.0 ? smoothstep(params.x - soft, params.x, d) : 1.0;
    out_color = vec4(color.rgb, color.a * outer * inner);
}
