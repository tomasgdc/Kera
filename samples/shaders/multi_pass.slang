// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

struct SceneVertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

float4 shadowProjectedPosition(float3 position)
{
    const float2 shadow_offset = float2(0.35, 0.15) * (1.0 - position.z);
    return float4(position.xy + shadow_offset, position.z, 1.0);
}

struct SceneVertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
    float2 shadow_uv : TEXCOORD0;
    float shadow_depth : TEXCOORD1;
};

Texture2D<float> shadowTexture;
SamplerState shadowSampler;

[shader("vertex")]
SceneVertexOutput sceneVertexMain(SceneVertexInput input)
{
    SceneVertexOutput output;
    const float4 shadow_position = shadowProjectedPosition(input.position);
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    output.shadow_uv = shadow_position.xy * 0.5 + 0.5;
    output.shadow_depth = shadow_position.z;
    return output;
}

[shader("fragment")]
float4 sceneFragmentMain(SceneVertexOutput input) : SV_Target
{
    const float shadow_depth = shadowTexture.SampleLevel(shadowSampler, input.shadow_uv, 0.0);
    const float lighting = input.shadow_depth > shadow_depth + 0.015 ? 0.38 : 1.0;
    return float4(input.color * lighting, 1.0);
}

struct ShadowVertexInput
{
    float3 position : POSITION;
};

struct ShadowVertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
ShadowVertexOutput shadowVertexMain(ShadowVertexInput input)
{
    ShadowVertexOutput output;
    output.position = shadowProjectedPosition(input.position);
    return output;
}

[shader("fragment")]
void shadowFragmentMain()
{
}

struct GltfSceneVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 tangent : TANGENT;
};

struct GltfSceneUniforms
{
    float4x4 model;
    float4x4 normalMatrix;
    float4x4 view;
    float4x4 projection;
    float4x4 lightView;
    float4x4 lightProjection;
    float4 cameraPosition;
    float4 lightDirectionShadowBias;
    float4 baseColorFactor;
    float4 emissiveFactorNormalScale;
    float4 metallicRoughnessOcclusion;
    float4 alphaModeCutoffDoubleSided;
};

struct GltfShadowUniforms
{
    float4x4 model;
    float4x4 lightView;
    float4x4 lightProjection;
    float4 alphaModeCutoffBaseAlpha;
};

ConstantBuffer<GltfSceneUniforms> sceneParams;
ConstantBuffer<GltfShadowUniforms> shadowParams;
Texture2D<float4> baseColorTexture;
Texture2D<float4> metalRoughnessTexture;
Texture2D<float4> emissiveTexture;
Texture2D<float4> occlusionTexture;
Texture2D<float4> normalTexture;
SamplerState materialSampler;

struct GltfSceneVertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 tangent : TANGENT;
    float4 shadowPosition : TEXCOORD1;
};

[shader("vertex")]
GltfSceneVertexOutput gltfSceneVertexMain(GltfSceneVertexInput input)
{
    GltfSceneVertexOutput output;
    const float4 worldPosition = mul(float4(input.position, 1.0), sceneParams.model);
    const float4 viewPosition = mul(worldPosition, sceneParams.view);
    output.position = mul(viewPosition, sceneParams.projection);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0), sceneParams.normalMatrix).xyz);
    output.uv = input.uv;
    output.tangent = float4(normalize(mul(float4(input.tangent.xyz, 0.0), sceneParams.model).xyz), input.tangent.w);
    output.shadowPosition = mul(mul(worldPosition, sceneParams.lightView), sceneParams.lightProjection);
    return output;
}

float sponzaShadowFactor(float4 shadowPosition)
{
    const float3 shadowNdc = shadowPosition.xyz / max(shadowPosition.w, 0.0001);
    const float2 shadowUv = shadowNdc.xy * 0.5 + 0.5;
    if (shadowUv.x <= 0.0 || shadowUv.x >= 1.0 || shadowUv.y <= 0.0 || shadowUv.y >= 1.0 ||
        shadowNdc.z <= 0.0 || shadowNdc.z >= 1.0)
    {
        return 1.0;
    }

    uint shadowWidth = 0;
    uint shadowHeight = 0;
    shadowTexture.GetDimensions(shadowWidth, shadowHeight);
    const float2 texelSize = 1.0 / float2(max(shadowWidth, 1u), max(shadowHeight, 1u));
    float litSamples = 0.0;
    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            const float2 sampleUv = clamp(shadowUv + float2(offsetX, offsetY) * texelSize,
                                          float2(0.0, 0.0), float2(1.0, 1.0));
            const float storedDepth = shadowTexture.SampleLevel(shadowSampler, sampleUv, 0.0);
            litSamples += shadowNdc.z <= storedDepth + sceneParams.lightDirectionShadowBias.w ? 1.0 : 0.0;
        }
    }
    return lerp(0.42, 1.0, litSamples / 9.0);
}

