// Copyright (c) - SurgeTechnologies - All rights reserved
[SurgeShader: Vertex]
#version 450 core
const int MAX_CASCADE_COUNT = 4;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;

layout(push_constant) uniform PushConstants
{
    mat4 Transform;
    mat4 ViewProjectionMatrix;

} uMesh;

// Set 0 belongs to the renderer
layout(set = 0, binding = 0) uniform Camera
{
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
    mat4 ViewProjectionMatrix;

} uCameraData;
layout(set = 0, binding = 1) uniform RendererData
{
    uint TilesCountX; // Forward+
    int ShowLightComplexity;
    float _Padding_1;
    float _Padding_2;
} uRendererData;

struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
    vec3 WorldPos;

    vec3 ViewSpacePos;
    vec4 LightSpaceVector[MAX_CASCADE_COUNT];
};
layout(location = 0) out VertexOutput vOutput;

layout(set = 3, binding = 0) uniform ShadowParams
{
    vec4 CascadeEnds;
    mat4 LightSpaceMatrix[4];
    int ShowCascades;
    uint CascadeCount;

    int ShadowQuality;
    int _Padding_;

}uShadowParams;

void main()
{
    vOutput.TexCoord = aTexCoord;
    vOutput.Tangent   = mat3(uMesh.Transform) * normalize(aTangent);
    vOutput.BiTangent = mat3(uMesh.Transform) * normalize(aBiTangent);
    vOutput.Normal    = mat3(uMesh.Transform) * normalize(aNormal);

    vOutput.WorldPos = vec3(uMesh.Transform * vec4(aPosition, 1.0));
    vOutput.ViewSpacePos = vec3(uCameraData.ViewMatrix * vec4(vOutput.WorldPos, 1.0));

    for(uint i = 0; i < uShadowParams.CascadeCount; i++)
        vOutput.LightSpaceVector[i] = uShadowParams.LightSpaceMatrix[i] * vec4(vOutput.WorldPos, 1.0);

    gl_Position = uMesh.ViewProjectionMatrix * vec4(vOutput.WorldPos, 1.0);
}

[SurgeShader: Pixel]
#version 450 core

const int MAX_CASCADE_COUNT = 4;

// ---------- Stage Inputs ----------
struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
    vec3 WorldPos;

    vec3 ViewSpacePos;
    vec4 LightSpaceVector[MAX_CASCADE_COUNT];
};
layout(location = 0) in VertexOutput vInput;

// ---------- Stage Outputs ----------
layout(location = 0) out vec4 FinalColor;

// ---------- Constants ----------
const float PI = 3.141592;
const float Epsilon = 0.00001;
const vec3 DielectricF0 = vec3(0.04); // Constant normal incidence Fresnel factor for all dielectrics.

// ---------- Lights ----------
struct PointLight
{
    vec3 Position;
    float Intensity;

    vec3 Color;
    float Radius;

    float Falloff;
    int _Padding_;
    int _Padding_1;
    int _Padding_2;
};
struct DirectionalLight
{
    vec3 Direction;
    float Intensity;

    vec3 Color;
    float Size;
};

// Set 0 belongs to the renderer
layout(set = 0, binding = 0) uniform Camera
{
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
    mat4 ViewProjectionMatrix;

} uCameraData;
layout(set = 0, binding = 1) uniform RendererData
{
    uint TilesCountX; // Forward+
    int ShowLightComplexity;
    float _Padding_1;
    float _Padding_2;
} uRendererData;
layout(set = 0, binding = 2) uniform Lights
{
    vec3 CameraPosition;
    int PointLightCount;

    PointLight PointLights[1024];
    DirectionalLight DirLight;

} uLights;
// Forward+
layout(set = 0, binding = 3) readonly buffer VisibleLightIndicesBuffer
{
    int Indices[];

} sVisibleLightIndicesBuffer;
layout(set = 0, binding = 4) uniform sampler2D uPreDepthMap;

// Material - Set 1 and 2
layout(set = 1, binding = 0) uniform Material
{
    vec3 Albedo;
    float Metalness;

    float Roughness;
    int UseNormalMap;

    int _Padding_;
    int _Padding_1;

} uMaterial;
layout(set = 2, binding = 0) uniform sampler2D AlbedoMap;
layout(set = 2, binding = 1) uniform sampler2D NormalMap;
layout(set = 2, binding = 2) uniform sampler2D RoughnessMap;
layout(set = 2, binding = 3) uniform sampler2D MetalnessMap;

