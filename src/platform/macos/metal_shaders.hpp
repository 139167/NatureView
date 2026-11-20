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
    float4 cameraRight;
    float4 cameraUp;
    float4 cameraForward;
    float4 skySettings;
    float4x4 lightViewProjection;
    float4 shadowSettings;
    float4 windSettings;
};

struct TonemapUniforms {
    float exposure;
    float padding[3];
};

struct VertexOut {
    float4 position [[position]];
    float3 worldPosition;
    float3 normal;
    float2 uv;
};

struct SkyOut {
    float4 position [[position]];
    float2 ndc;
};

struct TonemapOut {
    float4 position [[position]];
};

struct ShadowOut {
    float4 position [[position]];
};

struct TreeInstance {
    float4 positionAndHeight;
    float4 parameters;
};

struct TreeTransform {
    float3 position;
    float3 normal;
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

vertex ShadowOut shadowVertex(
    VertexIn in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]])
{
    ShadowOut out;
    out.position = uniforms.lightViewProjection * float4(in.position, 1.0);
    return out;
}

TreeTransform transformTree(
    VertexIn in,
    TreeInstance instance,
    constant Uniforms& uniforms)
{
    float yaw = instance.parameters.x;
    float radius = instance.parameters.y;
    float phase = instance.parameters.z;
    float3 local = float3(
        in.position.x * radius,
        in.position.y * instance.positionAndHeight.w,
        in.position.z * radius);
    float cosine = cos(yaw);
    float sine = sin(yaw);
    float3 rotated = float3(
        local.x * cosine - local.z * sine,
        local.y,
        local.x * sine + local.z * cosine);
    float3 normal = normalize(float3(
        in.normal.x * cosine - in.normal.z * sine,
        in.normal.y,
        in.normal.x * sine + in.normal.z * cosine));

    float3 worldPosition = instance.positionAndHeight.xyz + rotated;
    float windDirection = uniforms.windSettings.x;
    float2 wind = float2(cos(windDirection), sin(windDirection));
    float2 crossWind = float2(-wind.y, wind.x);
    float heightWeight = pow(saturate(in.position.y), 1.7);
    float windStrength =
        uniforms.windSettings.y * saturate(uniforms.windSettings.z);
    float oscillation = sin(
        uniforms.settings.z * 1.35 +
        phase +
        dot(worldPosition.xz, float2(0.006, 0.004)));
    float bend =
        heightWeight * instance.positionAndHeight.w *
        (0.025 + windStrength * 0.090);
    worldPosition.xz +=
        wind * bend * (0.65 + oscillation * 0.35) +
        crossWind * bend * oscillation * 0.24;

    TreeTransform result;
    result.position = worldPosition;
    result.normal = normal;
    return result;
}

vertex VertexOut treeVertex(
    VertexIn in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]],
    constant TreeInstance* instances [[buffer(2)]],
    uint instanceID [[instance_id]])
{
    TreeInstance instance = instances[instanceID];
    TreeTransform transformed = transformTree(in, instance, uniforms);
    VertexOut out;
    out.position =
        uniforms.viewProjection * float4(transformed.position, 1.0);
    out.worldPosition = transformed.position;
    out.normal = transformed.normal;
    out.uv = float2(in.uv.x, instance.parameters.w);
    return out;
}

vertex ShadowOut treeShadowVertex(
    VertexIn in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]],
    constant TreeInstance* instances [[buffer(2)]],
    uint instanceID [[instance_id]])
{
    TreeInstance instance = instances[instanceID];
    TreeTransform transformed = transformTree(in, instance, uniforms);
    ShadowOut out;
    out.position =
        uniforms.lightViewProjection * float4(transformed.position, 1.0);
    return out;
}

vertex SkyOut skyVertex(uint vertexID [[vertex_id]])
{
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0),
    };
    SkyOut out;
    out.position = float4(positions[vertexID], 1.0, 1.0);
    out.ndc = positions[vertexID];
    return out;
}

