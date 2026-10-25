//SURGE:[Shader: Vertex]
#version 450
#extension GL_KHR_vulkan_glsl: enable

// (Rid) Adreno 610 (2020 driver) fails vkCreateGraphicsPipelines (KGSL GPUOBJ_ALLOC EINVAL) when the fragment shader's only varying
// read is the texture coordinate. Two varyings at +/- a texel, averaged in the fragment shader (== the UV, the compiler can't know it)
layout(location = 0) out vec2 outUV_A;
layout(location = 1) out vec2 outUV_B;

layout(push_constant) uniform PushConstants
{
    vec4 DepthParams;      // x: P[2][2], y: P[3][2], z: 1 = perspective / 0 = orthographic
    vec4 BloomTint;        // rgb: tint * intensity
    vec2 ScreenResolution; // Scene image size (render scale applied)
    vec2 CameraNearFar;
    vec2 AOResolution;
    vec2 BloomTexelSize;
    float VignetteIntensity;
    float VignetteSoftness;
    float Grain;
    int AOMode;            // 0 = off, 1 = on, 2 = AO only (debug)
    int BloomMode;         // 0 = off, 1 = bilinear, 2 = 9 tap tent
    int SurfaceRotation;   // Quarter turns the output is pre-rotated by (Android swapchain pre-rotation)
} pc;

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

    // Pre-rotation: the swapchain image is in the display's native orientation, the UVs keep sampling the scene upright
    vec2 position = uv * 2.0 - 1.0;
    if (pc.SurfaceRotation == 1)
        position = vec2(-position.y, position.x);
    else if (pc.SurfaceRotation == 2)
        position = -position;
    else if (pc.SurfaceRotation == 3)
        position = vec2(position.y, -position.x);
    gl_Position = vec4(position, 0.0, 1.0);

    vec2 offset = vec2(0.0, 1.0 / pc.ScreenResolution.y);
    outUV_A = uv + offset;
    outUV_B = uv - offset;
}

//SURGE:[Shader: Fragment]
#version 450

layout(location = 0) in vec2 inUV_A;
layout(location = 1) in vec2 inUV_B;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D sceneColor;
layout(binding = 1, set = 0) uniform sampler2D sceneDepth;
layout(binding = 2, set = 0) uniform sampler2D aoTexture;     // R8, AO resolution
layout(binding = 3, set = 0) uniform sampler2D aoLinearDepth; // R32F, AO resolution
layout(binding = 4, set = 0) uniform sampler2D bloomTexture;  // Half res, top of the bloom upsample chain

layout(push_constant) uniform PushConstants
{
    vec4 DepthParams;      // x: P[2][2], y: P[3][2], z: 1 = perspective / 0 = orthographic
    vec4 BloomTint;        // rgb: tint * intensity
    vec2 ScreenResolution; // Scene image size (render scale applied)
    vec2 CameraNearFar;
    vec2 AOResolution;
    vec2 BloomTexelSize;
    float VignetteIntensity;
    float VignetteSoftness;
    float Grain;
    int AOMode;            // 0 = off, 1 = on, 2 = AO only (debug)
    int BloomMode;         // 0 = off, 1 = bilinear, 2 = 9 tap tent
    int SurfaceRotation;   // Quarter turns the output is pre-rotated by (Android swapchain pre-rotation)
} pc;

vec2 gUV; // (inUV_A + inUV_B) / 2, set first thing in main(): every read goes through it, never through a single varying

// Vignette
float ComputeVignette()
{
    // Calculate distance from the center of the screen (0.0 at center, 1.0 at corners)
    vec2 d = gUV - vec2(0.5);
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
    float noise = fract(sin(dot(gUV, vec2(12.9898, 78.233))) * 43758.5453);
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
    float rawDepth = texture(sceneDepth, gUV).r;
    if (rawDepth >= 1.0)
        return 1.0; // Sky

    float depth = LinearizeDepth(rawDepth);
    vec4 aoQuad = textureGather(aoTexture, gUV, 0);
    vec4 depthQuad = textureGather(aoLinearDepth, gUV, 0);

    // Gather order: (-,+) (+,+) (+,-) (-,-)
    vec2 f = fract(gUV * pc.AOResolution - 0.5);
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
        return textureLod(bloomTexture, gUV, 0.0).rgb;

    // 9 tap tent while upscaling half res -> full res, hides the bilinear diamond pattern on strong highlights
    vec2 t = pc.BloomTexelSize;
    vec3 sum = textureLod(bloomTexture, gUV, 0.0).rgb * 4.0;
    sum += (textureLod(bloomTexture, gUV + vec2( 0.0, -t.y), 0.0).rgb +
            textureLod(bloomTexture, gUV + vec2(-t.x,  0.0), 0.0).rgb +
            textureLod(bloomTexture, gUV + vec2( t.x,  0.0), 0.0).rgb +
            textureLod(bloomTexture, gUV + vec2( 0.0,  t.y), 0.0).rgb) * 2.0;
    sum += textureLod(bloomTexture, gUV + vec2(-t.x, -t.y), 0.0).rgb +
           textureLod(bloomTexture, gUV + vec2( t.x, -t.y), 0.0).rgb +
           textureLod(bloomTexture, gUV + vec2(-t.x,  t.y), 0.0).rgb +
           textureLod(bloomTexture, gUV + vec2( t.x,  t.y), 0.0).rgb;
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
    gUV = (inUV_A + inUV_B) * 0.5;

    // (Rid) Runs on every output pixel (full resolution, not render scale): the effects that are off are skipped (uniform branches)
    vec3 HDRColor = texture(sceneColor, gUV).rgb;

    if (pc.AOMode != 0)
    {
        float ao = SampleAmbientOcclusion();
        if (pc.AOMode == 2) // AO Only (visualization)
        {
            outColor = vec4(vec3(ao), 1.0);
            return;
        }
        HDRColor *= ao; // Applied in linear HDR, before tonemapping
    }

    if (pc.BloomMode != 0)
        HDRColor += SampleBloom() * pc.BloomTint.rgb; // After AO, emitted glow should not be occluded

    vec3 LDRColor = ACESFilmic(HDRColor);

    if (pc.VignetteIntensity > 0.0)
        LDRColor *= ComputeVignette();
    if (pc.Grain > 0.0)
        LDRColor += FilmGrain();
    LDRColor = clamp(LDRColor, 0.0, 1.0); // (Rid) Stops negative grain values from breaking the pow() function below

    LDRColor = pow(LDRColor, vec3(1.0 / 2.2)); // Gamma

    outColor = vec4(LDRColor, 1.0);
}