// Shadows - Set 3
layout(set = 3, binding = 0) uniform ShadowParams
{
    vec4 CascadeEnds;
    mat4 LightSpaceMatrix[4];
    int ShowCascades;
    uint CascadeCount;

    int ShadowQuality;
    int _Padding_;

}uShadowParams;
layout(set = 3, binding = 1) uniform sampler2D ShadowMap1;
layout(set = 3, binding = 2) uniform sampler2D ShadowMap2;
layout(set = 3, binding = 3) uniform sampler2D ShadowMap3;
layout(set = 3, binding = 4) uniform sampler2D ShadowMap4;

struct PBRParameters
{
    vec3 Albedo;
    float Metalness;
    float Roughness;

    vec3 Normal;
    vec3 View;
};
PBRParameters gPBRParams;

//------------------------------------------------------------------------------
// The following BRDF has been adapted from the Filament material system
// and Strontium (an open-source multimedia engine).
//------------------------------------------------------------------------------
float GGXNormal(float nDotH, float actualRoughness)
{
    float a = nDotH * actualRoughness;
    float k = actualRoughness / (1.0 - nDotH * nDotH + a * a);
    return k * k * (1.0 / PI);
}

// Fast visibility term. Incorrect as it approximates the two square roots.
float GGXViewFast(float nDotV, float nDotL, float actualRoughness)
{
    float a = actualRoughness;
    float vVGGX = nDotL * (nDotV * (1.0 - a) + a);
    float lVGGX = nDotV * (nDotL * (1.0 - a) + a);
    return 0.5 / max(vVGGX + lVGGX, 1e-5);
}

// Schlick approximation for the Fresnel factor.
vec3 FresnelSchlick(float vDotH, vec3 f0, vec3 f90)
{
    float p = 1.0 - vDotH;
    return f0 + (f90 - f0) * p * p * p * p * p;
}

// Cook-Torrance specular for the specular component of the BRDF.
vec3 CookTorranceSpecular(float nDotH, float lDotH, float nDotV, float nDotL,
                          float vDotH, float actualRoughness, vec3 f0, vec3 f90)
{
    float D = GGXNormal(nDotH, actualRoughness);
    vec3 F = FresnelSchlick(vDotH, f0, f90);
    float V = GGXViewFast(nDotV, nDotL, actualRoughness);
    return D * F * V;
}

// Lambertian diffuse for the diffuse component of the BRDF. Corrected to guarantee
// energy is conserved.
vec3 CorrectedLambertianDiffuse(vec3 f0, vec3 f90, float vDotH, float lDotH,
                                vec3 diffuseAlbedo)
{
    // Making the assumption that the external medium is air (IOR of 1).
    vec3 iorExtern = vec3(1.0);
    // Calculating the IOR of the medium using f0.
    vec3 iorIntern = (vec3(1.0) - sqrt(f0)) / (vec3(1.0) + sqrt(f0));
    // Ratio of the IORs.
    vec3 iorRatio = iorExtern / iorIntern;

    // Compute the incoming and outgoing Fresnel factors.
    vec3 fIncoming = FresnelSchlick(lDotH, f0, f90);
    vec3 fOutgoing = FresnelSchlick(vDotH, f0, f90);

    // Compute the fraction of light which doesn't get reflected back into the
    // medium for TIR.
    vec3 rExtern = PI * (20.0 * f0 + 1.0) / 21.0;
    // Use rExtern to compute the fraction of light which gets reflected back into
    // the medium for TIR.
    vec3 rIntern = vec3(1.0) - (iorRatio * iorRatio * (vec3(1.0) - rExtern));

    // The TIR contribution.
    vec3 tirDiffuse = vec3(1.0) - (rIntern * diffuseAlbedo);

    // The final diffuse BRDF.
    return (iorRatio * iorRatio) * diffuseAlbedo * (vec3(1.0) - fIncoming)
           * (vec3(1.0) - fOutgoing) / (PI * tirDiffuse);
}