float3 skyRadiance(
    float3 ray,
    float3 sunDirection,
    float cloud,
    float daylight,
    float auroraActivity,
    float time)
{
    float elevation = saturate(ray.y);
    float3 horizonColor = float3(0.16, 0.23, 0.32);
    float3 zenith = float3(0.018, 0.065, 0.18);
    float3 color = mix(horizonColor, zenith, pow(elevation, 0.62));
    color *= mix(float3(0.010, 0.018, 0.035), float3(1.0), daylight);

    float solar = saturate(dot(ray, sunDirection));
    float halo = pow(solar, 32.0) * 0.35;
    float disk = smoothstep(0.9992, 0.99975, solar) * 8.0;
    color += float3(1.0, 0.67, 0.34) * (halo + disk) * daylight;
    color *= 1.0 - cloud * 0.28;

    float night = 1.0 - daylight;
    float clear = 1.0 - smoothstep(0.35, 0.80, cloud);
    float2 celestial = float2(
        atan2(ray.x, ray.z),
        asin(clamp(ray.y, -1.0, 1.0)));
    float2 starGrid = celestial * float2(120.0, 64.0);
    float2 starCell = floor(starGrid);
    float2 starLocal = fract(starGrid) - 0.5;
    float starHash = hash21(starCell);
    float star =
        smoothstep(0.996, 0.9995, starHash) *
        (1.0 - smoothstep(0.02, 0.12, length(starLocal))) *
        smoothstep(0.02, 0.18, ray.y);
    star *= 0.72 + 0.28 * sin(time * 0.7 + starHash * 47.0);
    color += float3(0.72, 0.82, 1.0) * star * night * clear * 0.55;

    float azimuth = atan2(ray.x, ray.z);
    float vertical =
        smoothstep(0.012, 0.070, ray.y) *
        (1.0 - smoothstep(0.62, 0.94, ray.y));
    float2 horizontalDirection =
        normalize(ray.xz + float2(0.0001, 0.0001));
    float northMask = smoothstep(-0.35, 0.45, horizontalDirection.y);
    float3 auroraEmission = float3(0.0);
    if (auroraActivity > 0.001 && night > 0.01 && clear > 0.01) {
        constexpr int layerCount = 6;
        for (int layer = 0; layer < layerCount; ++layer) {
            float layerAmount =
                (static_cast<float>(layer) + 0.5) / static_cast<float>(layerCount);
            float planeHeight = 0.10 + layerAmount * 0.58;
            float planeScale = min(
                planeHeight / max(ray.y, 0.025),
                80.0);
            float2 plane = ray.xz * planeScale;

            float warp = fbm(
                plane * 0.055 +
                float2(time * 0.004, layerAmount * 1.7));
            float broadWave = 0.5 + 0.5 * sin(
                azimuth * 4.1 +
                warp * 3.4 +
                layerAmount * 0.9 +
                time * 0.011);
            float broadCurtain = smoothstep(
                0.42,
                0.74,
                broadWave * 0.72 + warp * 0.48);

            float ribbonDetail = fbm(float2(
                plane.x * 0.15 + time * 0.018,
                plane.y * 0.10 - layerAmount * 2.3));
            float ribbonWave = 0.5 + 0.5 * sin(
                azimuth * 22.0 +
                ribbonDetail * 7.0 +
                layerAmount * 1.4 +
                time * 0.017);
            float ribbon = saturate(
                pow(ribbonWave, 2.4) *
                    (0.35 + ribbonDetail * 1.2) +
                0.07);
            float density =
                broadCurtain * ribbon *
                (1.0 - layerAmount * 0.42);
            float3 layerColor = mix(
                float3(0.035, 0.86, 0.36),
                float3(0.36, 0.07, 0.74),
                saturate(layerAmount * 0.82 + ray.y * 0.24));
            auroraEmission += layerColor * density;
        }
    }
    color += auroraEmission * vertical * northMask * night * clear *
             auroraActivity * 0.78;
    return color;
}

