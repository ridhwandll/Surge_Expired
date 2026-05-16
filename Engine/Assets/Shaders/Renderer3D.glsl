//SURGE:[Shader: Vertex]
#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 ViewProjection;
    vec3 CameraPos;
    float _pad;

} uFrame;

layout(push_constant) uniform PushConstants
{
    mat4 Transform;
    uint LightBufferIndex;
    uint LightCount;
    uint MaterialBufferIndex;
    uint MaterialIndex;
} uMesh;

struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
    vec3 WorldPos;
};
layout(location = 0) out VertexOutput vOutput;

void main()
{
    vOutput.WorldPos = (uMesh.Transform * vec4(aPosition, 1.0)).xyz;
    vOutput.TexCoord = aTexCoord;
    vOutput.Tangent = mat3(uMesh.Transform) * normalize(aTangent);
    vOutput.BiTangent = mat3(uMesh.Transform) * normalize(aBiTangent);
    vOutput.Normal = mat3(uMesh.Transform) * normalize(aNormal);
    gl_Position = uFrame.ViewProjection * uMesh.Transform * vec4(aPosition, 1.0);
}

//SURGE:[Shader: Fragment]
#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
    vec3 WorldPos;
};
layout(location = 0) in VertexOutput vInput;
layout(location = 0) out vec4 FinalColor;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 ViewProjection;
    vec3 CameraPos;
    float _pad;

} uFrame;

layout(push_constant) uniform PushConstants
{
    mat4 Transform;
    uint LightBufferIndex; // bindless index into uBuffers[]
    uint LightCount;
    uint MaterialBufferIndex;
    uint MaterialIndex;
} uMesh;

struct Material
{
    vec4 AlbedoMetallic;    // xyz=albedo, w=metallic
    float Roughness;
    float Reflectance;
    uint AlbedoTexIndex;
    uint NormalTexIndex;
};

struct Light
{
    vec4 PositionType;   // xyz = pos/dir, w = type (0=dir, 1=point)
    vec4 Color;          // rgb = color, a = intensity
    float Radius;        // point light falloff radius
    float Falloff;
    float _pad1, _pad2;
};
//Set: 1 Lights
layout(set = 1, binding = 0) readonly buffer LightBuffer
{
    Light lights[];

} slights[];

layout(set = 1, binding = 0) readonly buffer MaterialBuffer
{
    Material materials[];

} sMaterialBuffers[];

//Bindless texture array: 
layout(set = 1, binding = 1) uniform sampler2D uTexture[4096];


//layout(set = 1, binding = 1) uniform SceneUBO
//{
//    vec3  ambientColor;
//    float ambientStrength;
//    uint  lightCount;
//} scene;


// Energy conserving Blinn-Phong?
vec3 CalculateMobilePBR(Light light, Material mat, vec3 N, vec3 V, vec3 fragPos)
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
    float alpha = max(mat.Roughness, 0.04); 
    float shininess = 2.0 / (pow(alpha, 4.0)) - 2.0;

    // Energy Conservation: Metals have no Diffuse
    // f0 represents the base reflectivity (at 0 degrees)
    // Non-metals (dielectrics) use a constant (usually 0.04), metals use Albedo
    vec3 f0 = vec3(0.04) * mat.Reflectance;
    vec3 specColor = mix(f0, mat.AlbedoMetallic.rgb, mat.AlbedoMetallic.a);
    vec3 diffuseColor = mat.AlbedoMetallic.rgb * (1.0 - mat.AlbedoMetallic.a);

    // Normalized Blinn-Phong Specular
    // The (shininess + 8)/8 factor ensures the light energy stays consistent
    // as the highlight gets tighter.
    float specNormalization = (shininess + 8.0) / (8.0 * 3.14159);
    float specularTerm = pow(dotNH, shininess) * specNormalization;
    
    // Final Composition
    vec3 diffuse = (diffuseColor / 3.14159) * dotNL;
    vec3 specular = specColor * specularTerm * dotNL;

    vec3 lightIntensity = light.Color.rgb * light.Color.a;
    return (diffuse + specular) * lightIntensity * attenuation;
}

void main()
{
    Material mat = sMaterialBuffers[uMesh.MaterialBufferIndex].materials[uMesh.MaterialIndex];

    vec3 N = normalize(vInput.Normal);
    vec3 V = normalize(uFrame.CameraPos - vInput.WorldPos);

    // Environment Reflection
    vec3 skyColor = vec3(0.3, 0.3, 0.3);  // Dull Blue Sky
    vec3 groundColor = vec3(0.1, 0.1, 0.1); // Dark Ground    
    float skyWeight = N.y * 0.5 + 0.5; 
    vec3 fakeEnvironment = mix(groundColor, skyColor, skyWeight);

    // Metals reflect the environment tinted by their Albedo
    vec3 ambientReflection = fakeEnvironment * mix(vec3(0.04), mat.AlbedoMetallic.rgb, mat.AlbedoMetallic.a);

    // Direct Lighting Accumulation
    vec3 directAccumulation = vec3(0.0);
    for (uint i = 0; i < uMesh.LightCount; i++)       
        directAccumulation += CalculateMobilePBR(slights[uMesh.LightBufferIndex].lights[i], mat, N, V, vInput.WorldPos);        

    vec3 color = ambientReflection + directAccumulation;
    color = color / (color + vec3(1.0));

    // Linear to sRGB conversion (Gamma Correction)
    color = pow(color, vec3(1.0 / 2.2));

    FinalColor = vec4(color, 1.0);
}