vec3 FilamentBRDF(vec3 l, vec3 v, vec3 n, float roughness, float metallic, vec3 dielectricF0,
                  vec3 metallicF0, vec3 f90, vec3 diffuseAlbedo)
{
    vec3 h = normalize(v + l);

    float nDotV = max(abs(dot(n, v)), 1e-5);
    float nDotL = clamp(dot(n, l), 1e-5, 1.0);
    float nDotH = clamp(dot(n, h), 1e-5, 1.0);
    float lDotH = clamp(dot(l, h), 1e-5, 1.0);
    float vDotH = clamp(dot(v, h), 1e-5, 1.0);

    float clampedRoughness = max(roughness, 0.045);
    float actualRoughness = clampedRoughness * clampedRoughness;

    vec3 fs = CookTorranceSpecular(nDotH, lDotH, nDotV, nDotL, vDotH,
                                   actualRoughness, dielectricF0, f90);
    vec3 fd = CorrectedLambertianDiffuse(dielectricF0, f90, vDotH, lDotH,
                                         diffuseAlbedo);
    vec3 dielectricBRDF = fs + fd;

    vec3 metallicBRDF = CookTorranceSpecular(nDotH, lDotH, nDotV, nDotL, vDotH,
                                             actualRoughness, metallicF0, f90);

    return mix(dielectricBRDF, metallicBRDF, metallic);
}
//------------------------------------------------------------------------------

vec3 CalculateNormal()
{
   vec3 newNormal;
   if (uMaterial.UseNormalMap == 1)
   {
        vec3 normal = normalize(vInput.Normal);
        vec3 tangent = normalize(vInput.Tangent);
        vec3 bitangent = normalize(vInput.BiTangent);

        vec3 bumpMapNormal = texture(NormalMap, vInput.TexCoord).xyz;
        bumpMapNormal = 2.0 * bumpMapNormal - vec3(1.0);

        mat3 TBN = mat3(tangent, bitangent, normal);
        newNormal = TBN * bumpMapNormal;
        newNormal = normalize(newNormal);
   }
   else
   {
        newNormal = normalize(vInput.Normal);
   }
   return newNormal;
}

int GetLightBufferIndex(int i)
{
    ivec2 tileID = ivec2(gl_FragCoord) / ivec2(16, 16); //Current Fragment position / Tile count
    uint index = tileID.y * uRendererData.TilesCountX + tileID.x;

    uint offset = index * 1024; // Max lights 1024
    return sVisibleLightIndicesBuffer.Indices[offset + i];
}

int GetPointLightCount()
{
    int result = 0;
    for (int i = 0; i < uLights.PointLightCount; i++)
    {
        uint lightIndex = GetLightBufferIndex(i);
        if (lightIndex == -1)
            break;

        result++;
    }

    return result;
}

vec3 CalculatePointLights(vec3 dielectricF0, vec3 metallicF0)
{
    vec3 result = vec3(0.0);
    for (int i = 0; i < uLights.PointLightCount; i++)
    {
        uint lightIndex = GetLightBufferIndex(i);
        if (lightIndex == -1)
            break;

        PointLight light = uLights.PointLights[lightIndex];

        vec3 lightVector = light.Position - vInput.WorldPos;
        vec3 lightDir = normalize(lightVector);
        float lightDistance = length(lightVector);

        float attenuation = clamp(1.0 - (lightDistance * lightDistance) / (light.Radius * light.Radius), 0.0, 1.0);
        attenuation *= mix(attenuation, 1.0, light.Falloff);

        vec3 brdf = FilamentBRDF(lightDir, gPBRParams.View, gPBRParams.Normal,
                                 gPBRParams.Roughness, gPBRParams.Metalness, dielectricF0,
                                 metallicF0, vec3(1.0), gPBRParams.Albedo);
        float nDotL = max(dot(gPBRParams.Normal, lightDir), 0.0);
        result += max(brdf * light.Intensity * light.Color * attenuation * nDotL, 0.0.xxx);
    }
    return result;
}

//Employ stochastic sampling
const int PCF_SAMPLES = 64;

