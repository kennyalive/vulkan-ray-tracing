#version 460
#extension GL_NVX_raytracing : require

#include "shared.glsl"
#include "utils.glsl"

layout (location=0) rayPayloadInNVX Payload payload;

void main() {
    payload.color = srgb_encode(vec3(0.32f, 0.32f, 0.4f));
}
