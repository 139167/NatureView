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
        float rock = saturate(smoothstep(0.72, 0.35, normal.y) +
                              smoothstep(520.0, 920.0, in.worldPosition.y));
        float moisture = 0.5 + 0.5 * sin(
            in.worldPosition.x * 0.011 + in.worldPosition.z * 0.017);
        float3 vegetation = mix(float3(0.055, 0.11, 0.04),
                                float3(0.11, 0.17, 0.06), moisture);
        float3 stone = float3(0.21, 0.20, 0.175);
        color = mix(vegetation, stone, saturate(rock));

        float snow = smoothstep(1060.0, 1220.0, in.worldPosition.y) *
                     smoothstep(0.35, 0.72, normal.y);
        color = mix(color, float3(0.76, 0.80, 0.82), snow);

        float diffuse = saturate(dot(normal, sunDirection));
        float skyLight = 0.16 + 0.23 * saturate(normal.y);
        color *= skyLight + diffuse * 0.95;
    }

    float distanceToCamera = length(in.worldPosition - uniforms.cameraPosition.xyz);
    float fog = 1.0 - exp(-uniforms.settings.y * distanceToCamera);
    fog += smoothstep(900.0, 4800.0, distanceToCamera) * 0.30;
    color = mix(color, float3(0.55, 0.62, 0.68), saturate(fog) * 0.80);
    return float4(saturate(color), 1.0);
}
)METAL";

}  // namespace natureview::platform
