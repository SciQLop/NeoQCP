#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D spriteTexture;

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
    // The sprite is a shape mask here; the colour is premultiplied, so alpha counts once.
    fragColor = v_color * texture(spriteTexture, v_uv).a * alpha;
}
