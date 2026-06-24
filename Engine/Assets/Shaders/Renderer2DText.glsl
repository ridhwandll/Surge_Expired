//SURGE:[Shader: Vertex]
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inTextureIndex;

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 View;
    mat4 ViewProjection;
    vec3 CameraPos;
    float _pad;

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

layout(location = 0) flat in uint inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in uint inTextureIndex;

layout(set = 1, binding = 0) uniform sampler2D uTextures[16];

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform TextParams
{
    float PxRange;
    float pad[2];
} u_Params;

float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

vec2 Sqr(vec2 x) { return x * x; }

float ScreenPxRange()
{
    vec2 unitRange = vec2(u_Params.PxRange) / vec2(textureSize(uTextures[inTextureIndex], 0));
    vec2 screenTexSize = inversesqrt(Sqr(dFdx(inUV)) + Sqr(dFdy(inUV)));
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main()
{
    vec4 fillColor = unpackUnorm4x8(inColor);
    fillColor.rgb = pow(fillColor.rgb, vec3(2.2));
    
    vec3 msd = texture(uTextures[inTextureIndex], inUV).rgb;
    float sd = Median(msd.r, msd.g, msd.b);
    
    float screenPxDistance = ScreenPxRange() * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    if (opacity < 0.01)
        discard;

    fillColor.a *= opacity;
    o_Color = fillColor;
}