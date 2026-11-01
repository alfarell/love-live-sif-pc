#version 450
// Textured 3D mesh (stage, characters). Vertices are already in model space; skinning is done on the CPU.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(set = 1, binding = 0) uniform Transform {
    mat4 mvp;
    mat4 model;
};
layout(location = 0) out vec3 world_normal;
layout(location = 1) out vec2 tex_uv;

void main() {
    gl_Position = mvp * vec4(position, 1.0);
    world_normal = mat3(model) * normal;
    tex_uv = uv;
}
