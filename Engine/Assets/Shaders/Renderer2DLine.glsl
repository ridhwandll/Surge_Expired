//SURGE:[Shader: Vertex]
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 View;
    mat4 ViewProjection;
    vec3 CameraPos;
    float _pad;

} uFrame;

layout(location = 0) flat out vec4 outColor;

void main()
{
    gl_Position = uFrame.ViewProjection * vec4(inPosition, 1.0);
    outColor = inColor;
}

//SURGE:[Shader: Fragment]
#version 450

layout(location = 0) flat in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = inColor;
    color.rgb = pow(color.rgb, vec3(2.2));
    outColor = color;
}