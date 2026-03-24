#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUv;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUv;

layout(push_constant) uniform MeshPushConstants {
    mat4 mvp;
    mat4 model;
} pushData;

void main() {
    gl_Position = pushData.mvp * vec4(inPos, 1.0);
    outNormal = mat3(pushData.model) * inNormal;
    outColor = inColor;
    outUv = inUv;
}
