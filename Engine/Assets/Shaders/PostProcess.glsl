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

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D sceneColor;
layout(binding = 1, set = 0) uniform sampler2D outlineMask;

layout(push_constant) uniform PushConstants
{
    vec4 ColorThickness;   // rgb = color, a = thickness in pixels
    vec2 ScreenResolution;
    vec2 CameraNearFar;
} pc;

void main()
{
    vec4  scene = texture(sceneColor,  inUV);
    float maskC = texture(outlineMask, inUV).r;
    vec2  texel = 1.0 / pc.ScreenResolution;

    vec2 offset = texel * max(pc.ColorThickness.a, 1.0);

    float checkN = texture(outlineMask, inUV + vec2( 0.0,      offset.y)).r;
    float checkS = texture(outlineMask, inUV + vec2( 0.0,     -offset.y)).r;
    float checkE = texture(outlineMask, inUV + vec2( offset.x,  0.0     )).r;
    float checkW = texture(outlineMask, inUV + vec2(-offset.x,  0.0     )).r;
    bool solidZone = abs(maskC - checkN) < 0.01 &&  abs(maskC - checkS) < 0.01 &&  abs(maskC - checkE) < 0.01 &&  abs(maskC - checkW) < 0.01;
    if (solidZone)
    {
        //outColor = vec4(1, 0, 0, 1);
        outColor = scene;
        return; //Early exit
    }

    // Heavy pass 8-tap wide pattern
    // [maskNW]   [maskN]   [maskNE]
    //         \     |     /
    // [maskW] ---[maskC]--- [maskE]
    //         /     |     \
    // [maskSW]   [maskS]   [maskSE]

    float maskN  = checkN;
    float maskS  = checkS;
    float maskE  = checkE;
    float maskW  = checkW;

    float maskNW = texture(outlineMask, inUV + vec2(-offset.x,  offset.y)).r;
    float maskNE = texture(outlineMask, inUV + vec2( offset.x,  offset.y)).r;
    float maskSW = texture(outlineMask, inUV + vec2(-offset.x, -offset.y)).r;
    float maskSE = texture(outlineMask, inUV + vec2( offset.x, -offset.y)).r;

    float maxDiff = max(
        max(max(abs(maskC - maskN),  abs(maskC - maskS)),
            max(abs(maskC - maskE),  abs(maskC - maskW))),
        max(max(abs(maskC - maskNW), abs(maskC - maskNE)),
            max(abs(maskC - maskSW), abs(maskC - maskSE)))
    );

    float edge = 0.0;
    if (abs(maskC - maskN)  > 0.01 || abs(maskC - maskS)  > 0.01 ||
        abs(maskC - maskE)  > 0.01 || abs(maskC - maskW)  > 0.01 ||
        abs(maskC - maskNW) > 0.01 || abs(maskC - maskNE) > 0.01 ||
        abs(maskC - maskSW) > 0.01 || abs(maskC - maskSE) > 0.01)
    {
        edge = 1.0;
    }

    outColor = mix(scene, vec4(pc.ColorThickness.rgb, 1.0), edge);
}