[shader("fragment")]
float4 gltfSceneFragmentMain(GltfSceneVertexOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    const float4 baseColorSample = baseColorTexture.Sample(materialSampler, input.uv);
    float alpha = baseColorSample.a * sceneParams.baseColorFactor.a;
    const float alphaMode = sceneParams.alphaModeCutoffDoubleSided.x;
    if (alphaMode > 0.5 && alphaMode < 1.5 && alpha < sceneParams.alphaModeCutoffDoubleSided.y)
    {
        discard;
    }
    if (alphaMode < 1.5)
    {
        alpha = 1.0;
    }

    float3 normal = normalize(input.normal);
    float3 tangent = normalize(input.tangent.xyz - normal * dot(normal, input.tangent.xyz));
    float3 bitangent = normalize(cross(normal, tangent) * input.tangent.w);
    float3 normalDetail = normalTexture.Sample(materialSampler, input.uv).xyz * 2.0 - 1.0;
    normalDetail.xy *= sceneParams.emissiveFactorNormalScale.w;
    normal = normalize(tangent * normalDetail.x + bitangent * normalDetail.y + normal * normalDetail.z);
    if (sceneParams.alphaModeCutoffDoubleSided.z > 0.5 && !isFrontFace)
    {
        normal *= -1.0;
    }

    const float4 metalRoughness = metalRoughnessTexture.Sample(materialSampler, input.uv);
    const float3 albedo = baseColorSample.rgb * sceneParams.baseColorFactor.rgb;
    const float metallic = saturate(metalRoughness.b * sceneParams.metallicRoughnessOcclusion.x);
    const float roughness = clamp(metalRoughness.g * sceneParams.metallicRoughnessOcclusion.y, 0.08, 1.0);
    const float ambientOcclusion = lerp(1.0, occlusionTexture.Sample(materialSampler, input.uv).r,
                                        sceneParams.metallicRoughnessOcclusion.z);
    const float3 lightDirection = normalize(sceneParams.lightDirectionShadowBias.xyz);
    const float3 viewDirection = normalize(sceneParams.cameraPosition.xyz - input.worldPosition);
    const float3 halfVector = normalize(lightDirection + viewDirection);
    const float nDotL = saturate(dot(normal, lightDirection));
    const float nDotH = saturate(dot(normal, halfVector));
    const float shadow = sponzaShadowFactor(input.shadowPosition);
    const float3 diffuse = albedo * (0.10 * ambientOcclusion + nDotL * shadow * ambientOcclusion);
    const float3 specular = lerp(float3(0.04, 0.04, 0.04), albedo, metallic) *
                            pow(nDotH, lerp(96.0, 6.0, roughness)) * nDotL * shadow;
    const float3 emissive = emissiveTexture.Sample(materialSampler, input.uv).rgb *
                            sceneParams.emissiveFactorNormalScale.rgb;
    const float3 color = (diffuse + specular + emissive) / (diffuse + specular + emissive + 1.0);
    return float4(color, alpha);
}

