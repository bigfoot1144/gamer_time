#version 450

vec2 positions[3] = vec2[](
    vec2( 0.0, -0.55),
    vec2( 0.55, 0.55),
    vec2(-0.55, 0.55)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.3, 0.3),
    vec3(0.3, 1.0, 0.4),
    vec3(0.3, 0.5, 1.0)
);

layout(location = 0) out vec3 vColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    vColor = colors[gl_VertexIndex];
}
