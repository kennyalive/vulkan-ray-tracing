#version 460
#extension GL_NVX_raytracing : require

#include "utils.glsl"

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

layout (location=0) rayPayloadInNVX vec3 color;
layout (location=1) hitAttributeNVX vec3 attribs;

layout(std430, binding=3) readonly buffer Indices {
    uint index_buffer[];
};

layout(std430, binding=4) readonly buffer Vertices {
    Vertex vertex_buffer[];
};

layout(binding=5) uniform texture2D image;
layout(binding=6) uniform sampler image_sampler;

void main() {
    uint i0 = uint(index_buffer[gl_PrimitiveID*3 + 0]);
    uint i1 = uint(index_buffer[gl_PrimitiveID*3 + 1]);
    uint i2 = uint(index_buffer[gl_PrimitiveID*3 + 2]);

    vec2 uv0 = fract(vec2(vertex_buffer[i0].u, vertex_buffer[i0].v));
    vec2 uv1 = fract(vec2(vertex_buffer[i1].u, vertex_buffer[i1].v));
    vec2 uv2 = fract(vec2(vertex_buffer[i2].u, vertex_buffer[i2].v));

    vec3 b = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 uv = fract(uv0*b.x + uv1*b.y + uv2*b.z);
    color = srgb_encode(texture(sampler2D(image, image_sampler), uv).rgb);
}
