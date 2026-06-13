//SURGE:[Shader: Fragment]
#version 450
const int MAX_CASCADE_COUNT = 3;

struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
    vec3 WorldPos;
    vec3 ViewSpacePos;
    vec4 LightSpaceVectors[MAX_CASCADE_COUNT];
};
layout(location = 0) in VertexOutput vInput;
layout(location = 0) out vec4 FinalColor;

struct Light
{
    vec4 PositionType;   // xyz = pos/dir, w = type (0 = dir, 1 = point)
    vec3 Color;
    float Intensity;
    float Radius;
    float Falloff;
    float _pad1, _pad2;
};

// Push Constants
layout(push_constant) uniform PushConstants
{
    mat4 Transform;
    uint LightCount;
} uMesh;

// Set 0: Frame + Lights
layout(std140, set = 0, binding = 0) uniform FrameUBO
{
    mat4 View;
    mat4 ViewProjection;
    mat4 InverseViewProjection;
    vec3 CameraPos;
    float _pad;

} uFrame;
layout(std140, set = 0, binding = 1) readonly buffer Lights
{
    // GI PARAMETERS TODO: Move to somewhere else?
    vec3 SkyAmbient;
    float _pad1;
    vec3 HorizonAmbient;
    float _pad2;
    vec3 GroundAmbient;
    float _pad3;

    Light Lights[256];
} uLights;

// Set 1: Materials
layout(set = 1, binding = 0) uniform Material
{
    vec3 Albedo;
    float Metallic;
    float Roughness;
    float Reflectance;
    int UseAlbedoMap;
    int UseNormalMap;
    int UseMetallicMap;
    int UseRoughnessMap;

} uMaterial;
layout(set = 1, binding = 1) uniform sampler2D AlbedoMap;
layout(set = 1, binding = 2) uniform sampler2D NormalMap;
layout(set = 1, binding = 3) uniform sampler2D RoughnessMetallicMap;

// Set 3: Shadows
//3 cascades max for now
layout(set = 2, binding = 0) uniform sampler2DShadow uShadowMap0;
layout(set = 2, binding = 1) uniform sampler2DShadow uShadowMap1;
layout(set = 2, binding = 2) uniform sampler2DShadow uShadowMap2;
layout(set = 2, binding = 3) uniform ShadowUBO
{
    vec4 CascadeEnds;
    mat4 LightSpaceMatrix[MAX_CASCADE_COUNT];
    uint CascadeCount;
    int ShowCascades;
    float _pad0, pad1;
} uShadowParams;

struct PBRParameters
{
    vec3 Albedo;
    float Metallic;
    float Roughness;
    vec3 Normal;
    vec3 View;
};
PBRParameters gPBRParams;

const vec2 poissonDisk[4] = vec2[](vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725), vec2(-0.09418410,  0.92938870), vec2( 0.34495938,  0.29387760));
float SampleShadowMap(int cascadeIndex, vec3 texCoords)
{
    if (cascadeIndex == 0)
        return texture(uShadowMap0, texCoords).r;
    if (cascadeIndex == 1)
        return texture(uShadowMap1, texCoords).r;
    if (cascadeIndex == 2)
        return texture(uShadowMap2, texCoords).r;
}

float SampleShadow(int cascade, vec3 shadowCoord)
{
    vec2 uv = shadowCoord.xy * 0.5 + 0.5;
    float depth = shadowCoord.z;

    vec2 texelSize = vec2(1.0 / 2048.0);
    float shadow = 0.0;
    for (int i = 0; i < 4; i++)
        shadow += SampleShadowMap(cascade, vec3(uv + poissonDisk[i] * texelSize, depth));

    return shadow / 4.0;
}
//float SampleShadow(int cascade, vec3 shadowCoord) // Hard Shadows
//{
//    vec2 uv = shadowCoord.xy * 0.5 + 0.5;
//    float depth = shadowCoord.z;
//
//    return SampleShadowMap(cascade, vec3(uv, depth));
//}

