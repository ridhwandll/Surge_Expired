[Shader: Vertex]
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform PushConstants
{
    mat4 ViewProj;
} uPush;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

void main()
{
    gl_Position = uPush.ViewProj * vec4(inPosition, 1.0);
    outColor = inColor;
    outUV = inUV;
}

[Shader: Fragment]
#version 450

layout(location = 0) in  vec4 inColor;
layout(location = 1) in  vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = inColor; // TODO: Texture sampling
}