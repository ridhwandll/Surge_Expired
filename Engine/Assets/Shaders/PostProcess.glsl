//SURGE:[Shader: Vertex]
#version 450
#extension GL_KHR_vulkan_glsl: enable

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec2 outUV_N;
layout(location = 2) out vec2 outUV_S;
layout(location = 3) out vec2 outUV_E;
layout(location = 4) out vec2 outUV_W;

struct VignetteGrainConfig
{
    float Intensity;
    float Softness;
    float Grain;
    float _Pad;
};
layout(push_constant) uniform PushConstants
{
    vec4 ColorThickness;
    vec2 ScreenResolution;
    VignetteGrainConfig VignetteGrain;
    vec2 CameraNearFar;
    int EnableFXAA;
} pc;

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);

    vec2 texel = 1.0 / pc.ScreenResolution;
    vec2 offset = texel * max(pc.ColorThickness.a, 1.0);

    outUV_N = outUV + vec2(0.0,  offset.y);
    outUV_S = outUV + vec2(0.0, -offset.y);
    outUV_E = outUV + vec2( offset.x, 0.0);
    outUV_W = outUV + vec2(-offset.x, 0.0);
}

//SURGE:[Shader: Fragment]
#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec2 inUV_N;
layout(location = 2) in vec2 inUV_S;
layout(location = 3) in vec2 inUV_E;
layout(location = 4) in vec2 inUV_W;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D sceneColor;
layout(binding = 1, set = 0) uniform sampler2D outlineMask;
layout(binding = 2, set = 0) uniform sampler2D sceneDepth;

struct VignetteGrainConfig
{
    float Intensity;
    float Softness;
    float Grain;
    float _Pad;
};
layout(push_constant) uniform PushConstants
{
    vec4 ColorThickness;
    vec2 ScreenResolution;
    VignetteGrainConfig VignetteGrain;
    vec2 CameraNearFar;
    int EnableFXAA;
} pc;

// FXAA
vec4 CalculateFXAA(sampler2D tex, vec2 uv, vec2 texel)
{
    const float FXAA_REDUC_MIN = 1.0 / 128.0;
    const float FXAA_REDUC_MUL = 1.0 / 8.0;
    const float FXAA_SPAN_MAX  = 8.0;

    vec3 rgbNW = texture(tex, uv + vec2(-texel.x, -texel.y)).rgb;
    vec3 rgbNE = texture(tex, uv + vec2( texel.x, -texel.y)).rgb;
    vec3 rgbSW = texture(tex, uv + vec2(-texel.x,  texel.y)).rgb;
    vec3 rgbSE = texture(tex, uv + vec2( texel.x,  texel.y)).rgb;
    vec3 rgbM  = texture(tex, uv).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUC_MUL), FXAA_REDUC_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)) * texel;

    vec3 rgbA = 0.5 * (texture(tex, uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture(tex, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(tex, uv + dir * (0.0 / 3.0 - 0.5)).rgb + texture(tex, uv + dir * (3.0 / 3.0 - 0.5)).rgb);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        return vec4(rgbA, 1.0);

    return vec4(rgbB, 1.0);
}

float CalculateOutline()
{
    float maskC = texture(outlineMask, inUV).r;
    float maskN = texture(outlineMask, inUV_N).r;
    float maskS = texture(outlineMask, inUV_S).r;
    float maskE = texture(outlineMask, inUV_E).r;
    float maskW = texture(outlineMask, inUV_W).r;

    float edge = 0.0;
    if (abs(maskC - maskN) > 0.01 || abs(maskC - maskS) > 0.01 || abs(maskC - maskE) > 0.01 || abs(maskC - maskW) > 0.01)
        edge = 1.0;

    return edge;
}

// Vignette
float ComputeVignette()
{
    // Calculate distance from the center of the screen (0.0 at center, 1.0 at corners)
    vec2 d = inUV - vec2(0.5);
    float dist = length(d); // Max distance at extreme corners is ~0.707

    float radius = 0.5;
    float innerBoundary = radius - (pc.VignetteGrain.Softness * 0.4);
    float outerBoundary = radius + (pc.VignetteGrain.Softness * 0.4);

    float vignetteResponse = smoothstep(innerBoundary, outerBoundary, dist);
    vignetteResponse = 1.0 - vignetteResponse;
    return mix(1.0, vignetteResponse, pc.VignetteGrain.Intensity);
}

// Film grain
float FilmGrain()
{
    float noise = fract(sin(dot(inUV, vec2(12.9898, 78.233))) * 43758.5453);
    return (noise - 0.5) * pc.VignetteGrain.Grain;
}

vec3 ACESFilmic(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// TODO: Depth-Driven Atmospheric Fog
// TODO: Screen-Space God Rays
// TODO: Chromatic Aberration
// TODO: Expose Tonemapping controls (give options: Reinhard/ACES etc.)

void main()
{
    vec2 texel = 1.0 / pc.ScreenResolution;
    vec3 HDRColor;
    if (pc.EnableFXAA == 1)
        HDRColor = CalculateFXAA(sceneColor, inUV, texel).rgb;
    else
        HDRColor = texture(sceneColor, inUV).rgb;

    vec3 LDRColor = ACESFilmic(HDRColor);

    LDRColor *= ComputeVignette();
    LDRColor += FilmGrain();
    LDRColor = clamp(LDRColor, 0.0, 1.0); // (Rid) Stops negative grain values from breaking the pow() function below

    LDRColor = pow(LDRColor, vec3(1.0 / 2.2)); //Gamma

    vec3 finalColor = mix(LDRColor, pc.ColorThickness.rgb, CalculateOutline()); // Outlines

    outColor = vec4(finalColor, 1.0);
}