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

// TODO Textures
//layout(set = 1, binding = 0) uniform sampler2D uTextures[16];

layout(location = 0) flat in uint inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in uint inTextureIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = unpackUnorm4x8(inColor);
    //outColor = texture(uTextures[inTextureIndex], inUV) * color;

    // Convert incoming sRGB vertex data to Linear Space, then gamma corrected later in PostProcess pass
    // Else there will be double gamma correction
    color.rgb = pow(color.rgb, vec3(2.2)); 

    outColor = color;
}