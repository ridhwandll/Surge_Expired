//SURGE:[Shader: Fragment]
#version 450

struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
};
layout(location = 0) in VertexOutput vInput;
layout(location = 0) out vec4 FinalColor;

// Bindless texture array
layout(set = 0, binding = 0) uniform sampler2D uTexture[4096];

void main()
{
    FinalColor = vec4(1.0, 0.0, 1.0, 1.0);
}

//SURGE:[Shader: Vertex]
#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;

layout(push_constant) uniform PushConstants
{
    mat4 Transform;
    mat4 ViewProj;
} uMesh;

struct VertexOutput
{
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 BiTangent;
};
layout(location = 0) out VertexOutput vOutput;

void main()
{
    vOutput.TexCoord = aTexCoord;
    vOutput.Tangent = mat3(uMesh.Transform) * normalize(aTangent);
    vOutput.BiTangent = mat3(uMesh.Transform) * normalize(aBiTangent);
    vOutput.Normal = mat3(uMesh.Transform) * normalize(aNormal);
    gl_Position = uMesh.ViewProj * uMesh.Transform * vec4(aPosition, 1.0);
}