const vec2 gPoissonDisk[64] = vec2[](
    vec2(-0.884081, 0.124488),
    vec2(-0.714377, 0.027940),
    vec2(-0.747945, 0.227922),
    vec2(-0.939609, 0.243634),
    vec2(-0.985465, 0.045534),
    vec2(-0.861367, -0.136222),
    vec2(-0.881934, 0.396908),
    vec2(-0.466938, 0.014526),
    vec2(-0.558207, 0.212662),
    vec2(-0.578447, -0.095822),
    vec2(-0.740266, -0.095631),
    vec2(-0.751681, 0.472604),
    vec2(-0.553147, -0.243177),
    vec2(-0.674762, -0.330730),
    vec2(-0.402765, -0.122087),
    vec2(-0.319776, -0.312166),
    vec2(-0.413923, -0.439757),
    vec2(-0.979153, -0.201245),
    vec2(-0.865579, -0.288695),
    vec2(-0.243704, -0.186378),
    vec2(-0.294920, -0.055748),
    vec2(-0.604452, -0.544251),
    vec2(-0.418056, -0.587679),
    vec2(-0.549156, -0.415877),
    vec2(-0.238080, -0.611761),
    vec2(-0.267004, -0.459702),
    vec2(-0.100006, -0.229116),
    vec2(-0.101928, -0.380382),
    vec2(-0.681467, -0.700773),
    vec2(-0.763488, -0.543386),
    vec2(-0.549030, -0.750749),
    vec2(-0.809045, -0.408738),
    vec2(-0.388134, -0.773448),
    vec2(-0.429392, -0.894892),
    vec2(-0.131597, 0.065058),
    vec2(-0.275002, 0.102922),
    vec2(-0.106117, -0.068327),
    vec2(-0.294586, -0.891515),
    vec2(-0.629418, 0.379387),
    vec2(-0.407257, 0.339748),
    vec2(0.071650, -0.384284),
    vec2(0.022018, -0.263793),
    vec2(0.003879, -0.136073),
    vec2(-0.137533, -0.767844),
    vec2(-0.050874, -0.906068),
    vec2(0.114133, -0.070053),
    vec2(0.163314, -0.217231),
    vec2(-0.100262, -0.587992),
    vec2(-0.004942, 0.125368),
    vec2(0.035302, -0.619310),
    vec2(0.195646, -0.459022),
    vec2(0.303969, -0.346362),
    vec2(-0.678118, 0.685099),
    vec2(-0.628418, 0.507978),
    vec2(-0.508473, 0.458753),
    vec2(0.032134, -0.782030),
    vec2(0.122595, 0.280353),
    vec2(-0.043643, 0.312119),
    vec2(0.132993, 0.085170),
    vec2(-0.192106, 0.285848),
    vec2(0.183621, -0.713242),
    vec2(0.265220, -0.596716),
    vec2(-0.009628, -0.483058),
    vec2(-0.018516, 0.435703)
);
vec2 SamplePoissonDisk(uint index)
{
  return gPoissonDisk[index % 64];
}

// Interleaved gradient noise from:
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float interleavedGradientNoise(vec2 uvs)
{
  const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
  return fract(magic.z * fract(dot(uvs, magic.xy)));
}

// Create a rotation matrix with the noise function above.
mat2 randomRotation(vec2 uvs)
{
  float theta = 2.0 * PI * interleavedGradientNoise(uvs);
  float sinTheta = sin(theta);
  float cosTheta = cos(theta);
  return mat2(cosTheta, sinTheta, -sinTheta, cosTheta);
}

float GetShadowBias()
{
    const float minBias = 0.002;
    float bias = max(minBias * (1.0 - dot(gPBRParams.Normal, uLights.DirLight.Direction)), minBias);
    return bias;
}

float SampleShadowMap(int cascadeIndex, vec2 texCoords)
{
    if (cascadeIndex == 0)
        return texture(ShadowMap1, texCoords).r;
    if (cascadeIndex == 1)
        return texture(ShadowMap2, texCoords).r;
    if (cascadeIndex == 2)
        return texture(ShadowMap3, texCoords).r;
    if (cascadeIndex == 3)
        return texture(ShadowMap4, texCoords).r;
}

float MediumShadowsDirectionalLight(int cascade, vec3 shadowCoords)
{
    float bias = GetShadowBias();
    float sum = 0.0;
    vec2 texelSize = 1.0 / textureSize(ShadowMap1, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float z = SampleShadowMap(cascade, (shadowCoords.xy * 0.5 + 0.5) + vec2(x, y) * texelSize).x;
            sum += shadowCoords.z - bias > z ? 0.0 : 1.0;
        }
    }
    sum /= 9.0;
    return sum;
}

