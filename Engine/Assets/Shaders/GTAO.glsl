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

// Ground Truth Ambient Occlusion (Jimenez et al. 2016), structured after Intel's XeGTAO
// View space convention used here: +X right, +Y up, +Z forward (positive linear depth)
// Noise is a 4x4 interleaved pattern which AOBlur.glsl removes, so no temporal accumulation is required

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outAO;

layout(binding = 0, set = 0) uniform sampler2D uLinearDepth;

layout(push_constant) uniform PushConstants
{
    vec4 ProjInfo;      // xy: uv -> view scale, zw: uv -> view offset (multiplied by depth when perspective)
    vec2 TexelSize;     // 1.0 / AO resolution
    float Radius;       // World units
    float FalloffRange; // Fraction of Radius
    float MaxDistance;
    float Intensity;
    int SliceCount;
    int StepsPerSlice;
    int Flags;          // FLAG_PERSPECTIVE | FLAG_ACCURATE_NORMALS
} pc;

const int FLAG_PERSPECTIVE = 1;
const int FLAG_ACCURATE_NORMALS = 2;

const float PI = 3.1415926535;
const float PI_HALF = 1.5707963267;
const float MAX_RADIUS_SCREEN_FRACTION = 0.2; // Caps the kernel when the camera is very close to geometry (texture cache thrashing on mobile)

