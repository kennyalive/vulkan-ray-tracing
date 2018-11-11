#version 460
#extension GL_GOOGLE_include_directive : require
#include "shared.glsl"

layout(location=0) in Frag_In frag_in;
layout(location = 0) out vec4 color_attachment0;

layout(binding=1) uniform texture2D image;
layout(binding=2) uniform sampler image_sampler;

void main() {
    // float2 uvdx = abs(ddx(input.uv));
    // float2 uvdy = abs(ddy(input.uv));
    // float filter_width = 2.0 * max(max(uvdx[0], uvdx[1]), max(uvdy[0], uvdy[1]));
    // float f = 150.0 * (filter_width - 0.0015);
    // return float4(srgb_encode(f), srgb_encode(f), srgb_encode(f), 1.0);

    vec3 color = texture(sampler2D(image, image_sampler), frag_in.uv).xyz;
    color_attachment0 = vec4(srgb_encode(color), 1);
}
