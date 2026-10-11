#pragma once

#include <string_view>

namespace seed::studio {

inline constexpr std::string_view SeedMeshVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexcoord;
layout(location = 3) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat4 uLightViewProjection;

out vec3 vColor;
out vec2 vTexcoord;
out vec3 vWorldNormal;
out vec3 vWorldPosition;
out vec4 vShadowPosition;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    gl_Position = uViewProjection * worldPosition;
    vColor = aColor;
    vTexcoord = aTexcoord;
    vWorldPosition = worldPosition.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normalMatrix * aNormal);
    vShadowPosition = uLightViewProjection * worldPosition;
}
)GLSL";

inline constexpr std::string_view SeedMeshFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor;
in vec2 vTexcoord;
in vec3 vWorldNormal;
in vec3 vWorldPosition;
in vec4 vShadowPosition;
out vec4 FragColor;

uniform vec4 uBaseColor;
uniform sampler2D uBaseTexture;
uniform int uUseTexture;
uniform sampler2D uMetallicRoughnessTexture;
uniform int uUseMetallicRoughnessTexture;
uniform sampler2D uNormalTexture;
uniform int uUseNormalTexture;
uniform float uMetallic;
uniform float uRoughness;
uniform float uNormalScale;
uniform vec3 uCameraPosition;
uniform vec3 uDirectionalDirection;
uniform vec3 uDirectionalColor;
uniform float uDirectionalIntensity;
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform vec3 uEnvironmentZenithColor;
uniform vec3 uEnvironmentHorizonColor;
uniform float uEnvironmentIntensity;
uniform int uEnvironmentEnabled;
uniform sampler2D uShadowMap;
uniform int uReceiveShadows;
uniform int uShadowsEnabled;

const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(N, H), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = nDotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.000001);
}

