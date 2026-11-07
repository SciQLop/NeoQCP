#version 440

layout(location = 0) in vec2 v_texcoord;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D tex;

layout(std140, binding = 1) uniform ImageParams {
    float x0;
    float y0;
    float x1;
    float y1;
    float width;
    float height;
    float yFlip;
    float alpha;
    float u0;
    float v0;
    float u1;
    float v1;
} pc;

void main()
{
    fragColor = texture(tex, v_texcoord) * pc.alpha;
}