// Energy conserving Blinn-Phong?
vec3 CalculateMobilePBR(Light light, vec3 N, vec3 V, vec3 fragPos)
{   
    vec3 L; // Light Vector
    float attenuation = 1.0;

    if (light.PositionType.w == 0.0)
        L = normalize(-light.PositionType.xyz); // Directional Light
    else 
    {
        // Point Light
        vec3 dir = light.PositionType.xyz - fragPos;
        float dist = length(dir);
        L = normalize(dir);

        // PHYSICAL FALLOFF CALCULATION
        // Inverse Square Falloff (The physical part)
        float distanceSquare = dist * dist;
        attenuation = 1.0 / max(distanceSquare, 0.0001); // Avoid div by zero

        // Windowing Function
        // Forces the light to reach 0 at the light.Radius distance
        float factor = dist / light.Radius;
        float window = clamp(1.0 - pow(factor, 4.0), 0.0, 1.0);
        window *= window;

        attenuation *= window;       
        attenuation = pow(attenuation, light.Falloff);
    }

    vec3 H = normalize(L + V); // Halfway Vector(H)
    float dotNH = max(dot(N, H), 0.0);
    float dotNL = max(dot(N, L), 0.0);

    // Map Roughness to Blinn-Phong exponent (Shininess)
    // Roughness^4 for a more linear artistic feel
    float alpha = clamp(gPBRParams.Roughness, 0.04, 1.0);
    float shininess = max(2.0 / (pow(alpha, 4.0)) - 2.0, 1.0);

    // Energy Conservation: Metals have no Diffuse
    // f0 represents the base reflectivity (at 0 degrees)
    // Non-metals (dielectrics) use a constant (usually 0.04), metals use Albedo
    vec3 f0 = vec3(0.04) * uMaterial.Reflectance;
    vec3 specColor = mix(f0, gPBRParams.Albedo, gPBRParams.Metallic);
    vec3 diffuseColor = gPBRParams.Albedo * (1.0 - gPBRParams.Metallic);

    // Normalized Blinn-Phong Specular
    // The (shininess + 8)/8 factor ensures the light energy stays consistent
    // as the highlight gets tighter.
    float specNormalization = (shininess + 8.0) / (8.0 * 3.14159);
    float specularTerm = pow(dotNH, shininess) * specNormalization;

    // Final Composition
    vec3 diffuse = (diffuseColor / 3.14159) * dotNL;
    vec3 specular = specColor * specularTerm * dotNL;

    vec3 lightIntensity = light.Color.rgb * light.Intensity;

    return (diffuse + specular) * lightIntensity * attenuation;
}

vec3 CalculateNormal()
{
    if (uMaterial.UseNormalMap == 1)
    {
        vec3 normal = normalize(vInput.Normal);
        vec3 tangent = normalize(vInput.Tangent);
        vec3 bitangent = normalize(vInput.BiTangent);
   
        vec3 bumpMapNormal = texture(NormalMap, vInput.TexCoord).xyz;
        bumpMapNormal = 2.0 * bumpMapNormal - vec3(1.0);
   
        mat3 TBN = mat3(tangent, bitangent, normal);
        vec3 newNormal = TBN * bumpMapNormal;
        
        return normalize(newNormal);
    }
    
    return normalize(vInput.Normal);
}

vec3 CalculateFastGI(vec3 N, vec3 V)
{
    // Hemispherical diffuse ambient
    float skyGroundWeight = N.y * 0.5 + 0.5;
    vec3  rawSkyColor = mix(uLights.HorizonAmbient, uLights.SkyAmbient, max(N.y, 0.0));
    vec3  totalAmbientDiffuse = mix(uLights.GroundAmbient, rawSkyColor, skyGroundWeight);

    // Metals have no diffuse
    vec3 diffuseAmbient = totalAmbientDiffuse * gPBRParams.Albedo * (1.0 - gPBRParams.Metallic);

    // Environment reflection direction
    vec3  R = reflect(-V, N);
    float reflSkyWeight = R.y * 0.5 + 0.5;
    vec3  rawReflColor = mix(uLights.HorizonAmbient, uLights.SkyAmbient, max(R.y, 0.0));
    vec3  environmentRefl = mix(uLights.GroundAmbient, rawReflColor, reflSkyWeight);

    vec3 f0 = vec3(0.04) * uMaterial.Reflectance;
    vec3 specColor = mix(f0, gPBRParams.Albedo, gPBRParams.Metallic);

    // Roughness blends BETWEEN mirror-like and hemispherical ambient
    // roughness = 0 -> sharp directional reflection
    // roughness = 1 -> hemispherical diffuse ambient (blurry, but NOT zero)
    // Metals at any roughness always get non-zero specularAmbient
    float smoothness = 1.0 - gPBRParams.Roughness;
    vec3  ambientEnv = mix(totalAmbientDiffuse, environmentRefl, smoothness * smoothness);
    vec3  specularAmbient = ambientEnv * specColor;

    return diffuseAmbient + specularAmbient;
}

