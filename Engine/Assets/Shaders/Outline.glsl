//SURGE:[Shader: Vertex]
#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;
layout(location = 5) in vec3 aSmoothNormal;

layout(push_constant) uniform PC
{
    mat4 ModelViewProjection;
    vec4 ColorThickness;

} uPC;

void main()
{
    vec3 extruded = aPosition + normalize(aSmoothNormal) * uPC.ColorThickness.a * 0.016;
    gl_Position  = uPC.ModelViewProjection * vec4(extruded, 1.0);
}

//SURGE:[Shader: Fragment]
#version 450

layout(push_constant) uniform PC
{
    mat4 ModelViewProjection;
    vec4 ColorThickness;

} uPC;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(uPC.ColorThickness.rgb, 1.0);
}