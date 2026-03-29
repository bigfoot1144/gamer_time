#version 450

layout(push_constant) uniform ScenePushConstants {
    vec2 cameraCenter;
    vec2 viewportSize;
    vec2 fogSize;
    float zoom;
} scene;

layout(set = 0, binding = 0) uniform sampler2D uFogMask;

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec2 vWorldPos;
layout(location = 2) flat in uint vSpriteIndex;
layout(location = 3) flat in uint vFlags;
layout(location = 0) out vec4 outColor;

vec3 sprite_color(uint spriteIndex) {
    float idx = float(spriteIndex % 7u);
    return vec3(
        0.25 + 0.10 * mod(idx + 0.0, 3.0),
        0.45 + 0.08 * mod(idx + 1.0, 3.0),
        0.30 + 0.12 * mod(idx + 2.0, 3.0)
    );
}

void main() {
    vec2 centeredUv = vUv - vec2(0.5);
    float dist = length(centeredUv);
    if (dist > 0.5) {
        discard;
    }

    vec2 fogUv = clamp(vWorldPos / max(scene.fogSize, vec2(1.0)), vec2(0.0), vec2(1.0));
    float fogValue = texture(uFogMask, fogUv).r;

    vec3 color = sprite_color(vSpriteIndex);
    if ((vFlags & 1u) != 0u) {
        float outline = smoothstep(0.42, 0.5, dist);
        color = mix(color, vec3(1.0, 0.9, 0.2), outline);
    }

    color *= mix(0.25, 1.0, fogValue);
    float alpha = smoothstep(0.5, 0.44, dist);
    outColor = vec4(color, alpha);
}
