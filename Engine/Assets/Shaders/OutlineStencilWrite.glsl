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
} uPC;

void main()
{
    // Project the geometry precisely to match your normal rendering layout
    gl_Position = uPC.ModelViewProjection * vec4(aPosition, 1.0);
}

//SURGE:[Shader: Fragment]
#version 450

void main()
{
    // Intentionally left blank
    // The GPU executes the Stencil State operations (REPLACEs with 1) automatically at the end of the rasterization stage
}