struct GltfShadowVertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct GltfShadowVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
GltfShadowVertexOutput gltfShadowVertexMain(GltfShadowVertexInput input)
{
    GltfShadowVertexOutput output;
    const float4 worldPosition = mul(float4(input.position, 1.0), shadowParams.model);
    output.position = mul(mul(worldPosition, shadowParams.lightView), shadowParams.lightProjection);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
void gltfShadowFragmentMain(GltfShadowVertexOutput input)
{
    const float alpha =
        baseColorTexture.Sample(materialSampler, input.uv).a * shadowParams.alphaModeCutoffBaseAlpha.z;
    if (shadowParams.alphaModeCutoffBaseAlpha.x > 0.5 && shadowParams.alphaModeCutoffBaseAlpha.x < 1.5 &&
        alpha < shadowParams.alphaModeCutoffBaseAlpha.y)
    {
        discard;
    }
}

struct CompositeVertexInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct CompositeVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D<float4> sceneTexture;
SamplerState sceneSampler;
Texture2D<float> shadowPreviewTexture;
SamplerState shadowPreviewSampler;

[shader("vertex")]
CompositeVertexOutput compositeVertexMain(CompositeVertexInput input)
{
    CompositeVertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 compositeFragmentMain(CompositeVertexOutput input) : SV_Target
{
    return sceneTexture.Sample(sceneSampler, input.uv);
}

[shader("fragment")]
float4 shadowPreviewFragmentMain(CompositeVertexOutput input) : SV_Target
{
    const float depth = shadowPreviewTexture.SampleLevel(shadowPreviewSampler, input.uv, 0.0);
    const float preview = 1.0 - depth;
    return float4(preview, preview, preview, 1.0);
}

[shader("fragment")]
float4 shadowInsetFragmentMain(CompositeVertexOutput input) : SV_Target
{
    const float2 inset_min = float2(0.66, 0.05);
    const float2 inset_max = float2(0.95, 0.41);
    const float2 inset_size = inset_max - inset_min;
    const bool inside = all(input.uv >= inset_min) && all(input.uv <= inset_max);
    if (!inside)
    {
        return sceneTexture.Sample(sceneSampler, input.uv);
    }

    const float2 inset_uv = (input.uv - inset_min) / inset_size;
    const float depth = shadowPreviewTexture.SampleLevel(shadowPreviewSampler, inset_uv, 0.0);
    const float preview = 1.0 - depth;
    const float2 border_distance = min(inset_uv, 1.0 - inset_uv);
    const bool border = min(border_distance.x, border_distance.y) < 0.018;
    const float3 shade = float3(preview, preview, preview);
    return float4(border ? float3(0.95, 0.80, 0.20) : shade, 1.0);
}

Texture2D<float4> msaaReferenceTexture;
SamplerState msaaReferenceSampler;

[shader("fragment")]
float4 msaaComparisonFragmentMain(CompositeVertexOutput input) : SV_Target
{
    const float2 reference_min = float2(0.41, 0.06);
    const float2 reference_max = float2(0.67, 0.27);
    const float2 resolved_min = float2(0.70, 0.06);
    const float2 resolved_max = float2(0.96, 0.27);
    const float2 reference_size = reference_max - reference_min;
    const float2 resolved_size = resolved_max - resolved_min;
    const bool in_reference = all(input.uv >= reference_min) && all(input.uv <= reference_max);
    const bool in_resolved = all(input.uv >= resolved_min) && all(input.uv <= resolved_max);
    if (!in_reference && !in_resolved)
    {
        return sceneTexture.Sample(sceneSampler, input.uv);
    }

    const float2 pane_uv = in_reference ? (input.uv - reference_min) / reference_size
                                        : (input.uv - resolved_min) / resolved_size;
    const float2 source_uv = float2(0.50, 0.56) + (pane_uv - 0.5) * 0.0625;
    const float2 border_distance = min(pane_uv, 1.0 - pane_uv);
    const bool border = min(border_distance.x, border_distance.y) < 0.022;
    const float3 colour = in_reference
                              ? msaaReferenceTexture.SampleLevel(msaaReferenceSampler, source_uv, 0.0).rgb
                              : sceneTexture.SampleLevel(sceneSampler, source_uv, 0.0).rgb;
    const float3 border_colour = in_reference ? float3(0.92, 0.26, 0.20) : float3(0.20, 0.88, 0.38);
    return float4(border ? border_colour : colour, 1.0);
}

Texture2D<float4> shadowActiveTexture;
SamplerState shadowActiveSampler;
Texture2D<float4> shadowReferenceTexture;
SamplerState shadowReferenceSampler;

[shader("fragment")]
float4 shadowComparisonFragmentMain(CompositeVertexOutput input) : SV_Target
{
    const float2 reference_min = float2(0.41, 0.05);
    const float2 reference_max = float2(0.67, 0.31);
    const float2 active_min = float2(0.70, 0.05);
    const float2 active_max = float2(0.96, 0.31);
    const float2 reference_size = reference_max - reference_min;
    const float2 active_size = active_max - active_min;
    const bool in_reference = all(input.uv >= reference_min) && all(input.uv <= reference_max);
    const bool in_active = all(input.uv >= active_min) && all(input.uv <= active_max);
    if (!in_reference && !in_active)
    {
        return sceneTexture.Sample(sceneSampler, input.uv);
    }

    const float2 pane_uv = in_reference ? (input.uv - reference_min) / reference_size
                                        : (input.uv - active_min) / active_size;
    // Frame the diagonal wall-shadow transition in the Sponza diagnostic view.
    const float2 diagonal_wall_shadow_focus_uv = float2(0.50, 0.771);
    const float2 diagonal_wall_shadow_source_span = float2(0.160, 0.264);
    const float2 source_uv = diagonal_wall_shadow_focus_uv +
                             (pane_uv - 0.5) * diagonal_wall_shadow_source_span;
    const float2 border_distance = min(pane_uv, 1.0 - pane_uv);
    const bool border = min(border_distance.x, border_distance.y) < 0.022;
    const float3 colour = in_reference
                              ? shadowReferenceTexture.SampleLevel(shadowReferenceSampler, source_uv, 0.0).rgb
                              : shadowActiveTexture.SampleLevel(shadowActiveSampler, source_uv, 0.0).rgb;
    const float3 border_colour = in_reference ? float3(0.95, 0.43, 0.08) : float3(0.12, 0.80, 0.95);
    return float4(border ? border_colour : colour, 1.0);
}
