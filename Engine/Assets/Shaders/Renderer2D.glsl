//SURGE:[Shader: Vertex]
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inTextureIndex;

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
} uPush;

layout(location = 0) flat out uint outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) flat out uint outTextureIndex;

void main()
{
    gl_Position = uFrame.ViewProjection * vec4(inPosition, 1.0);
    outColor = inColor;
    outUV = inUV;
    outTextureIndex = inTextureIndex;
}

//SURGE:[Shader: Fragment]
#version 450

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
} uMesh;

layout(set = 1, binding = 1) uniform sampler2D uTexture[4096];

layout(location = 0) flat in uint inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in uint inTextureIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = unpackUnorm4x8(inColor);
    outColor = texture(uTexture[inTextureIndex], inUV) * color;
}