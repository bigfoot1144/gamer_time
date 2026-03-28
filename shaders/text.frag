#version 450

layout(set = 0, binding = 0) uniform sampler2D font_atlas;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 sample_color = texture(font_atlas, frag_uv);
    out_color = sample_color;
}
