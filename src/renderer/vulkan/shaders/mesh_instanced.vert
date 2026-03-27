#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUv;
layout(location = 4) in vec4 inModelRow0;
layout(location = 5) in vec4 inModelRow1;
layout(location = 6) in vec4 inModelRow2;
layout(location = 7) in vec4 inModelRow3;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUv;

layout(push_constant) uniform MeshInstancePushConstants {
    mat4 projectionView;
} pushData;

void main() {
    mat4 model = mat4(inModelRow0, inModelRow1, inModelRow2, inModelRow3);
    vec4 worldPosition = model * vec4(inPos, 1.0);
    gl_Position = pushData.projectionView * worldPosition;
    outNormal = mat3(model) * inNormal;
    outColor = inColor;
    outUv = inUv;
}
