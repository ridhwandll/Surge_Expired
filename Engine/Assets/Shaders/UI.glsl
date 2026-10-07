//SURGE:[Shader: Vertex]
#version 450

// Screen space UI: sprites and MSDF text share one shader/pipeline so they batch together and keep their submission (hierarchy) order

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inTextureIndex; // bits 0-7: texture slot, bits 8-23: MSDF px range * 256, bit 31: MSDF text glyph

layout(push_constant) uniform PushConstants
{
    mat4 ViewProjection; // Orthographic, (0,0) = top left
} uFrame;

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

layout(set = 0, binding = 0) uniform sampler2D uTextures[16];

layout(location = 0) flat in uint inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in uint inTextureIndex;

layout(location = 0) out vec4 outColor;

const uint TEXT_FLAG = 0x80000000u;

float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    // UI is drawn after post processing, colors are already display (gamma) space, no linearization here
    vec4 color = unpackUnorm4x8(inColor);
    uint slot = inTextureIndex & 0xFFu;

    // Derivatives before branching (the flag is flat per primitive, but keep it obviously safe)
    vec2 uvDx = dFdx(inUV);
    vec2 uvDy = dFdy(inUV);

    if((inTextureIndex & TEXT_FLAG) == 0u)
    {
        outColor = texture(uTextures[slot], inUV) * color;
        return;
    }

    // MSDF text
    float pxRange = float((inTextureIndex >> 8) & 0xFFFFu) / 256.0;
    vec2 unitRange = vec2(pxRange) / vec2(textureSize(uTextures[slot], 0));
    vec2 screenTexSize = inversesqrt(uvDx * uvDx + uvDy * uvDy);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    vec3 msd = texture(uTextures[slot], inUV).rgb;
    float sd = Median(msd.r, msd.g, msd.b);
    float opacity = clamp(screenPxRange * (sd - 0.5) + 0.5, 0.0, 1.0);
    if(opacity < 0.001)
        discard;

    outColor = vec4(color.rgb, color.a * opacity); // Color alpha applies to text too (fading labels)
}