float HardShadowsDirectionalLight(int cascade, vec3 shadowCoords)
{
    float bias = GetShadowBias();
    float z = SampleShadowMap(cascade, shadowCoords.xy * 0.5 + 0.5).x;
    float result = shadowCoords.z - bias > z ? 0.0 : 1.0;
    return result;
}

float PCFDirectionalLight(int cascade, vec3 shadowCoords, float radius)
{
    float bias = GetShadowBias();
    float sum = 0.0;
    vec2 texelSize = 1.0 / textureSize(ShadowMap1, 0);

    mat2 rot = randomRotation(gl_FragCoord.xy);

    for (int i = 0; i < PCF_SAMPLES; i++)
    {
        vec2 offset = rot * gPoissonDisk[i] * texelSize * radius;
        float z = SampleShadowMap(cascade, shadowCoords.xy * 0.5 + 0.5 + offset).x;
        sum += shadowCoords.z - bias > z ? 0.0 : 1.0;
    }
    return sum / float(PCF_SAMPLES);
}

float CalculateSearchWidth(float receiverDepth, DirectionalLight light)
{
    return light.Size * receiverDepth;
}

float CalcBlockerDistance(int cascade, vec3 shadowCoords, DirectionalLight light)
{
    float bias = GetShadowBias();
    vec2 texel = 1.0 / textureSize(ShadowMap1, 0).xy;
    float blockerDistances = 0.0;
    float numBlockerDistances = 0.0;
    float receiverDepth = shadowCoords.z;

    mat2 rot = randomRotation(gl_FragCoord.xy);

    float searchWidth = CalculateSearchWidth(receiverDepth, light);
    for (uint i = 0; i < PCF_SAMPLES; i++)
    {
      vec2 disk = rot * SamplePoissonDisk(i) * searchWidth;

      float blockerDepth = SampleShadowMap(cascade, shadowCoords.xy * 0.5 + 0.5 + disk * texel).r;
      if (blockerDepth < receiverDepth - bias)
      {
        numBlockerDistances += 1.0;
        blockerDistances += blockerDepth;
      }
    }

    if (numBlockerDistances > 0.0)
      return blockerDistances / numBlockerDistances;
    else
      return -1.0;
}

float CalcPCFKernelSize(int cascade, vec3 shadowCoords, DirectionalLight light)
{
    float receiverDepth = shadowCoords.z;
    float blockerDepth = CalcBlockerDistance(cascade, shadowCoords, light);

    float kernelSize = 1.0;
    if (blockerDepth > 0.0)
    {
      float penumbraWidth = light.Size * (receiverDepth - blockerDepth) / blockerDepth;
      kernelSize = penumbraWidth / receiverDepth;
    }

    return kernelSize;
}

float PCSSDirectionalLight(int cascade, vec3 shadowCoords, DirectionalLight light)
{
    float kernelRadius = CalcPCFKernelSize(cascade, shadowCoords, light);
    return PCFDirectionalLight(cascade, shadowCoords, kernelRadius);
}

float CalculateShadows(int cascadeIndex, vec4 lightSpaceVector, vec3 normal,
                       vec3 direction, DirectionalLight light)
{
    vec3 shadowCoords = lightSpaceVector.xyz / lightSpaceVector.w; // Perspective divide
    float currentDepth = shadowCoords.z;
    if (currentDepth > 1.0)
        return 0.0;

    float shadow = 0.0;

    if (uShadowParams.ShadowQuality == 0)
        shadow = HardShadowsDirectionalLight(cascadeIndex, shadowCoords); // Hard Shadows
    else if(uShadowParams.ShadowQuality == 1)
        shadow = MediumShadowsDirectionalLight(cascadeIndex, shadowCoords);
    else if(uShadowParams.ShadowQuality == 2)
        shadow = PCFDirectionalLight(cascadeIndex, shadowCoords, 3.0);
    else if(uShadowParams.ShadowQuality == 3)
        shadow = PCSSDirectionalLight(cascadeIndex, shadowCoords, light);

    return shadow;
}

