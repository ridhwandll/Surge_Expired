//SURGE:[Shader: Vertex]
#version 450
#extension GL_KHR_vulkan_glsl: enable

layout(location = 0) out vec2 outUV;

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}

//SURGE:[Shader: Fragment]
#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D finalImage;

void main()
{
    outColor = texture(finalImage, inUV);
}
