#version 460
#extension GL_NVX_raytracing : require

layout(row_major) uniform;
#include "shared.glsl"
#include "utils.glsl"

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

layout (location=0) rayPayloadInNVX Payload payload;
layout (location=1) hitAttributeNVX vec3 attribs;

layout(binding=2) uniform Uniform_Block {
    mat4x3 camera_to_world;
    mat4x3 model_transform;
};

layout(std430, binding=3) readonly buffer Indices {
    uint index_buffer[];
};

layout(std430, binding=4) readonly buffer Vertices {
    Vertex vertex_buffer[];
};

layout(binding=5) uniform texture2D image;
layout(binding=6) uniform sampler image_sampler;

void main() {
    // fetch vertex data
    uint i0 = index_buffer[gl_PrimitiveID*3 + 0];
    uint i1 = index_buffer[gl_PrimitiveID*3 + 1];
    uint i2 = index_buffer[gl_PrimitiveID*3 + 2];

    Vertex V0 = vertex_buffer[i0];
    Vertex V1 = vertex_buffer[i1];
    Vertex V2 = vertex_buffer[i2];

    vec3 p0 = model_transform * vec4(V0.x, V0.y, V0.z, 1.0);
    vec3 p1 = model_transform * vec4(V1.x, V1.y, V1.z, 1.0);
    vec3 p2 = model_transform * vec4(V2.x, V2.y, V2.z, 1.0);

    float u0 = fract(V0.u);
    float v0 = fract(V0.v);
    float u1 = fract(V1.u);
    float v1 = fract(V1.v);
    float u2 = fract(V2.u);
    float v2 = fract(V2.v);

    vec3 face_normal = normalize(cross(p1 - p0, p2 - p0));

    // compute dp/vu, dp/dv (PBRT, 3.6.2)
    vec3 dpdu, dpdv;
    {
        vec3 p10 = p1 - p0;
        vec3 p20 = p2 - p0;
        vec2 c1, c2;
        solve_2x2_helper(u1 - u0, v1 - v0, u2 - u0, v2 - v0, c1, c2);
        dpdu = c1.x*p10 + c1.y*p20;
        dpdv = c2.x*p10 + c2.y*p20;
    }

    // compute offsets from main intersection point to approximated intersections of auxilary rays
    vec3 dpdx, dpdy;
    {
        vec3 p = gl_WorldRayOriginNVX + gl_WorldRayDirectionNVX * gl_HitTNVX;
        float plane_d = -dot(face_normal, p);

        float tx = ray_plane_intersection(gl_WorldRayOriginNVX, payload.rx_dir, face_normal, plane_d);
        float ty = ray_plane_intersection(gl_WorldRayOriginNVX, payload.ry_dir, face_normal, plane_d);

        vec3 px = gl_WorldRayOriginNVX + payload.rx_dir * tx;
        vec3 py = gl_WorldRayOriginNVX + payload.ry_dir * ty;

        dpdx = px - p;
        dpdy = py - p;
    }

    // compute du/dx, dv/dx, du/dy, dv/dy (PBRT, 10.1.1)
    float dudx, dvdx, dudy, dvdy;
    {
        uint dim0 = 0, dim1 = 1;
        vec3 a = abs(face_normal);
        if (a.x > a.y && a.x > a.z) {
            dim0 = 1;
            dim1 = 2;
        } else if (a.y > a.z) {
            dim0 = 0;
            dim1 = 2;
        }

        vec2 c1, c2;
        solve_2x2_helper(dpdu[dim0], dpdv[dim0], dpdu[dim1], dpdv[dim1], c1, c2);

        dudx = c1.x*dpdx[dim0] + c1.y*dpdx[dim1];
        dvdx = c2.x*dpdx[dim0] + c2.y*dpdx[dim1];

        dudy = c1.x*dpdy[dim0] + c1.y*dpdy[dim1];
        dvdy = c2.x*dpdy[dim0] + c2.y*dpdy[dim1];
    }

    // float filter_width = 2.0 * max(max(abs(dudx), abs(dvdx)), max(abs(dudy), abs(dvdy)));
    // float f = 150.0 * (filter_width - 0.0015);
    // payload.color = vec3(srgb_encode(f));

    vec2 uv = fract(barycentric_interpolate(attribs.x, attribs.y, vec2(u0, v0), vec2(u1, v1), vec2(u2, v2)));
    payload.color = srgb_encode(texture(sampler2D(image, image_sampler), uv).rgb);
}