float geometrySchlickGGX(float nDotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.000001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 f0, float roughness) {
    vec3 grazing = max(vec3(1.0 - roughness), f0);
    return f0 + (grazing - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 sampleSeedEnvironment(vec3 direction) {
    if (uEnvironmentEnabled == 0) return vec3(0.0);
    vec3 D = normalize(direction);
    float up = clamp(D.y, 0.0, 1.0);
    float down = clamp(-D.y, 0.0, 1.0);
    vec3 upper = mix(uEnvironmentHorizonColor, uEnvironmentZenithColor, pow(up, 0.65));
    vec3 ground = uEnvironmentHorizonColor * 0.18;
    vec3 lower = mix(uEnvironmentHorizonColor, ground, pow(down, 0.80));
    vec3 color = D.y >= 0.0 ? upper : lower;
    return color * max(uEnvironmentIntensity, 0.0);
}

vec3 seedDiffuseIrradiance(vec3 normal) {
    vec3 N = normalize(normal);
    vec3 horizonDirection = normalize(vec3(N.x + 0.001, 0.001, N.z));
    vec3 primary = sampleSeedEnvironment(N);
    vec3 overhead = sampleSeedEnvironment(vec3(0.0, 1.0, 0.0));
    vec3 horizon = sampleSeedEnvironment(horizonDirection);
    return primary * 0.55 + overhead * 0.20 + horizon * 0.25;
}

vec2 environmentBRDFApprox(float nDotV, float roughness) {
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nDotV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

vec3 materialNormal() {
    vec3 N = normalize(vWorldNormal);
    if (uUseNormalTexture == 0) return N;

    vec3 tangentNormal = texture(uNormalTexture, vTexcoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= max(uNormalScale, 0.0);
    tangentNormal = normalize(tangentNormal);

    vec3 positionDx = dFdx(vWorldPosition);
    vec3 positionDy = dFdy(vWorldPosition);
    vec2 uvDx = dFdx(vTexcoord);
    vec2 uvDy = dFdy(vTexcoord);
    float determinant = uvDx.x * uvDy.y - uvDx.y * uvDy.x;
    if (abs(determinant) < 0.0000001) return N;

    vec3 T = normalize(positionDx * uvDy.y - positionDy * uvDx.y);
    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(cross(N, T)) * (determinant < 0.0 ? -1.0 : 1.0);
    return normalize(mat3(T, B, N) * tangentNormal);
}

float shadowVisibility(vec3 normal, vec3 directionToLight) {
    if (uShadowsEnabled == 0 || uReceiveShadows == 0) return 1.0;
    vec3 projected = vShadowPosition.xyz / max(vShadowPosition.w, 0.00001);
    projected = projected * 0.5 + 0.5;
    if (projected.z <= 0.0 || projected.z >= 1.0 ||
        projected.x <= 0.0 || projected.x >= 1.0 ||
        projected.y <= 0.0 || projected.y >= 1.0) return 1.0;

    float bias = max(0.0015 * (1.0 - dot(normal, directionToLight)), 0.00035);
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closest = texture(uShadowMap, projected.xy + vec2(x, y) * texel).r;
            visibility += projected.z - bias <= closest ? 1.0 : 0.0;
        }
    }
    return visibility / 9.0;
}

void main() {
    vec4 sampled = vec4(1.0);
    if (uUseTexture != 0) sampled = texture(uBaseTexture, vTexcoord);

    vec3 albedo = vColor * uBaseColor.rgb;
    if (uUseTexture != 0) albedo *= pow(max(sampled.rgb, vec3(0.0)), vec3(2.2));
    float alpha = uBaseColor.a * sampled.a;

    float metallic = clamp(uMetallic, 0.0, 1.0);
    float roughness = clamp(uRoughness, 0.04, 1.0);
    if (uUseMetallicRoughnessTexture != 0) {
        vec4 metallicRoughness = texture(uMetallicRoughnessTexture, vTexcoord);
        roughness = clamp(roughness * metallicRoughness.g, 0.04, 1.0);
        metallic = clamp(metallic * metallicRoughness.b, 0.0, 1.0);
    }

    vec3 N = materialNormal();
    vec3 V = normalize(uCameraPosition - vWorldPosition);
    vec3 L = normalize(-uDirectionalDirection);
    vec3 H = normalize(V + L);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float ndf = distributionGGX(N, H, roughness);
    float geometry = geometrySmith(N, V, L, roughness);
    vec3 fresnel = fresnelSchlick(max(dot(H, V), 0.0), f0);

    vec3 numerator = ndf * geometry * fresnel;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = fresnel;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    float nDotL = max(dot(N, L), 0.0);
    float shadow = shadowVisibility(N, L);
    vec3 radiance = uDirectionalColor * max(uDirectionalIntensity, 0.0);
    vec3 direct = (kD * albedo / PI + specular) * radiance * nDotL * shadow;
    vec3 ambient = uAmbientColor * max(uAmbientIntensity, 0.0) * albedo * mix(1.0, 0.45, metallic);

    vec3 environment = vec3(0.0);
    if (uEnvironmentEnabled != 0 && uEnvironmentIntensity > 0.0) {
        float nDotV = max(dot(N, V), 0.0);
        vec3 environmentFresnel = fresnelSchlickRoughness(nDotV, f0, roughness);
        vec3 environmentKD = (vec3(1.0) - environmentFresnel) * (1.0 - metallic);

        vec3 irradiance = seedDiffuseIrradiance(N);
        vec3 diffuseIBL = irradiance * albedo * environmentKD;

        vec3 R = reflect(-V, N);
        vec3 sharpReflection = sampleSeedEnvironment(R);
        vec3 averageEnvironment = (
            uEnvironmentZenithColor +
            uEnvironmentHorizonColor +
            uEnvironmentHorizonColor * 0.18
        ) * (max(uEnvironmentIntensity, 0.0) / 3.0);
        vec3 prefilteredReflection = mix(
            sharpReflection,
            averageEnvironment,
            clamp(roughness * roughness, 0.0, 1.0)
        );
        vec2 environmentBRDF = environmentBRDFApprox(nDotV, roughness);
        vec3 specularIBL = prefilteredReflection * (f0 * environmentBRDF.x + environmentBRDF.y);

        environment = diffuseIBL * 0.35 + specularIBL * 0.85;
    }

    vec3 color = max(ambient + direct + environment, vec3(0.0));
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, alpha);
}
)GLSL";

} // namespace seed::studio
