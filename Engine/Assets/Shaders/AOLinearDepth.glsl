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

// Converts the hardware depth buffer into positive view space depth at AO resolution
// Every following AO pass reads this small R32F target instead of the full res D32 buffer (bandwidth win on mobile)

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outLinearDepth;

layout(binding = 0, set = 0) uniform sampler2D uSceneDepth;

layout(push_constant) uniform PushConstants
{
    vec4 DepthParams;   // x: P[2][2], y: P[3][2], z: 1 = perspective / 0 = orthographic
    vec2 FullOverAOSize; // Full res size / AO res size
} pc;

float LinearizeDepth(float d)
{
    if (pc.DepthParams.z > 0.5)
        return pc.DepthParams.y / (d + pc.DepthParams.x);

    return (pc.DepthParams.y - d) / pc.DepthParams.x;
}

void main()
{
    ivec2 fullSize = textureSize(uSceneDepth, 0);
    ivec2 coord = min(ivec2(gl_FragCoord.xy * pc.FullOverAOSize), fullSize - 1);
    outLinearDepth = LinearizeDepth(texelFetch(uSceneDepth, coord, 0).r);
}
