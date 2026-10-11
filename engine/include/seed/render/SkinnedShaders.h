#pragma once

#include <string_view>

namespace seed {

// Seed's first backend-neutral skinning vertex contract expressed in GLSL for
// the current OpenGL 3.3 backend. The renderer only knows about attributes and
// the uJointMatrices palette; animation evaluation remains entirely Seed-owned.
inline constexpr std::string_view SeedSkinnedMeshVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexcoord;
layout(location = 3) in vec3 aNormal;
layout(location = 4) in vec4 aJoints;
layout(location = 5) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat4 uLightViewProjection;
uniform mat4 uJointMatrices[96];

out vec3 vColor;
out vec2 vTexcoord;
out vec3 vWorldNormal;
out vec3 vWorldPosition;
out vec4 vShadowPosition;

mat4 seedSkinMatrix() {
    vec4 weights = max(aWeights, vec4(0.0));
    float total = weights.x + weights.y + weights.z + weights.w;
    if (total <= 0.000001) return mat4(1.0);
    weights /= total;

    ivec4 joints = ivec4(aJoints + vec4(0.5));
    return
        uJointMatrices[joints.x] * weights.x +
        uJointMatrices[joints.y] * weights.y +
        uJointMatrices[joints.z] * weights.z +
        uJointMatrices[joints.w] * weights.w;
}

void main() {
    mat4 skin = seedSkinMatrix();
    vec4 localPosition = skin * vec4(aPosition, 1.0);
    vec3 localNormal = normalize(mat3(skin) * aNormal);
    vec4 worldPosition = uModel * localPosition;

    gl_Position = uViewProjection * worldPosition;
    vColor = aColor;
    vTexcoord = aTexcoord;
    vWorldPosition = worldPosition.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normalMatrix * localNormal);
    vShadowPosition = uLightViewProjection * worldPosition;
}
)GLSL";

} // namespace seed
