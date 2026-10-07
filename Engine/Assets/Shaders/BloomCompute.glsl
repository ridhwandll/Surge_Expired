//SURGE:[Shader: Compute]
#version 450
#extension GL_GOOGLE_include_directive : require

// Compute path of the bloom mip chain (desktop), one dispatch per mip
// See Bloom.glsl for the fragment path, both share Include/BloomCommon.glsli

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, set = 0, rgba16f) uniform writeonly image2D oTarget;
layout(binding = 1, set = 0) uniform sampler2D uSource;
layout(binding = 2, set = 0) uniform sampler2D uHighRes;

#include "Include/BloomCommon.glsli"

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, imageSize(oTarget))))
        return;

    vec2 uv = (vec2(pixel) + 0.5) * uBloom.OutputTexelSize;
    imageStore(oTarget, pixel, vec4(BloomFilter(uSource, uHighRes, uv), 1.0));
}
