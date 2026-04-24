#version 440

// Per-vertex (quad corner): 2 floats
layout(location = 0) in vec2 cornerOffset;  // {-1,-1}, {1,-1}, {1,1}, {-1,1}

// Per-instance: 3 floats (x, y, colorValue)
layout(location = 1) in vec3 instanceData;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out float v_colorValue;

layout(std140, binding = 0) uniform Params {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;
    float offsetY;
    float halfSize;   // marker half-size in logical pixels
    float useColorAxis; // 0.0 = no, 1.0 = yes
};

void main()
{
    // Instance position in logical pixels + pan offset
    float px = (instanceData.x + offsetX) * dpr;
    float py = (instanceData.y + offsetY) * dpr;

    // Quad corner offset in physical pixels
    float cx = cornerOffset.x * halfSize * dpr;
    float cy = cornerOffset.y * halfSize * dpr;

    float ndcX = ((px + cx) / width) * 2.0 - 1.0;
    float ndcY = yFlip * (((py + cy) / height) * 2.0 - 1.0);

    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_uv = cornerOffset * 0.5 + 0.5;  // map [-1,1] -> [0,1]
    v_colorValue = instanceData.z;
}