const float BAYER_4X4[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

bool IsPerspective() { return (pc.Flags & FLAG_PERSPECTIVE) != 0; }

vec3 ViewPosition(vec2 uv, float depth)
{
    vec2 xy = uv * pc.ProjInfo.xy + pc.ProjInfo.zw;
    return vec3(IsPerspective() ? xy * depth : xy, depth);
}

vec3 FetchViewPosition(vec2 uv)
{
    return ViewPosition(uv, textureLod(uLinearDepth, uv, 0.0).r);
}

// Polynomial acos approximation, max error ~1e-4 rad
float FastACos(float x)
{
    float ax = abs(x);
    float res = (-0.156583 * ax + PI_HALF) * sqrt(1.0 - ax);
    return x >= 0.0 ? res : PI - res;
}

vec3 ReconstructNormalAccurate(vec2 uv, vec3 center)
{
    // Pick the neighbour with the smaller depth discontinuity on each axis to avoid smearing normals across edges
    vec3 left   = FetchViewPosition(uv - vec2(pc.TexelSize.x, 0.0));
    vec3 right  = FetchViewPosition(uv + vec2(pc.TexelSize.x, 0.0));
    vec3 top    = FetchViewPosition(uv - vec2(0.0, pc.TexelSize.y));
    vec3 bottom = FetchViewPosition(uv + vec2(0.0, pc.TexelSize.y));

    vec3 dx = abs(left.z - center.z) < abs(right.z - center.z) ? center - left : right - center;
    vec3 dy = abs(top.z - center.z) < abs(bottom.z - center.z) ? center - top : bottom - center;
    return normalize(cross(dx, dy));
}

void main()
{
    vec3 viewPos = FetchViewPosition(inUV);

    // Derivatives must be taken in uniform control flow, before any early out
    vec3 normal = normalize(cross(dFdx(viewPos), dFdy(viewPos)));

    if (viewPos.z >= pc.MaxDistance)
    {
        outAO = 1.0;
        return;
    }

    if ((pc.Flags & FLAG_ACCURATE_NORMALS) != 0)
        normal = ReconstructNormalAccurate(inUV, viewPos);

    vec3 viewVec = IsPerspective() ? normalize(-viewPos) : vec3(0.0, 0.0, -1.0);
    if (dot(normal, viewVec) < 0.0)
        normal = -normal; // Handedness of the cross product depends on the projection, always face the camera

    // World radius -> AO texels
    float viewspaceTexelSize = pc.TexelSize.y * abs(pc.ProjInfo.y) * (IsPerspective() ? viewPos.z : 1.0);
    float screenRadius = min(pc.Radius / viewspaceTexelSize, MAX_RADIUS_SCREEN_FRACTION / pc.TexelSize.y);
    if (screenRadius < 1.0)
    {
        outAO = 1.0;
        return;
    }

    float falloffRange = max(pc.FalloffRange * pc.Radius, 1e-4);
    float falloffFrom = pc.Radius - falloffRange;
    float falloffMul = -1.0 / falloffRange;
    float falloffAdd = falloffFrom / falloffRange + 1.0;

    ivec2 pixel = ivec2(gl_FragCoord.xy) & 3;
    float noiseSlice = BAYER_4X4[pixel.x + pixel.y * 4] / 16.0;
    float noiseSample = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))); // Interleaved gradient noise

    float minS = 1.3 / screenRadius; // Skip the centre texel, it would always self occlude
    float visibility = 0.0;

    for (int slice = 0; slice < pc.SliceCount; slice++)
    {
        float phi = (float(slice) + noiseSlice) * PI / float(pc.SliceCount);
        vec2 omega = vec2(cos(phi), sin(phi)); // Screen space direction (AO texels)

        // Same direction expressed in view space (exact for any aspect ratio / projection flip)
        vec3 directionVec = normalize(vec3(omega * pc.TexelSize * pc.ProjInfo.xy, 0.0));
        vec3 orthoDirectionVec = directionVec - dot(directionVec, viewVec) * viewVec;
        vec3 axisVec = normalize(cross(orthoDirectionVec, viewVec));
        vec3 projectedNormal = normal - axisVec * dot(normal, axisVec);

        float signNorm = sign(dot(orthoDirectionVec, projectedNormal));
        float projectedNormalLength = max(length(projectedNormal), 1e-4);
        float cosNorm = clamp(dot(projectedNormal, viewVec) / projectedNormalLength, 0.0, 1.0);
        float n = signNorm * FastACos(cosNorm);

        // Lowest possible horizons are limited by the normal hemisphere
        float lowHorizonCos0 = cos(n + PI_HALF);
        float lowHorizonCos1 = cos(n - PI_HALF);
        float horizonCos0 = lowHorizonCos0;
        float horizonCos1 = lowHorizonCos1;

        vec2 uvStep = omega * screenRadius * pc.TexelSize;
        for (int stepIndex = 0; stepIndex < pc.StepsPerSlice; stepIndex++)
        {
            float s = (float(stepIndex) + noiseSample) / float(pc.StepsPerSlice);
            s = s * s + minS; // Quadratic distribution, more samples close to the pixel

            vec2 sampleOffset = uvStep * s;
            vec3 delta0 = FetchViewPosition(inUV + sampleOffset) - viewPos;
            vec3 delta1 = FetchViewPosition(inUV - sampleOffset) - viewPos;

            float dist0 = length(delta0);
            float dist1 = length(delta1);

            float weight0 = clamp(dist0 * falloffMul + falloffAdd, 0.0, 1.0);
            float weight1 = clamp(dist1 * falloffMul + falloffAdd, 0.0, 1.0);

            float shc0 = mix(lowHorizonCos0, dot(delta0 / max(dist0, 1e-5), viewVec), weight0);
            float shc1 = mix(lowHorizonCos1, dot(delta1 / max(dist1, 1e-5), viewVec), weight1);

            horizonCos0 = max(horizonCos0, shc0);
            horizonCos1 = max(horizonCos1, shc1);
        }

        projectedNormalLength = mix(projectedNormalLength, 1.0, 0.05); // Fudge from XeGTAO, avoids over darkening at grazing angles

        float h0 = -FastACos(horizonCos1);
        float h1 = FastACos(horizonCos0);
        h0 = n + clamp(h0 - n, -PI_HALF, PI_HALF);
        h1 = n + clamp(h1 - n, -PI_HALF, PI_HALF);

        // Cosine weighted visibility integral over the slice
        float sinN = sin(n);
        float iarc0 = (cosNorm + 2.0 * h0 * sinN - cos(2.0 * h0 - n)) * 0.25;
        float iarc1 = (cosNorm + 2.0 * h1 * sinN - cos(2.0 * h1 - n)) * 0.25;
        visibility += projectedNormalLength * (iarc0 + iarc1);
    }

    visibility /= float(pc.SliceCount);
    visibility = pow(clamp(visibility, 0.0, 1.0), pc.Intensity);

    // Fade out towards MaxDistance so distant noise / precision issues never show
    visibility = mix(visibility, 1.0, smoothstep(pc.MaxDistance * 0.75, pc.MaxDistance, viewPos.z));

    outAO = max(visibility, 0.03);
}
