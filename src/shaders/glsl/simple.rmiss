#version 460
#extension GL_NVX_raytracing : require

#include "utils.glsl"

layout (location=0) rayPayloadInNVX vec3 color;

void main() {
    color = srgb_encode(vec3(0.32f, 0.32f, 0.4f));
}
