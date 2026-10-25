//SURGE:[Shader: Fragment]
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Include/Renderer3DFragment.glsli"

//SURGE:[Shader: Vertex]
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Include/Renderer3DVertex.glsli"

// Per-instance transforms written by GeometryPass in draw order, gl_InstanceIndex includes firstInstance in Vulkan
layout(std430, set = 0, binding = 2) readonly buffer InstanceData
{
    mat4 Transforms[];
} uInstances;

void main()
{
    WriteVertexOutput(uInstances.Transforms[gl_InstanceIndex]);
}
