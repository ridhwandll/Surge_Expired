//SURGE:[Shader: Vertex]
#version 450
#extension GL_KHR_vulkan_glsl: enable

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec2 outUV_N;
layout(location = 2) out vec2 outUV_S;
layout(location = 3) out vec2 outUV_E;
layout(location = 4) out vec2 outUV_W;

layout(push_constant) uniform PushConstants
{
    vec4 ColorThickness;
    vec2 ScreenResolution;
    float VignetteIntensity;
    float VignetteSoftness;
    float Grain;
    int EnableFXAA;
    vec2 CameraNearFar;
    vec4 DepthParams; // x: P[2][2], y: P[3][2], z: 1 = perspective / 0 = orthographic
    vec2 AOResolution;
    int AOMode;       // 0 = off, 1 = on, 2 = AO only (debug)
    float _pad0;
    vec4 BloomTint;   // rgb: tint * intensity
    vec2 BloomTexelSize;
    int BloomMode;    // 0 = off, 1 = bilinear, 2 = 9 tap tent
    float _pad1;
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
layout(binding = 3, set = 0) uniform sampler2D aoTexture;     // R8, AO resolution
layout(binding = 4, set = 0) uniform sampler2D aoLinearDepth; // R32F, AO resolution
layout(binding = 5, set = 0) uniform sampler2D bloomTexture;  // Half res, top of the bloom upsample chain

layout(push_constant) uniform PushConstants
{
    vec4 ColorThickness;
    vec2 ScreenResolution;
    float VignetteIntensity;
    float VignetteSoftness;
    float Grain;
    int EnableFXAA;
    vec2 CameraNearFar;
    vec4 DepthParams; // x: P[2][2], y: P[3][2], z: 1 = perspective / 0 = orthographic
    vec2 AOResolution;
    int AOMode;       // 0 = off, 1 = on, 2 = AO only (debug)
    float _pad0;
    vec4 BloomTint;   // rgb: tint * intensity
    vec2 BloomTexelSize;
    int BloomMode;    // 0 = off, 1 = bilinear, 2 = 9 tap tent
    float _pad1;
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
    float innerBoundary = radius - (pc.VignetteSoftness * 0.4);
    float outerBoundary = radius + (pc.VignetteSoftness * 0.4);

    float vignetteResponse = smoothstep(innerBoundary, outerBoundary, dist);
    vignetteResponse = 1.0 - vignetteResponse;
    return mix(1.0, vignetteResponse, pc.VignetteIntensity);
}

// Film grain
float FilmGrain()
{
    float noise = fract(sin(dot(inUV, vec2(12.9898, 78.233))) * 43758.5453);
    return (noise - 0.5) * pc.Grain;
}

float LinearizeDepth(float d)
{
    if (pc.DepthParams.z > 0.5)
        return pc.DepthParams.y / (d + pc.DepthParams.x);

    return (pc.DepthParams.y - d) / pc.DepthParams.x;
}

// Joint bilateral upsample of the (possibly half res) AO buffer: 2 gathers, weights the 4 nearest AO texels
// by bilinear position and by how close their depth is to this pixel's depth, so AO does not leak across silhouettes
float SampleAmbientOcclusion()
{
    float rawDepth = texture(sceneDepth, inUV).r;
    if (rawDepth >= 1.0)
        return 1.0; // Sky

    float depth = LinearizeDepth(rawDepth);
    vec4 aoQuad = textureGather(aoTexture, inUV, 0);
    vec4 depthQuad = textureGather(aoLinearDepth, inUV, 0);

    // Gather order: (-,+) (+,+) (+,-) (-,-)
    vec2 f = fract(inUV * pc.AOResolution - 0.5);
    vec4 bilinear = vec4((1.0 - f.x) * f.y, f.x * f.y, f.x * (1.0 - f.y), (1.0 - f.x) * (1.0 - f.y));

    vec4 relativeDelta = abs(depthQuad - depth) / max(depth, 1e-3);
    vec4 weights = bilinear * exp2(-relativeDelta * relativeDelta * 1024.0);
    float totalWeight = dot(weights, vec4(1.0));

    if (totalWeight < 1e-4)
    {
        // No texel on this surface (thin feature at a depth edge), take the closest one in depth
        float best = relativeDelta.x;
        float ao = aoQuad.x;
        if (relativeDelta.y < best) { best = relativeDelta.y; ao = aoQuad.y; }
        if (relativeDelta.z < best) { best = relativeDelta.z; ao = aoQuad.z; }
        if (relativeDelta.w < best) { ao = aoQuad.w; }
        return ao;
    }

    return dot(aoQuad, weights) / totalWeight;
}

vec3 SampleBloom()
{
    if (pc.BloomMode == 1)
        return textureLod(bloomTexture, inUV, 0.0).rgb;

    // 9 tap tent while upscaling half res -> full res, hides the bilinear diamond pattern on strong highlights
    vec2 t = pc.BloomTexelSize;
    vec3 sum = textureLod(bloomTexture, inUV, 0.0).rgb * 4.0;
    sum += (textureLod(bloomTexture, inUV + vec2( 0.0, -t.y), 0.0).rgb +
            textureLod(bloomTexture, inUV + vec2(-t.x,  0.0), 0.0).rgb +
            textureLod(bloomTexture, inUV + vec2( t.x,  0.0), 0.0).rgb +
            textureLod(bloomTexture, inUV + vec2( 0.0,  t.y), 0.0).rgb) * 2.0;
    sum += textureLod(bloomTexture, inUV + vec2(-t.x, -t.y), 0.0).rgb +
           textureLod(bloomTexture, inUV + vec2( t.x, -t.y), 0.0).rgb +
           textureLod(bloomTexture, inUV + vec2(-t.x,  t.y), 0.0).rgb +
           textureLod(bloomTexture, inUV + vec2( t.x,  t.y), 0.0).rgb;
    return sum * (1.0 / 16.0);
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

    if (pc.AOMode != 0)
    {
        float ao = SampleAmbientOcclusion();
        if (pc.AOMode == 2)
        {
            outColor = vec4(vec3(ao), 1.0);
            return;
        }
        HDRColor *= ao; // Applied in linear HDR, before tonemapping
    }

    if (pc.BloomMode != 0)
        HDRColor += SampleBloom() * pc.BloomTint.rgb; // After AO, emitted glow should not be occluded

    vec3 LDRColor = ACESFilmic(HDRColor);

    LDRColor *= ComputeVignette();
    LDRColor += FilmGrain();
    LDRColor = clamp(LDRColor, 0.0, 1.0); // (Rid) Stops negative grain values from breaking the pow() function below

    LDRColor = pow(LDRColor, vec3(1.0 / 2.2)); //Gamma

    vec3 finalColor = mix(LDRColor, pc.ColorThickness.rgb, CalculateOutline()); // Outlines

    outColor = vec4(finalColor, 1.0);
}