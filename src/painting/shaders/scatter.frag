#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 1) in float v_colorValue;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D spriteTexture;
layout(binding = 2) uniform sampler2D colormapTexture;

layout(std140, binding = 0) uniform Params {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;
    float offsetY;
    float halfSize;
    float useColorAxis;
};

void main()
{
    vec4 sprite = texture(spriteTexture, v_uv);

    if (useColorAxis > 0.5) {
        // Color from 1D gradient, alpha from sprite
        vec4 cmapColor = texture(colormapTexture, vec2(v_colorValue, 0.5));
        fragColor = vec4(cmapColor.rgb * sprite.a, sprite.a);
    } else {
        // Full RGBA from sprite (color baked in)
        fragColor = sprite;
    }
}
