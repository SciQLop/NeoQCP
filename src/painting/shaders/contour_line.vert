#version 440

layout(location = 0) in vec2 uv;

layout(std140, binding = 0) uniform ContourLineParams {
    vec4 lineColor;
    vec2 ndcMin;
    vec2 ndcMax;
};

void main()
{
    vec2 ndc = ndcMin + uv * (ndcMax - ndcMin);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
