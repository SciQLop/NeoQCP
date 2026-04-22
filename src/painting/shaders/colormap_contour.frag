#version 440

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D colormapTex;
layout(binding = 2) uniform sampler2D zTex;

layout(std140, binding = 3) uniform ContourParams {
    vec4 penColor;      // premultiplied RGBA
    float lineWidth;    // contour width in screen pixels
    int levelCount;     // >0 → use levels[], 0 → use spacing for auto
    float spacing;      // auto: normalized spacing; explicit: 1.0 (zRange)
    float zMin;         // unused (0), reserved
    float levels[32];   // normalized level values [0,1]
} cp;

void main()
{
    vec4 base = texture(colormapTex, v_texcoord);
    float z = texture(zTex, v_texcoord).r; // [0,1] normalized

    float dzdx = dFdx(z);
    float dzdy = dFdy(z);
    float gradMag = length(vec2(dzdx, dzdy));

    float minDist = 1e10;

    if (cp.levelCount > 0)
    {
        for (int i = 0; i < cp.levelCount && i < 32; ++i)
        {
            float d = abs(z - cp.levels[i]);
            minDist = min(minDist, d);
        }
    }
    else if (cp.spacing > 0.0)
    {
        float shifted = z / cp.spacing;
        float nearest = round(shifted) * cp.spacing;
        minDist = abs(z - nearest);
    }

    float pixelDist = (gradMag > 1e-10) ? minDist / gradMag : 1e10;
    float halfWidth = cp.lineWidth * 0.5;
    float alpha = 1.0 - smoothstep(halfWidth - 0.5, halfWidth + 0.5, pixelDist);

    fragColor = mix(base, cp.penColor, alpha);
}
