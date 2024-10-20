#version 330 core

layout (location = 0) out vec4 color;

in vec2 tex_coord;
flat in int transparency_index;
flat in int remap_offset;
flat in int remap_rounds;
flat in int palette_offset;
flat in int palette_limit;

uniform sampler2D atlas;

vec4 handle(float index) {
    if (remap_rounds > 0) {
        float r_index = remap_offset / 255.0 + index;
        float r_rounds = remap_rounds / 255.0;
        return vec4(0.0, r_index, r_rounds, 1.0);
    }
    return vec4(index, 0.0, 0.0, 1.0);
}

void main() {
    vec4 texel = texture(atlas, tex_coord);

    // Don't render if it's transparent pixel
    int index = int(texel.r * 255.0);
    if (index == transparency_index) discard;

    // Palette offset and limit (for e.g. fonts)
    float limit = palette_limit / 255.0;
    float offset = palette_offset / 255.0;
    if (texel.r <= limit) {
        texel.r = clamp(texel.r + offset, 0, limit);
    }

    color = handle(texel.r);
}