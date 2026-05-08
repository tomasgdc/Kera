struct VertexInput {
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct InstanceInput {
    float4x4 modelMatrix : TRANSFORM;
};

struct GeometryUniforms {
    float4x4 projection;
    float4x4 view;
};

ConstantBuffer<GeometryUniforms> geometryParams : register(b0, space0);

struct GeometryVertexOutput {
    float4 sv_position : SV_Position;
    float3 color       : COLOR;
};

[shader("vertex")]
GeometryVertexOutput geometryVertexMain(VertexInput vertex,
                                        InstanceInput instance) {
    GeometryVertexOutput output;
    float4 worldPos = mul(float4(vertex.position, 1.0), instance.modelMatrix);
    float4 viewPos = mul(worldPos, geometryParams.view);
    output.sv_position = mul(viewPos, geometryParams.projection);
    output.color = vertex.color;
    return output;
}

[shader("fragment")]
float4 geometryFragmentMain(float3 color : COLOR) : SV_Target {
    return float4(color, 1.0);
}

struct FullscreenVertexInput {
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct FullscreenVertexOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
};

[shader("vertex")]
FullscreenVertexOutput fullscreenVertexMain(FullscreenVertexInput input) {
    FullscreenVertexOutput output;
    output.sv_position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

struct LightData {
    float4 positionRadius;
    float4 colorIntensity;
};

struct LightingUniforms {
    float4 ambientTimeLightCount;
    LightData lights[64];
};

Texture2D<float4> sceneTexture : register(t1, space0);

SamplerState sceneSampler : register(s2, space0);

ConstantBuffer<LightingUniforms> lightingParams : register(b3, space0);

[shader("fragment")]
float4 lightingFragmentMain(float2 uv : TEXCOORD0) : SV_Target {
    float3 albedo = sceneTexture.Sample(sceneSampler, uv).rgb;
    float ambient = lightingParams.ambientTimeLightCount.x;
    uint lightCount = min((uint)lightingParams.ambientTimeLightCount.z, 64u);

    float3 lightAccum = float3(ambient, ambient, ambient);
    float3 glowAccum = float3(0.0, 0.0, 0.0);
    for (uint i = 0; i < lightCount; ++i) {
        LightData light = lightingParams.lights[i];
        float2 delta = uv - light.positionRadius.xy;
        float distanceToLight = length(delta);
        float radius = max(light.positionRadius.z, 0.001);
        float falloff = saturate(1.0 - distanceToLight / radius);
        float localLight = falloff * falloff;
        float glow = smoothstep(radius, 0.0, distanceToLight);
        float core = smoothstep(radius * 0.16, 0.0, distanceToLight);

        lightAccum += light.colorIntensity.rgb * light.colorIntensity.a * localLight * 1.8;
        glowAccum += light.colorIntensity.rgb * light.colorIntensity.a * (glow * 0.22 + core * 0.65);
    }

    float sceneMask = saturate(max(albedo.r, max(albedo.g, albedo.b)) * 6.0);
    float3 color = albedo * lightAccum + glowAccum * (0.35 + sceneMask * 0.45);
    return float4(saturate(color), 1.0);
}
