//SURGE:[Shader: Vertex]
#version 450
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;

#define SKIN_SET 0
#define SKIN_BINDING 1
#include "Include/Skinning.glsli"

layout(push_constant) uniform ShadowPushConstant
{
    mat4 LightSpaceTransform; // View Projection matrix of the cascade being drawn (its own viewport in the atlas)
} pc;

// Written by ShadowPass (all cascades back to back), mirrors ShadowPass::SkinnedInstance
struct SkinnedInstance
{
    mat4 Transform;
    uint SkinOffset;
    uint _pad0, _pad1, _pad2;
};

layout(std430, set = 0, binding = 0) readonly buffer InstanceData
{
    SkinnedInstance Instances[];
} uInstances;

void main()
{
    SkinnedInstance instance = uInstances.Instances[gl_InstanceIndex];
    gl_Position = pc.LightSpaceTransform * instance.Transform * (GetSkinMatrix(instance.SkinOffset) * vec4(aPosition, 1.0));

    // Pancaking, like Shadow.glsl: casters toward the light are flattened onto the near plane instead of being clipped
    gl_Position.z = max(gl_Position.z, 0.0);
}

//SURGE:[Shader: Fragment]
#version 450

void main()
{
    // Intentionally left empty
}
