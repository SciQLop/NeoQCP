#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 v_color;

layout(std140, binding = 0) uniform ViewportParams {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;  // per-draw pixel offset
    float offsetY;
    float alpha;    // busy fade
} pc;

void main()
{
    float ndcX = ((position.x + pc.offsetX) * pc.dpr / pc.width) * 2.0 - 1.0;
    float ndcY = pc.yFlip * (((position.y + pc.offsetY) * pc.dpr / pc.height) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_color = color * pc.alpha;
}
