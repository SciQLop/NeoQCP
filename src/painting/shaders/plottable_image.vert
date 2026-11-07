#version 440

layout(location = 0) in vec2 position; // unit quad, [0, 1]
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

// Matches ImageUniforms in plottable-rhi-layer.h: the rect is in device pixels and already carries
// the layer's pan offset, so a pan only rewrites this block. u0..v1 name the texels to show, which
// is the whole texture for a rasterised fill.
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
    float px = mix(pc.x0, pc.x1, position.x);
    float py = mix(pc.y0, pc.y1, position.y);
    float ndcX = (px / pc.width) * 2.0 - 1.0;
    float ndcY = pc.yFlip * ((py / pc.height) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_texcoord = vec2(mix(pc.u0, pc.u1, texcoord.x), mix(pc.v0, pc.v1, texcoord.y));
}
