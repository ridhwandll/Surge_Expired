//SURGE:[Shader: Fragment]
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Include/Renderer3DFragment.glsli"

//SURGE:[Shader: Vertex]
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Include/Renderer3DVertex.glsli"

#define SKIN_SET 0
#define SKIN_BINDING 3
#include "Include/Skinning.glsli"

// Written by GeometryPass in draw order (mirrors GeometryPass::SkinnedInstance), gl_InstanceIndex includes firstInstance in Vulkan
struct SkinnedInstance
{
    mat4 Transform;
    uint SkinOffset;
    uint _pad0, _pad1, _pad2;
};

layout(std430, set = 0, binding = 2) readonly buffer InstanceData
{
    SkinnedInstance Instances[];
} uInstances;

void main()
{
    SkinnedInstance instance = uInstances.Instances[gl_InstanceIndex];
    WriteVertexOutput(instance.Transform * GetSkinMatrix(instance.SkinOffset));
}
