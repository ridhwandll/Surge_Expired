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

// Separable depth aware (bilateral) gaussian blur, run once horizontally and once vertically
// Removes the 4x4 GTAO noise pattern without bleeding occlusion across depth discontinuities

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outAO;

layout(binding = 0, set = 0) uniform sampler2D uAO;
layout(binding = 1, set = 0) uniform sampler2D uLinearDepth;

layout(push_constant) uniform PushConstants
{
    vec2 Direction; // One AO texel along the blur axis, in UV units
    float Sharpness;
    int Radius;
} pc;

void main()
{
    float centerDepth = textureLod(uLinearDepth, inUV, 0.0).r;
    float totalAO = textureLod(uAO, inUV, 0.0).r;
    float totalWeight = 1.0;

    float sigma = (float(pc.Radius) + 1.0) * 0.5;
    float gaussianFalloff = 1.0 / (2.0 * sigma * sigma);
    float invCenterDepth = 1.0 / max(centerDepth, 1e-3);

    for (int i = 1; i <= pc.Radius; i++)
    {
        float spatialWeight = exp(-float(i * i) * gaussianFalloff);
        vec2 offset = pc.Direction * float(i);

        for (int side = 0; side < 2; side++)
        {
            vec2 uv = side == 0 ? inUV + offset : inUV - offset;
            float relativeDelta = (textureLod(uLinearDepth, uv, 0.0).r - centerDepth) * invCenterDepth * pc.Sharpness;
            float weight = spatialWeight * exp2(-relativeDelta * relativeDelta);

            totalAO += textureLod(uAO, uv, 0.0).r * weight;
            totalWeight += weight;
        }
    }

    outAO = totalAO / totalWeight;
}