vec3 CalculateDirectionaLight(vec3 dielectricF0, vec3 metallicF0, out int cascadeIdx)
{
    DirectionalLight light = uLights.DirLight;

    vec3 lightDir = normalize(light.Direction);
    vec3 brdf = FilamentBRDF(lightDir, gPBRParams.View, gPBRParams.Normal,
                             gPBRParams.Roughness, gPBRParams.Metalness, dielectricF0,
                             metallicF0, vec3(1.0), gPBRParams.Albedo);

    float nDotL = max(dot(gPBRParams.Normal, lightDir), 0.0);
    vec3 result = brdf * light.Intensity * light.Color * nDotL;

    float shadow = 1.0;
    for (int j = 0; j < uShadowParams.CascadeCount; j++)
    {
        if (vInput.ViewSpacePos.z > -uShadowParams.CascadeEnds[j])
        {
            cascadeIdx = j;
            shadow = CalculateShadows(cascadeIdx, vInput.LightSpaceVector[j],
                                      gPBRParams.Normal, uLights.DirLight.Direction,
                                      light);
            break;
        }
    }
    return result * shadow;
}

vec3 GetGradient(float value)
{
    vec3 zero = vec3(0.0, 0.0, 0.0);
    vec3 white = vec3(0.0, 0.1, 0.9);
    vec3 red = vec3(0.2, 0.9, 0.4);
    vec3 blue = vec3(0.8, 0.8, 0.3);
    vec3 green = vec3(0.9, 0.2, 0.3);

    float step0 = 0.0;
    float step1 = 2.0;
    float step2 = 4.0;
    float step3 = 8.0;
    float step4 = 16.0;

    vec3 color = mix(zero, white, smoothstep(step0, step1, value));
    color = mix(color, white, smoothstep(step1, step2, value));
    color = mix(color, red, smoothstep(step1, step2, value));
    color = mix(color, blue, smoothstep(step2, step3, value));
    color = mix(color, green, smoothstep(step3, step4, value));

    return color;
}

void main()
{
    gPBRParams.Normal = CalculateNormal();
    gPBRParams.Albedo = texture(AlbedoMap, vInput.TexCoord).rgb * uMaterial.Albedo;

    // glTF packs roughness (G) and metalness (B) into one texture (so this shader is gltf only?)
    gPBRParams.Metalness = texture(MetalnessMap, vInput.TexCoord).b * uMaterial.Metalness;
    gPBRParams.Roughness = texture(RoughnessMap, vInput.TexCoord).g * uMaterial.Roughness;

    gPBRParams.View = normalize(uLights.CameraPosition - vInput.WorldPos);

    // Fresnel reflectance at normal incidence for metals. Had to separate the
    // dielectric and metallic BRDF's to ensure energy conservation.
    vec3 MetallicF0 = gPBRParams.Albedo * gPBRParams.Metalness;

    // Direct lighting calculation for analytical lights
    vec3 directLighting = vec3(0.0);
    directLighting += CalculatePointLights(DielectricF0, MetallicF0);
    int cascadeIndex;
    directLighting += CalculateDirectionaLight(DielectricF0, MetallicF0, cascadeIndex);

    // TODO: GI
    vec3 ambient = vec3(0.01) * gPBRParams.Albedo;

    FinalColor = vec4(ambient + directLighting, 1.0);

    if (uRendererData.ShowLightComplexity == 1)
    {
        int pointLightCount = GetPointLightCount();
        FinalColor.rgb = FinalColor.rgb + GetGradient(float(pointLightCount));
    }

    if (uShadowParams.ShowCascades == 1)
    {
        switch (cascadeIndex)
        {
            case 0:
                FinalColor.rgb *= vec3(1.0, 0.25, 0.25);
                break;
            case 1:
                FinalColor.rgb *= vec3(0.25, 1.0, 0.25);
                break;
            case 2:
                FinalColor.rgb *= vec3(0.25, 0.25, 1.0);
                break;
            case 3:
                FinalColor.rgb *= vec3(1.0, 1.0, 0.25);
                break;
        }
    }
    FinalColor = FinalColor / (FinalColor + vec4(1.0));
    FinalColor = pow(FinalColor, vec4(1.0/2.2));
}
