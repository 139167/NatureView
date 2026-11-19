#pragma once

namespace natureview::platform {

inline constexpr const char* kMetalShaderSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float2 uv [[attribute(2)]];
};

struct Uniforms {
    float4x4 viewProjection;
    float4 cameraPosition;
    float4 sunDirection;
    float4 settings;
};

struct VertexOut {
    float4 position [[position]];
    float3 worldPosition;
    float3 normal;
    float2 uv;
};

float hash21(float2 position)
{
    float2 point = fract(position * float2(123.34, 345.45));
    point += dot(point, point + 34.345);
    return fract(point.x * point.y);
}

float valueNoise(float2 position)
{
    float2 cell = floor(position);
    float2 local = fract(position);
    float2 weight = local * local * (3.0 - 2.0 * local);
    float lower = mix(hash21(cell), hash21(cell + float2(1.0, 0.0)), weight.x);
    float upper = mix(hash21(cell + float2(0.0, 1.0)),
                      hash21(cell + float2(1.0, 1.0)), weight.x);
    return mix(lower, upper, weight.y);
}

float fbm(float2 position)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 4; ++octave) {
        value += valueNoise(position) * amplitude;
        position = position * 2.03 + float2(17.3, 9.1);
        amplitude *= 0.5;
    }
    return value;
}

float terrainMicroHeight(float2 position)
{
    return fbm(position * 0.16 + float2(3.7, 11.9)) * 0.75 +
           valueNoise(position * 0.62 + float2(5.3, 2.1)) * 0.25;
}

vertex VertexOut terrainVertex(
    VertexIn in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]])
{
    VertexOut out;
    out.position = uniforms.viewProjection * float4(in.position, 1.0);
    out.worldPosition = in.position;
    out.normal = in.normal;
    out.uv = in.uv;
    return out;
}

fragment float4 terrainFragment(
    VertexOut in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]])
{
    float3 sunDirection = normalize(uniforms.sunDirection.xyz);
    float distanceToCamera = length(in.worldPosition - uniforms.cameraPosition.xyz);
    float3 color;

    if (uniforms.settings.x > 0.5) {
        float3 normal = normalize(float3(
            sin(in.worldPosition.x * 0.075 + in.worldPosition.z * 0.052) * 0.035,
            1.0,
            sin(in.worldPosition.z * 0.13 - in.worldPosition.x * 0.031) * 0.025));
        float3 viewDirection = normalize(in.worldPosition - uniforms.cameraPosition.xyz);
        float3 reflected = reflect(viewDirection, normal);
        float fresnel = pow(1.0 - saturate(abs(dot(viewDirection, normal))), 3.0);
        float3 sky = mix(float3(0.42, 0.52, 0.62), float3(0.14, 0.28, 0.48),
                         saturate(reflected.y));
        float glint = pow(saturate(dot(reflected, sunDirection)), 160.0);
        color = mix(float3(0.018, 0.085, 0.105), sky, 0.28 + fresnel * 0.62);
        color += float3(1.0, 0.82, 0.56) * glint * 0.8;
    } else {
        float3 normal = normalize(in.normal);
        float2 ground = in.worldPosition.xz;
        float macro = fbm(ground * 0.0045 + float2(4.2, 9.7));
        float meso = fbm(ground * 0.026 + float2(18.1, 21.7));
        float fine = valueNoise(ground * 0.19 + float2(5.3, 13.1));
        float micro = valueNoise(ground * 0.73 + float2(31.2, 7.4));
        float fineFootprint =
            max(fwidth(ground.x * 0.19), fwidth(ground.y * 0.19));
        float microFootprint =
            max(fwidth(ground.x * 0.73), fwidth(ground.y * 0.73));
        fine *= 1.0 - smoothstep(0.22, 1.10, fineFootprint);
        micro *= 1.0 - smoothstep(0.20, 0.85, microFootprint);

        const float offset = 0.28;
        float heightLeft = terrainMicroHeight(ground - float2(offset, 0.0));
        float heightRight = terrainMicroHeight(ground + float2(offset, 0.0));
        float heightSouth = terrainMicroHeight(ground - float2(0.0, offset));
        float heightNorth = terrainMicroHeight(ground + float2(0.0, offset));
        float normalDetailFade =
            (1.0 - smoothstep(120.0, 1100.0, distanceToCamera)) *
            (1.0 - smoothstep(0.30, 1.10, microFootprint));
        normal = normalize(normal + float3(
            (heightLeft - heightRight) * 1.8 * normalDetailFade,
            0.0,
            (heightSouth - heightNorth) * 1.8 * normalDetailFade));

        float slope = 1.0 - normal.y;
        float rock = saturate(smoothstep(0.20, 0.58, slope) +
                              smoothstep(520.0, 980.0, in.worldPosition.y) +
                              smoothstep(0.56, 0.84, meso) * 0.45);
        float3 vegetation = mix(
            float3(0.026, 0.062, 0.018),
            float3(0.070, 0.115, 0.031),
            macro);
        vegetation = mix(
            vegetation,
            float3(0.15, 0.125, 0.060),
            smoothstep(0.48, 0.82, meso) * 0.36);
        vegetation *= 0.80 + fine * 0.30 + micro * 0.06;

        float3 stone = mix(
            float3(0.090, 0.078, 0.063),
            float3(0.29, 0.27, 0.21),
            fine * 0.72 + micro * 0.28);
        color = mix(vegetation, stone, saturate(rock));

        float snow = smoothstep(1060.0, 1220.0, in.worldPosition.y) *
                     smoothstep(0.35, 0.72, normal.y);
        color = mix(color, float3(0.76, 0.80, 0.82), snow);

        float diffuse = saturate(dot(normal, sunDirection));
        float skyLight = 0.18 + 0.25 * saturate(normal.y);
        color *= skyLight + diffuse * 0.72;
        float sheen = pow(saturate(dot(normal, sunDirection)), 48.0) * rock * 0.10;
        color += float3(0.85, 0.78, 0.62) * sheen;
    }

    float fog = 1.0 - exp(-uniforms.settings.y * distanceToCamera);
    fog += smoothstep(900.0, 4800.0, distanceToCamera) * 0.30;
    color = mix(color, float3(0.55, 0.62, 0.68), saturate(fog) * 0.80);
    return float4(saturate(color), 1.0);
}
)METAL";

}  // namespace natureview::platform
