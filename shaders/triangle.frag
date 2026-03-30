#version 450

layout(push_constant) uniform ScenePushConstants {
    vec2 cameraCenter;
    vec2 viewportSize;
    vec2 atlasGrid;
    vec2 fogSize;
    float zoom;
} scene;

layout(set = 0, binding = 0) uniform sampler2D uSceneAtlas;
layout(set = 0, binding = 1) uniform sampler2D uFogMask;

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec2 vWorldPos;
layout(location = 2) flat in uint vSpriteIndex;
layout(location = 3) flat in uint vFlags;
layout(location = 4) in vec2 vAtlasUv;
layout(location = 0) out vec4 outColor;

vec3 fallback_color(uint spriteIndex) {
    float idx = float(spriteIndex % 7u);
    return vec3(
        0.30 + 0.10 * mod(idx + 0.0, 3.0),
        0.45 + 0.10 * mod(idx + 1.0, 3.0),
        0.25 + 0.12 * mod(idx + 2.0, 3.0)
    );
}

void main() {
    vec4 atlasColor = texture(uSceneAtlas, vAtlasUv);

    vec2 fogUv = clamp(vWorldPos / max(scene.fogSize, vec2(1.0)), vec2(0.0), vec2(1.0));
    float fogValue = texture(uFogMask, fogUv).r;

    vec3 color = atlasColor.a > 0.01 ? atlasColor.rgb : fallback_color(vSpriteIndex);
    if ((vFlags & 1u) != 0u) {
        float highlight = smoothstep(0.15, 0.85, 1.0 - distance(vUv, vec2(0.5)));
        color = mix(color, vec3(1.0, 0.9, 0.2), 0.35 * highlight);
    }

    color *= mix(0.85, 1.0, fogValue);
    outColor = vec4(color, max(atlasColor.a, 1.0));
}
