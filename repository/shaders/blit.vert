#version 450
// Draws one textured rectangle as a 4-vertex triangle strip; no vertex buffer.
layout(set = 1, binding = 0) uniform Rect {
    vec4 rect;  // x0, y0 (top-left), x1, y1 (bottom-right) in normalized device coordinates
};
layout(location = 0) out vec2 uv;

void main() {
    vec2 corner = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1);
    uv = corner;
    gl_Position = vec4(mix(rect.xy, rect.zw, corner), 0.0, 1.0);
}
