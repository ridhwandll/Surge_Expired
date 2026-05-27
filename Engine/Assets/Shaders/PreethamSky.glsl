//SURGE:[Shader: Fragment]
#version 450

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform FrameUBO
{
    mat4 ViewProjection;
    mat4 InverseViewProjection;
    vec3 CameraPos;
    float _pad;
} frame;

layout(push_constant) uniform PC
{
    vec3  SunDirection; // Normalized world direction TOWARDS sun
    float Turbidity;
    float Exposure;
    float SunIntensity;
    int EnableSunDisk;
} pc;

// PreethamSky model (proudly stolen from Shadertoy)
// https://www.shadertoy.com/view/llSSDR

#define PI 3.14159265359

float SaturatedDot(vec3 a, vec3 b)
{
    return max(dot(a, b), 0.0);
}

vec3 YxyToXYZ(vec3 Yxy)
{
    float Y = Yxy.r;
    float x = Yxy.g;
    float y = Yxy.b;
    float X = x * (Y / y);
    float Z = (1.0 - x - y) * (Y / y);
    return vec3(X, Y, Z);
}

vec3 XYZToRGB(vec3 XYZ)
{
    // CIE/E
    mat3 M = mat3(
         2.3706743, -0.9000405, -0.4706338,
        -0.5138850,  1.4253036,  0.0885814,
         0.0052982, -0.0146949,  1.0093968
    );
    return XYZ * M;
}

vec3 YxyToRGB(vec3 Yxy)
{
    return XYZToRGB(YxyToXYZ(Yxy));
}

void CalculatePerezDistribution(float t, out vec3 A, out vec3 B, out vec3 C, out vec3 D, out vec3 E)
{
    A = vec3( 0.1787 * t - 1.4630, -0.0193 * t - 0.2592, -0.0167 * t - 0.2608);
    B = vec3(-0.3554 * t + 0.4275, -0.0665 * t + 0.0008, -0.0950 * t + 0.0092);
    C = vec3(-0.0227 * t + 5.3251, -0.0004 * t + 0.2125, -0.0079 * t + 0.2102);
    D = vec3( 0.1206 * t - 2.5771, -0.0641 * t - 0.8989, -0.0441 * t - 1.6537);
    E = vec3(-0.0670 * t + 0.3703, -0.0033 * t + 0.0452, -0.0109 * t + 0.0529);
}

vec3 CalculateZenithLuminanceYxy(in float t, in float thetaS)
{
    float chi = (4.0/9.0 - t/120.0) * (PI - 2.0 * thetaS);
    float Yz = (4.0453 * t - 4.9710) * tan(chi) - 0.2155 * t + 2.4192;

    float theta2 = thetaS * thetaS;
    float theta3 = theta2 * thetaS;
    float T2 = t * t;

    float xz = ( 0.00165 * theta3 - 0.00375 * theta2 + 0.00209 * thetaS) * T2
             + (-0.02903 * theta3 + 0.06377 * theta2 - 0.03202 * thetaS + 0.00394) * t
             + ( 0.11693 * theta3 - 0.21196 * theta2 + 0.06052 * thetaS + 0.25886);

    float yz = ( 0.00275 * theta3 - 0.00610 * theta2 + 0.00317 * thetaS) * T2
             + (-0.04214 * theta3 + 0.08970 * theta2 - 0.04153 * thetaS + 0.00516) * t
             + ( 0.15346 * theta3 - 0.26756 * theta2 + 0.06670 * thetaS + 0.26688);

    return vec3(Yz, xz, yz);
}

vec3 CalculatePerezLuminanceYxy(in float theta, in float gamma, in vec3 A, in vec3 B, in vec3 C, in vec3 D, in vec3 E)
{
    return (1.0 + A * exp(B / cos(theta))) * (1.0 + C * exp(D * gamma) + E * cos(gamma) * cos(gamma));
}

vec3 CalculateSkyLuminanceRGB(in vec3 s, in vec3 e, in float t)
{
    vec3 A, B, C, D, E;
    CalculatePerezDistribution(t, A, B, C, D, E);

    float thetaS = acos(SaturatedDot(s, vec3(0,1,0)));
    float thetaE = acos(SaturatedDot(e, vec3(0,1,0)));
    float gammaE = acos(SaturatedDot(s, e));

    vec3 Yz = CalculateZenithLuminanceYxy(t, thetaS);

    vec3 fThetaGamma = CalculatePerezLuminanceYxy(thetaE, gammaE, A, B, C, D, E);
    vec3 fZeroThetaS = CalculatePerezLuminanceYxy(0.0,    thetaS, A, B, C, D, E);

    vec3 Yp = Yz * (fThetaGamma / max(fZeroThetaS, 0.0001));
    return YxyToRGB(Yp);
}

void main()
{
    vec4 clipPos = vec4(inUV * 2.0 - 1.0, 0.0, 1.0);
    vec4 worldPos = frame.InverseViewProjection * clipPos;
    vec3 rayDir = normalize(worldPos.xyz / worldPos.w - frame.CameraPos);

    vec3 skyColor = CalculateSkyLuminanceRGB(pc.SunDirection, rayDir, pc.Turbidity) * pc.Exposure;

    if (pc.EnableSunDisk == 1)
    {
        float cosGamma = clamp(dot(rayDir, pc.SunDirection), -1.0, 1.0);
        float sunDisk  = smoothstep(0.9997, 0.9999, cosGamma);
        skyColor += vec3(1.0, 0.95, 0.8) * sunDisk * pc.SunIntensity;
    }

    outColor = vec4(skyColor, 1.0);
}

//SURGE:[Shader: Vertex]
#version 450
#extension GL_KHR_vulkan_glsl: enable

layout(location = 0) out vec2 outUV;

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 1.0, 1.0); //z = 1.0 (Rid) This will change probably if we reverse Z, w = 1.0
}