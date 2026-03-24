#version 450

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

void main() {
    const float alpha = texture(fontAtlas, inUv).a;
    outFragColor = vec4(inColor.rgb, inColor.a * alpha);
}
