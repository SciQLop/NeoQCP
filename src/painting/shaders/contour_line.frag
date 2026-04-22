#version 440

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform ContourLineParams {
    vec4 lineColor;
    vec2 ndcMin;
    vec2 ndcMax;
};

void main()
{
    fragColor = lineColor;
}
