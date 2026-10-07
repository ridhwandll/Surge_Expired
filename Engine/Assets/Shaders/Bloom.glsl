//SURGE:[Shader: Vertex]
#version 450

layout(location = 0) out vec2 outUV;

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}

//SURGE:[Shader: Fragment]
#version 450
#extension GL_GOOGLE_include_directive : require

// Fragment path of the bloom mip chain (mobile), one fullscreen triangle per mip
// See BloomCompute.glsl for the compute path, both share Include/BloomCommon.glsli

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D uSource;
layout(binding = 1, set = 0) uniform sampler2D uHighRes;

#include "Include/BloomCommon.glsli"

void main()
{
    outColor = vec4(BloomFilter(uSource, uHighRes, inUV), 1.0);
}
