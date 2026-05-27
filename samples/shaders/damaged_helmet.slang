struct HelmetVertexInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float4 tangent  : TANGENT;
};

struct HelmetUniforms {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4 cameraPosition;
    float4 lightDirectionAmbient;
    float4 baseColorFactor;
    float4 emissiveFactorNormalScale;
    float4 metallicRoughnessOcclusion;
    float4 alphaModeCutoffReflectionExposure;
    float4 debugViewGamma;
    float4 padding2;
};

ConstantBuffer<HelmetUniforms> helmetParams;

Texture2D<float4> baseColorTexture;
Texture2D<float4> metalRoughnessTexture;
Texture2D<float4> emissiveTexture;
Texture2D<float4> occlusionTexture;
Texture2D<float4> normalTexture;
SamplerState materialSampler;

struct HelmetVertexOutput {
    float4 sv_position : SV_Position;
    float3 worldPosition : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 tangent : TANGENT;
};

[shader("vertex")]
HelmetVertexOutput helmetVertexMain(HelmetVertexInput input) {
    HelmetVertexOutput output;
    float4 worldPosition = mul(float4(input.position, 1.0), helmetParams.model);
    float4 viewPosition = mul(worldPosition, helmetParams.view);
    output.sv_position = mul(viewPosition, helmetParams.projection);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0), helmetParams.model).xyz);
    output.uv = input.uv;
    output.tangent = float4(normalize(mul(float4(input.tangent.xyz, 0.0), helmetParams.model).xyz), input.tangent.w);
    return output;
}

static const float kPi = 3.14159265359;
static const float kDielectricSpecular = 0.04;

