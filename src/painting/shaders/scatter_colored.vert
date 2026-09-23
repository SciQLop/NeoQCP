#version 440

layout(location = 0) in vec2 cornerOffset;
layout(location = 1) in vec3 instanceData;   // x, y, unused
layout(location = 2) in vec4 instanceColor;  // premultiplied

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

layout(std140, binding = 0) uniform Params {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;
    float offsetY;
    float halfSize;
    float useColorAxis;
    float alpha;
};

void main()
{
    float px = (instanceData.x + offsetX) * dpr;
    float py = (instanceData.y + offsetY) * dpr;
    float cx = cornerOffset.x * halfSize * dpr;
    float cy = cornerOffset.y * halfSize * dpr;
    float ndcX = ((px + cx) / width) * 2.0 - 1.0;
    float ndcY = yFlip * (((py + cy) / height) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_uv = cornerOffset * 0.5 + 0.5;
    v_color = instanceColor;
}