fragment float4 skyFragment(
    SkyOut in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]])
{
    float3 forward = normalize(uniforms.cameraForward.xyz);
    float3 right = normalize(uniforms.cameraRight.xyz);
    float3 up = normalize(uniforms.cameraUp.xyz);
    float tangent = uniforms.skySettings.x;
    float aspect = uniforms.skySettings.y;
    float cloud = uniforms.skySettings.z;
    float daylight = uniforms.skySettings.w;
    float3 ray = normalize(
        forward + right * (in.ndc.x * tangent * aspect) + up * (in.ndc.y * tangent));

    float3 sunDirection = normalize(uniforms.sunDirection.xyz);
    float3 color = skyRadiance(
        ray,
        sunDirection,
        cloud,
        daylight,
        uniforms.settings.w,
        uniforms.settings.z);
    return float4(color, 1.0);
}

vertex TonemapOut tonemapVertex(uint vertexID [[vertex_id]])
{
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0),
    };
    TonemapOut out;
    out.position = float4(positions[vertexID], 1.0, 1.0);
    return out;
}

float3 acesToneMap(float3 color)
{
    return saturate((color * (2.51 * color + 0.03)) /
                    (color * (2.43 * color + 0.59) + 0.14));
}

fragment float4 tonemapFragment(
    TonemapOut in [[stage_in]],
    texture2d<float> hdrColor [[texture(0)]],
    constant TonemapUniforms& uniforms [[buffer(0)]])
{
    constexpr sampler colorSampler(
        coord::normalized,
        address::clamp_to_edge,
        filter::nearest);
    float2 uv = in.position.xy / float2(hdrColor.get_width(), hdrColor.get_height());
    float3 color = hdrColor.sample(colorSampler, uv).rgb;
    color *= exp2(uniforms.exposure);
    return float4(acesToneMap(color), 1.0);
}

