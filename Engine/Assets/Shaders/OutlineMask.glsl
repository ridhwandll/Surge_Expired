//SURGE:[Shader: Vertex]
#version 450
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;

layout(push_constant) uniform OutineMaskPushConstants
{
    mat4 ModelViewProjection;
} pc;

void main()
{
    gl_Position = pc.ModelViewProjection * vec4(aPosition, 1.0);
}

//SURGE:[Shader: Fragment]
#version 450

layout(location = 0) out vec4 outColor;

void main()
{
    float channelValue = 1.0; // Outputs to a R8_UNORM image
    outColor = vec4(channelValue, 0.0, 0.0, 0.0);
}