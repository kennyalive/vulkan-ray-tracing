#version 460
#extension GL_NVX_raytracing : require
#extension GL_EXT_shader_16bit_storage : require

struct Vertex {
    vec3 pos;
    float pad;
    vec2 uv;
    vec2 pad2;
};

layout (location=0) rayPayloadInNVX vec3 color;
layout (location=1) hitAttributeNVX vec3 attribs;

layout(std430, binding=2) readonly buffer Indices {
    uint16_t index_buffer[];
};

layout(std430, binding=3) readonly buffer Vertices {
    Vertex vertex_buffer[];
};

layout(binding=4) uniform texture2D image;
layout(binding=5) uniform sampler image_sampler;

float srgb_encode(float c) {
    if (c <= 0.0031308f)
        return 12.92f * c;
    else
        return 1.055f * pow(c, 1.f/2.4f) - 0.055f;
}

vec3 srgb_encode(vec3 c) {
    return vec3(srgb_encode(c.r), srgb_encode(c.g), srgb_encode(c.b));
}

void main() {
    uint i0 = uint(index_buffer[gl_PrimitiveID*3 + 0]);
    uint i1 = uint(index_buffer[gl_PrimitiveID*3 + 1]);
    uint i2 = uint(index_buffer[gl_PrimitiveID*3 + 2]);

    vec2 uv0 = fract(vertex_buffer[i0].uv);
    vec2 uv1 = fract(vertex_buffer[i1].uv);
    vec2 uv2 = fract(vertex_buffer[i2].uv);

    vec3 b = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 uv = fract(uv0*b.x + uv1*b.y + uv2*b.z);
    color = srgb_encode(texture(sampler2D(image, image_sampler), uv).rgb);
}