fragment float4 terrainFragment(
    VertexOut in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]],
    depth2d<float> shadowMap [[texture(1)]])
{
    float3 sunDirection = normalize(uniforms.sunDirection.xyz);
    float distanceToCamera = length(in.worldPosition - uniforms.cameraPosition.xyz);
    float3 color;

    if (uniforms.settings.x > 1.5) {
        float3 normal = normalize(in.normal);
        float3 bark = float3(0.095, 0.065, 0.040) * (0.85 + in.uv.y * 0.25);
        float3 canopy = mix(
            float3(0.018, 0.075, 0.025),
            float3(0.045, 0.125, 0.040),
            in.uv.y);
        float3 surface = in.uv.x < 0.5 ? bark : canopy;

        float4 lightClip =
            uniforms.lightViewProjection * float4(in.worldPosition, 1.0);
        float3 lightNdc = lightClip.xyz / lightClip.w;
        float2 shadowUv = float2(
            lightNdc.x * 0.5 + 0.5,
            0.5 - lightNdc.y * 0.5);
        float shadowVisibility = 1.0;
        if (all(shadowUv >= 0.0) && all(shadowUv <= 1.0) &&
            lightNdc.z >= 0.0 && lightNdc.z <= 1.0) {
            constexpr sampler shadowSampler(
                coord::normalized,
                address::clamp_to_edge,
                filter::linear,
                compare_func::less_equal);
            float texel = uniforms.shadowSettings.x;
            float depthBias =
                uniforms.shadowSettings.y * (1.0 + (1.0 - normal.y) * 2.0);
            float reference = lightNdc.z - depthBias;
            float visibleSamples = 0.0;
            for (int y = -1; y <= 1; ++y) {
                for (int x = -1; x <= 1; ++x) {
                    visibleSamples += shadowMap.sample_compare(
                        shadowSampler,
                        shadowUv + float2(x, y) * texel,
                        reference);
                }
            }
            shadowVisibility = visibleSamples / 9.0;
        }
        float diffuse = saturate(dot(normal, sunDirection));
        float ambient = 0.20 + 0.18 * saturate(normal.y);
        color = surface *
            (ambient + diffuse * mix(0.12, 0.78, shadowVisibility));
    } else if (uniforms.settings.x > 0.5) {
        float time = uniforms.settings.z;
        float2 flowA = float2(
            in.uv.x * 15.0,
            in.uv.y * 420.0 - time * 1.35);
        float2 flowB = float2(
            in.uv.x * 7.0 + 4.0,
            in.uv.y * 190.0 - time * 0.62);
        float slopeX =
            cos(flowA.x * 1.3 + flowA.y) * 0.080 +
            cos(flowB.y * 1.7 - flowB.x) * 0.120;
        float slopeZ =
            sin(flowA.y * 1.6 - flowA.x) * 0.150 +
            sin(flowB.x * 1.2 + flowB.y) * 0.220;
        float footprint = max(
            max(fwidth(flowA.x), fwidth(flowA.y)),
            max(fwidth(flowB.x), fwidth(flowB.y)));
        float detailFade = 1.0 - smoothstep(0.20, 0.90, footprint);
        float3 normal = normalize(float3(
            -slopeX * detailFade,
            1.0,
            -slopeZ * detailFade));

        float3 viewDirection =
            normalize(uniforms.cameraPosition.xyz - in.worldPosition);
        float3 reflected = reflect(viewDirection, normal);
        float fresnel = 0.02 + 0.98 * pow(
            1.0 - saturate(dot(viewDirection, normal)), 5.0);
        float3 reflection = skyRadiance(
            reflected,
            sunDirection,
            uniforms.skySettings.z,
            uniforms.skySettings.w,
            uniforms.settings.w,
            time);

        float edge = abs(in.uv.x * 2.0 - 1.0);
        float depthVariation = fbm(float2(
            in.uv.y * 70.0 - time * 0.25,
            edge * 4.0));
        float3 deepWater = float3(0.004, 0.018, 0.028);
        float3 shallowWater = float3(0.018, 0.085, 0.095);
        float3 water = mix(
            deepWater,
            shallowWater,
            0.26 + depthVariation * 0.52);
        color = mix(water, reflection, 0.16 + fresnel * 0.78);

        float glint = pow(saturate(dot(reflected, sunDirection)), 260.0);
        color += float3(1.0, 0.78, 0.46) * glint * 2.2;

        float foamNoise = valueNoise(float2(
            in.uv.x * 22.0,
            in.uv.y * 330.0 - time * 1.8));
        float foamBand =
            smoothstep(0.76, 0.99, edge) * (0.28 + foamNoise * 0.72);
        color = mix(color, float3(0.52, 0.60, 0.62), foamBand * 0.48);
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
        float4 lightClip =
            uniforms.lightViewProjection * float4(in.worldPosition, 1.0);
        float3 lightNdc = lightClip.xyz / lightClip.w;
        float2 shadowUv = float2(
            lightNdc.x * 0.5 + 0.5,
            0.5 - lightNdc.y * 0.5);
        float shadowVisibility = 1.0;
        if (all(shadowUv >= 0.0) && all(shadowUv <= 1.0) &&
            lightNdc.z >= 0.0 && lightNdc.z <= 1.0) {
            constexpr sampler shadowSampler(
                coord::normalized,
                address::clamp_to_edge,
                filter::linear,
                compare_func::less_equal);
            float texel = uniforms.shadowSettings.x;
            float depthBias =
                uniforms.shadowSettings.y * (1.0 + (1.0 - normal.y) * 2.0);
            float reference = lightNdc.z - depthBias;
            float visibleSamples = 0.0;
            for (int y = -1; y <= 1; ++y) {
                for (int x = -1; x <= 1; ++x) {
                    float2 offset = float2(x, y) * texel;
                    visibleSamples += shadowMap.sample_compare(
                        shadowSampler,
                        shadowUv + offset,
                        reference);
                }
            }
            shadowVisibility = visibleSamples / 9.0;
        }
        float directVisibility = mix(0.16, 1.0, shadowVisibility);
        color *= skyLight + diffuse * 0.72 * directVisibility;
        float sheen = pow(saturate(dot(normal, sunDirection)), 48.0) * rock * 0.10;
        color += float3(0.85, 0.78, 0.62) * sheen;
    }

    float fog = 1.0 - exp(-uniforms.settings.y * distanceToCamera);
    fog += smoothstep(900.0, 4800.0, distanceToCamera) * 0.30;
    color = mix(color, float3(0.28, 0.35, 0.43), saturate(fog) * 0.80);
    return float4(max(color, 0.0), 1.0);
}
)METAL";

}  // namespace natureview::platform
