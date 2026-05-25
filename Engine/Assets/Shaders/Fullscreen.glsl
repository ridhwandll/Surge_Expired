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
precision highp float;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform sampler2D sceneColor;
layout(binding = 1, set = 0) uniform sampler2D sceneDepth;

layout(push_constant) uniform PushConstants
{
    vec4  ColorThickness;
    vec2  ScreenResolution;
    vec2  CameraPlanes;
} pc;

float LinearizeDepth(float d, float near, float far)
{
    return (near * far) / (far - d * (far - near));
}

bool IsSkybox(float d)
{
    return d >= 0.9999 || d <= 0.00001;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / pc.ScreenResolution;
    vec2 texel = 1.0 / pc.ScreenResolution;
    vec2 offset = texel * max(pc.ColorThickness.a, 1.0);

    float near = pc.CameraPlanes.x;
    float far = pc.CameraPlanes.y;
    float c_raw = texture(sceneDepth, uv).r;
    vec4 scene = texture(sceneColor, uv);

    if (IsSkybox(c_raw))
    {
        outColor = scene;
        return;
    }

    float c = LinearizeDepth(c_raw, near, far);

    float n_raw = texture(sceneDepth, uv + vec2( 0.0,      offset.y)).r;
    float s_raw = texture(sceneDepth, uv + vec2( 0.0,     -offset.y)).r;
    float e_raw = texture(sceneDepth, uv + vec2( offset.x,  0.0    )).r;
    float w_raw = texture(sceneDepth, uv + vec2(-offset.x,  0.0    )).r;

    float n = IsSkybox(n_raw) ? far : LinearizeDepth(n_raw, near, far);
    float s = IsSkybox(s_raw) ? far : LinearizeDepth(s_raw, near, far);
    float e = IsSkybox(e_raw) ? far : LinearizeDepth(e_raw, near, far);
    float w = IsSkybox(w_raw) ? far : LinearizeDepth(w_raw, near, far);

    float maxDiff = max(max(abs(c - n), abs(c - s)), max(abs(c - e), abs(c - w)));

    float threshold = c * 0.08;
    float edge = smoothstep(threshold, threshold * 1.5, maxDiff);
    float fade = 1.0 - smoothstep(20.0, 40.0, c);
    edge *= fade;

    outColor = mix(scene, vec4(pc.ColorThickness.rgb, 1.0), edge);
}