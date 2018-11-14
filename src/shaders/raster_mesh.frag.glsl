#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

layout(push_constant) uniform Push_Constants {
    uint show_texture_lods;
};

layout(location=0) in Frag_In frag_in;
layout(location = 0) out vec4 color_attachment0;

layout(binding=1) uniform texture2D image;
layout(binding=2) uniform sampler image_sampler;

void main() {
    vec3 color;

    if (show_texture_lods != 0) {
        // The commented code below will give result very close to raytracing version.
        // The lod calculated by textureQueryLod gives a bit different result and that's
        // fine since implementations are not restricted to some fixed algorithm.
        // vec2 uvdx = abs(dFdx(frag_in.uv));
        // vec2 uvdy = abs(dFdy(frag_in.uv));
        // float filter_width = max(max(uvdx[0], uvdx[1]), max(uvdy[0], uvdy[1]));
        // float lod = textureQueryLevels(sampler2D(image, image_sampler)) - 1 + log2(filter_width);

        float lod = textureQueryLod(sampler2D(image, image_sampler), frag_in.uv).y;
        color = color_encode_lod(lod);
    } else {
        color = texture(sampler2D(image, image_sampler), frag_in.uv).xyz;
    }
    color_attachment0 = vec4(srgb_encode(color), 1);
}
