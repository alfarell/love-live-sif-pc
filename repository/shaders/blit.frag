#version 450
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main() {
    vec4 t = texture(tex, uv);
    color = vec4(t.rgb * t.a, 1.0);  // composite over black (card art has transparent borders)
}
