#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D albedoTexture;

void main() {
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(vec3(-0.35, 0.75, 0.42));
    float diffuse = max(dot(normal, lightDir), 0.0);
    float light = 0.24 + diffuse * 0.76;
    vec3 albedo = texture(albedoTexture, inUv).rgb;
    outColor = vec4(albedo * inColor * light, 1.0);
}