float3 F_Schlick(float3 f0, float3 f90, float vDotH) {
    float x = saturate(1.0 - vDotH);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

float V_GGX(float nDotL, float nDotV, float alphaRoughness) {
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float ggxV = nDotL * sqrt(nDotV * nDotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float ggxL = nDotV * sqrt(nDotL * nDotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float ggx = ggxV + ggxL;
    return ggx > 0.0 ? 0.5 / ggx : 0.0;
}

float D_GGX(float nDotH, float alphaRoughness) {
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = nDotH * nDotH * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / max(kPi * f * f, 0.0001);
}

float3 BRDF_lambertian(float3 diffuseColor) {
    return diffuseColor / kPi;
}

float3 BRDF_specularGGX(float alphaRoughness, float nDotL, float nDotV, float nDotH) {
    return float3(V_GGX(nDotL, nDotV, alphaRoughness) * D_GGX(nDotH, alphaRoughness));
}

float3 toneMapKhronosPbrNeutral(float3 color) {
    const float startCompression = 0.76;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) {
        return saturate(color);
    }
    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / max(peak, 0.0001);
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return saturate(lerp(color, float3(newPeak, newPeak, newPeak), g));
}

float studioCard(float3 direction, float3 cardDirection, float exponent, float softness) {
    return pow(saturate(dot(direction, normalize(cardDirection)) * softness + (1.0 - softness)), exponent);
}

// Sample-local neutral lighting approximation; Kera does not expose renderer-owned IBL in this slice.
float3 sampleStudioRadiance(float3 direction, float roughness) {
    float sky = saturate(direction.y * 0.5 + 0.5);
    float horizon = pow(1.0 - abs(direction.y), 2.0);
    float gloss = 1.0 - saturate(roughness);
    float sharpness = lerp(0.45, 1.0, gloss);
    float frontKey = studioCard(direction, float3(-0.48, 0.2, -0.86), lerp(7.0, 34.0, gloss), sharpness);
    float sideStrip = studioCard(direction, float3(0.9, 0.12, -0.34), lerp(12.0, 72.0, gloss), sharpness);
    float topPanel = studioCard(direction, float3(-0.12, 0.94, -0.32), lerp(8.0, 48.0, gloss), sharpness);
    float lowerFill = studioCard(direction, float3(0.12, -0.45, -0.88), lerp(4.0, 18.0, gloss), 0.6);
    float3 dark = float3(0.08, 0.09, 0.1);
    float3 skyColor = float3(0.62, 0.64, 0.66);
    float3 horizonColor = float3(0.88, 0.88, 0.86);
    float3 radiance = lerp(dark, skyColor, sky);
    radiance += horizonColor * horizon * 0.18;
    radiance += float3(1.0, 1.0, 1.0) * frontKey * lerp(0.32, 1.25, gloss);
    radiance += float3(0.92, 0.94, 0.96) * sideStrip * lerp(0.24, 0.95, gloss);
    radiance += float3(1.0, 1.0, 0.98) * topPanel * lerp(0.28, 0.9, gloss);
    radiance += float3(0.38, 0.52, 0.58) * lowerFill * 0.28;
    return radiance;
}

float luminance(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float4 materialDebugView(float debugMode, float2 uv, float3 albedo, float alpha, float metallic, float roughness,
                         float ambientOcclusion, float3 emissive, float3 normal, float3 tangent, float3 bitangent,
                         float3 normalDetail) {
    int mode = int(debugMode + 0.5);
    if (mode == 1) {
        return float4(albedo, alpha);
    }
    if (mode == 2) {
        return float4(normal * 0.5 + 0.5, 1.0);
    }
    if (mode == 3) {
        return float4(metallic, metallic, metallic, 1.0);
    }
    if (mode == 4) {
        return float4(roughness, roughness, roughness, 1.0);
    }
    if (mode == 5) {
        return float4(ambientOcclusion, ambientOcclusion, ambientOcclusion, 1.0);
    }
    if (mode == 6) {
        return float4(emissive, 1.0);
    }
    if (mode == 7) {
        return float4(tangent * 0.5 + 0.5, 1.0);
    }
    if (mode == 8) {
        return float4(bitangent * 0.5 + 0.5, 1.0);
    }
    if (mode == 9) {
        return float4(normalDetail * 0.5 + 0.5, 1.0);
    }
    if (mode == 10) {
        float ramp = saturate(uv.x);
        return float4(ramp, ramp, ramp, 1.0);
    }
    if (mode == 11) {
        float ramp = pow(saturate(uv.x), helmetParams.debugViewGamma.z);
        return float4(ramp, ramp, ramp, 1.0);
    }
    return float4(0.0, 0.0, 0.0, 0.0);
}

[shader("fragment")]
float4 helmetFragmentMain(HelmetVertexOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target {
    float4 baseColorSample = baseColorTexture.Sample(materialSampler, input.uv);
    float alpha = baseColorSample.a * helmetParams.baseColorFactor.a;
    if (helmetParams.alphaModeCutoffReflectionExposure.x > 0.5 &&
        helmetParams.alphaModeCutoffReflectionExposure.x < 1.5 &&
        alpha < helmetParams.alphaModeCutoffReflectionExposure.y) {
        discard;
    }
    if (helmetParams.alphaModeCutoffReflectionExposure.x < 1.5) {
        alpha = 1.0;
    }

    float4 metalRoughness = metalRoughnessTexture.Sample(materialSampler, input.uv);
    float3 emissive = emissiveTexture.Sample(materialSampler, input.uv).rgb * helmetParams.emissiveFactorNormalScale.rgb;
    float ambientOcclusion = lerp(1.0, occlusionTexture.Sample(materialSampler, input.uv).r,
                                  helmetParams.metallicRoughnessOcclusion.z);
    float3 normalDetail = normalTexture.Sample(materialSampler, input.uv).xyz * 2.0 - 1.0;
    normalDetail.xy *= helmetParams.emissiveFactorNormalScale.w;

    float3 albedo = baseColorSample.rgb * helmetParams.baseColorFactor.rgb;
    float roughness = clamp(metalRoughness.g * helmetParams.metallicRoughnessOcclusion.y, 0.04, 1.0);
    float metallic = saturate(metalRoughness.b * helmetParams.metallicRoughnessOcclusion.x);
    float3 normal = normalize(input.normal);
    float3 tangent = normalize(input.tangent.xyz - normal * dot(normal, input.tangent.xyz));
    float3 bitangent = normalize(cross(normal, tangent) * input.tangent.w);
    normal = normalize(tangent * normalDetail.x + bitangent * normalDetail.y + normal * normalDetail.z);
    if (helmetParams.padding2.x > 0.5 && !isFrontFace) {
        normal *= -1.0;
        tangent *= -1.0;
        bitangent *= -1.0;
    }
    if (helmetParams.debugViewGamma.x > 0.5) {
        return materialDebugView(helmetParams.debugViewGamma.x, input.uv, albedo, alpha, metallic, roughness,
                                 ambientOcclusion, emissive, normal, tangent, bitangent, normalDetail);
    }

    float3 lightDirection = normalize(-helmetParams.lightDirectionAmbient.xyz);
    float ambient = helmetParams.lightDirectionAmbient.w;
    float3 viewDirection = normalize(helmetParams.cameraPosition.xyz - input.worldPosition);
    float3 halfVector = normalize(lightDirection + viewDirection);
    float3 reflectionDirection = reflect(-viewDirection, normal);

    float nDotL = saturate(dot(normal, lightDirection));
    float nDotV = saturate(dot(normal, viewDirection));
    float nDotH = saturate(dot(normal, halfVector));
    float vDotH = saturate(dot(viewDirection, halfVector));

    float3 dielectricSpecular = float3(kDielectricSpecular, kDielectricSpecular, kDielectricSpecular);
    float3 reflectance90 = float3(1.0, 1.0, 1.0);
    float alphaRoughness = max(roughness * roughness, 0.001);

    float3 lightColor = float3(1.55, 1.55, 1.55);
    float3 dielectricFresnel = F_Schlick(dielectricSpecular, reflectance90, vDotH);
    float3 metalFresnel = F_Schlick(albedo, reflectance90, vDotH);
    float3 lightDiffuse = lightColor * nDotL * BRDF_lambertian(albedo);
    float3 lightSpecular = lightColor * nDotL * BRDF_specularGGX(alphaRoughness, nDotL, nDotV, nDotH);
    float3 dielectricBrdf = lerp(lightDiffuse, lightSpecular, dielectricFresnel);
    float3 metalBrdf = metalFresnel * lightSpecular;
    float3 directLight = lerp(dielectricBrdf, metalBrdf, metallic);

    float3 ambientRadiance = sampleStudioRadiance(normal, 1.0) * ambient;
    float3 ambientDiffuse = albedo * ambientRadiance * ambientOcclusion * (1.0 - metallic);

    float3 dielectricEnvironmentFresnel = F_Schlick(dielectricSpecular, reflectance90, nDotV);
    float3 metalEnvironmentFresnel = F_Schlick(albedo, reflectance90, nDotV);
    float3 environmentRadiance = sampleStudioRadiance(reflectionDirection, roughness);
    float specularOcclusion = lerp(1.0, ambientOcclusion, saturate(roughness * 0.65));
    float3 dielectricEnvironment = lerp(ambientDiffuse, environmentRadiance, dielectricEnvironmentFresnel);
    float3 metalEnvironment = environmentRadiance * metalEnvironmentFresnel;
    float3 ambientSpecular = lerp(dielectricEnvironment, metalEnvironment, metallic) * specularOcclusion *
                             helmetParams.alphaModeCutoffReflectionExposure.z * (0.08 + (1.0 - roughness) * 0.52);

    float3 color = directLight + ambientSpecular + emissive;
    color = toneMapKhronosPbrNeutral(color * helmetParams.alphaModeCutoffReflectionExposure.w);
    return float4(color, alpha);
}

struct FullscreenVertexInput {
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct FullscreenVertexOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
};

Texture2D<float4> sceneTexture;

[shader("vertex")]
FullscreenVertexOutput fullscreenVertexMain(FullscreenVertexInput input) {
    FullscreenVertexOutput output;
    output.sv_position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fullscreenFragmentMain(float2 uv : TEXCOORD0) : SV_Target {
    return sceneTexture.Sample(materialSampler, uv);
}
