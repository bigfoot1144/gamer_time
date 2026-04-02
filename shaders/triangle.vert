#version 450

layout(push_constant) uniform ScenePushConstants {
    vec2 cameraCenter;
    vec2 viewportSize;
    vec2 atlasGrid;
    vec2 fogSize;
    float zoom;
} scene;

layout(location = 0) in vec2 inQuadPos;
layout(location = 1) in vec2 inQuadUv;
layout(location = 2) in vec2 inWorldPos;
layout(location = 3) in vec2 inSize;
layout(location = 4) in uint inSpriteIndex;
layout(location = 5) in uint inFlags;
layout(location = 6) in float inOpacity;
layout(location = 7) in float inRotationRadians;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec2 vWorldPos;
layout(location = 2) flat out uint vSpriteIndex;
layout(location = 3) flat out uint vFlags;
layout(location = 4) out vec2 vAtlasUv;
layout(location = 5) out float vOpacity;

void main() {
    vec2 localOffset = (inQuadPos - vec2(0.5)) * inSize;
    float s = sin(inRotationRadians);
    float c = cos(inRotationRadians);
    localOffset = vec2(
        localOffset.x * c - localOffset.y * s,
        localOffset.x * s + localOffset.y * c
    );
    vec2 worldPos = inWorldPos + localOffset;
    vec2 screenPos = (worldPos - scene.cameraCenter) * scene.zoom + scene.viewportSize * 0.5;
    vec2 ndc = vec2(
        (screenPos.x / scene.viewportSize.x) * 2.0 - 1.0,
        1.0 - (screenPos.y / scene.viewportSize.y) * 2.0
    );

    gl_Position = vec4(ndc, 0.0, 1.0);
    vec2 tileUv = vec2(inQuadUv.x, 1.0 - inQuadUv.y);
    vUv = tileUv;
    vWorldPos = worldPos;
    vSpriteIndex = inSpriteIndex;
    vFlags = inFlags;
    vOpacity = inOpacity;

    vec2 atlasGrid = max(scene.atlasGrid, vec2(1.0));
    uint atlasColumns = uint(atlasGrid.x);
    float tileX = float(inSpriteIndex % atlasColumns);
    float tileRowFromTop = float(inSpriteIndex / atlasColumns);
    float tileY = tileRowFromTop;
    vAtlasUv = (vec2(tileX, tileY) + tileUv) / atlasGrid;
}
