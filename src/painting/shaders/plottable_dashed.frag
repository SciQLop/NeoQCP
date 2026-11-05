#version 440

layout(location = 0) in vec4 v_color;
layout(location = 1) in float v_distance;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform ViewportParams {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;
    float offsetY;
    float alpha;
    float dashOffset;
    float patternLength;
    float patternCount;
    float pad0;
    float pad1;
    vec4 pattern[2];
} pc;

void main()
{
    // The pattern is on/off lengths in pixels along the line; the vertex shader carries the arc
    // length, so the dash phase belongs to the geometry and survives a pan.
    float phase = mod(v_distance + pc.dashOffset, pc.patternLength);
    for (int i = 0; i < 8; ++i)
    {
        if (float(i) >= pc.patternCount)
            break;
        float length = i < 4 ? pc.pattern[0][i] : pc.pattern[1][i - 4];
        if (phase < length)
        {
            if ((i % 2) == 1)
                discard;
            fragColor = v_color;
            return;
        }
        phase -= length;
    }
    fragColor = v_color;
}
