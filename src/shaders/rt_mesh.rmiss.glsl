#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_NVX_raytracing : require

#include "common.glsl"
#include "rt_utils.glsl"

layout (location=0) rayPayloadInNVX Payload payload;

void main() {
    payload.color = srgb_encode(vec3(0.32f, 0.32f, 0.4f));
}