vec3 VisuaLizeCascades(vec3 finalColor, int cascadeIndex)
{
    if (uShadowParams.ShowCascades == 1 && cascadeIndex != -1)
    {
        switch (cascadeIndex)
        {
            case 0: return finalColor *= vec3(1.0,  0.25, 0.25); // Red
            case 1: return finalColor *= vec3(0.25, 1.0,  0.25); // Green
            case 2: return finalColor *= vec3(1.0,  1.0,  0.25); // Yellow
            case 3: return finalColor *= vec3(0.25, 0.25, 1.0 ); // Blue
        }
    }
    return finalColor;
}

void main()
{
    if (uMaterial.UseAlbedoMap == 1)
    {
        vec4 tex = texture(AlbedoMap, vInput.TexCoord);
        // Bad on mobile
        //if (tex.a < 1.0)
        //    discard;

        gPBRParams.Albedo = tex.rgb * uMaterial.Albedo;
    }
    else
        gPBRParams.Albedo = uMaterial.Albedo;

    gPBRParams.Normal = CalculateNormal();
    uMaterial.UseMetallicMap == 1 ? gPBRParams.Metallic = texture(RoughnessMetallicMap, vInput.TexCoord).b * uMaterial.Metallic : gPBRParams.Metallic = uMaterial.Metallic;
    uMaterial.UseRoughnessMap == 1 ? gPBRParams.Roughness = texture(RoughnessMetallicMap, vInput.TexCoord).g * uMaterial.Roughness : gPBRParams.Roughness = uMaterial.Roughness;

    gPBRParams.View = normalize(uFrame.CameraPos - vInput.WorldPos);

    // GI
    vec3 ambient = CalculateFastGI(gPBRParams.Normal, gPBRParams.View);

    // Direct Lighting Accumulation
    vec3 directAccumulation = vec3(0.0);
    int cascadeIndex = -1; // -1 = No cascade

    for (uint i = 0; i < uMesh.LightCount; i++)
    {
        vec3 contribution = CalculateMobilePBR(uLights.Lights[i], gPBRParams.Normal, gPBRParams.View, vInput.WorldPos);

        if (uLights.Lights[i].PositionType.w == 0.0) // Directional light
        {
            float shadow = 1.0;
            for (int j = 0; j < uShadowParams.CascadeCount; j++)
            {
                if (vInput.ViewSpacePos.z > -uShadowParams.CascadeEnds[j])
                {
                    cascadeIndex = j;
                    vec3 shadowCoords = vInput.LightSpaceVectors[j].xyz / vInput.LightSpaceVectors[j].w;
                    shadow = SampleShadow(cascadeIndex, shadowCoords);
                    break;
                }
            }
            contribution *= shadow;
        }

        directAccumulation += contribution;
    }

    vec3 HDRColor = ambient + directAccumulation;
    FinalColor = vec4(VisuaLizeCascades(HDRColor, cascadeIndex), 1.0);
    //FinalColor = vec4(HDRColor, 1.0);
}

//SURGE:[Shader: Vertex]
#version 450
const int MAX_CASCADE_COUNT = 3;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 View;
    mat4 ViewProjection;
    mat4 InverseViewProjection;
    vec3 CameraPos;
    float _pad;

} uFrame;

layout(push_constant) uniform PushConstants
{
    mat4 Transform;
    uint LightCount;

} uMesh;

struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
    vec3 WorldPos;
    vec3 ViewSpacePos;
    vec4 LightSpaceVectors[MAX_CASCADE_COUNT];
};
layout(location = 0) out VertexOutput vOutput;

layout(set = 2, binding = 3) uniform ShadowUBO
{
    vec4 CascadeEnds;
    mat4 LightSpaceMatrix[MAX_CASCADE_COUNT];
    uint CascadeCount;
    int ShowCascades;
    float _pad0, pad1;
} uShadowParams;

void main()
{
    vOutput.TexCoord = aTexCoord;
    vOutput.WorldPos = (uMesh.Transform * vec4(aPosition, 1.0)).xyz;
    vOutput.Tangent = mat3(uMesh.Transform) * normalize(aTangent);
    vOutput.BiTangent = mat3(uMesh.Transform) * normalize(aBiTangent);
    vOutput.Normal = mat3(uMesh.Transform) * normalize(aNormal);

    vOutput.ViewSpacePos = vec3(uFrame.View * vec4(vOutput.WorldPos, 1.0));

    for(uint i = 0; i < MAX_CASCADE_COUNT; i++)
        vOutput.LightSpaceVectors[i] = uShadowParams.LightSpaceMatrix[i] * vec4(vOutput.WorldPos, 1.0);

    gl_Position = uFrame.ViewProjection * uMesh.Transform * vec4(aPosition, 1.0);
}
