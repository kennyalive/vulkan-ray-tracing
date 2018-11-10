#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_NVX_raytracing : require

#include "shared.glsl"
#include "utils.glsl"

layout(binding = 0, rgba8) uniform image2D image;
layout(binding = 1) uniform accelerationStructureNVX accel;

layout(binding=2) uniform Uniform_Block {
    mat4x3 camera_to_world;
    mat4x3 model_transform;
};

layout(location = 0) rayPayloadNVX Payload payload;

const float tmin = 1e-3f;
const float tmax = 1e+3f;

void main() {
    Ray ray = generate_ray(camera_to_world, vec2(gl_LaunchIDNVX.xy) + vec2(0.5));
    payload.rx_dir = ray.rx_dir;
    payload.ry_dir = ray.ry_dir;

    traceNVX(accel, gl_RayFlagsOpaqueNVX, 0xff, 0, 0, 0, ray.origin, tmin, ray.dir, tmax, 0);
    imageStore(image, ivec2(gl_LaunchIDNVX.xy), vec4(payload.color, 1.0));
}
