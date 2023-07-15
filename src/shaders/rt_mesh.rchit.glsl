#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_tracing_position_fetch : require

#include "common.glsl"

#define HIT_SHADER
#include "rt_utils.glsl"

hitAttributeEXT vec2 attribs;

struct Buffer_Vertex {
    float x, y, z;
    float u, v;
};

layout(push_constant) uniform Push_Constants {
      layout(offset = 4) uint show_texture_lods;
};

layout (location=0) rayPayloadInEXT Ray_Payload payload;

layout(std430, binding=3) readonly buffer Indices {
    uint index_buffer[];
};

layout(std430, binding=4) readonly buffer Vertices {
    Buffer_Vertex vertex_buffer[];
};

layout(binding=5) uniform texture2D image;
layout(binding=6) uniform sampler image_sampler;

Vertex fetch_vertex(int vertex_index) {
    uint i = index_buffer[vertex_index];
    Buffer_Vertex bv = vertex_buffer[i];

    Vertex v;
    v.uv = fract(vec2(bv.u, bv.v));
    return v;
}

void main() {
    Vertex v0 = fetch_vertex(gl_PrimitiveID*3 + 0);
    Vertex v1 = fetch_vertex(gl_PrimitiveID*3 + 1);
    Vertex v2 = fetch_vertex(gl_PrimitiveID*3 + 2);

    v0.p = gl_HitTriangleVertexPositionsEXT[0];
    v1.p = gl_HitTriangleVertexPositionsEXT[1];
    v2.p = gl_HitTriangleVertexPositionsEXT[2];

    v0.p = gl_ObjectToWorldEXT * vec4(v0.p, 1);
    v1.p = gl_ObjectToWorldEXT * vec4(v1.p, 1);
    v2.p = gl_ObjectToWorldEXT * vec4(v2.p, 1);

    int mip_levels = textureQueryLevels(sampler2D(image, image_sampler));
    float lod = compute_texture_lod(v0, v1, v2, payload.rx_dir, payload.ry_dir, mip_levels);

    vec3 color;
    if (show_texture_lods != 0) {
        color = color_encode_lod(lod);
    } else {
        vec2 uv = fract(barycentric_interpolate(attribs.x, attribs.y, v0.uv, v1.uv, v2.uv));
        color = textureLod(sampler2D(image, image_sampler), uv, lod).rgb;
    }

    payload.color = srgb_encode(color);
}
