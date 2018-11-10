layout(row_major) uniform;

struct Frag_In {
    vec3 normal;
    vec2 uv;
};

struct Payload {
    vec3 rx_dir;
    vec3 ry_dir;
    vec3 color;
};


float srgb_encode(float c) {
    if (c <= 0.0031308f)
        return 12.92f * c;
    else
        return 1.055f * pow(c, 1.f/2.4f) - 0.055f;
}

vec3 srgb_encode(vec3 c) {
    return vec3(srgb_encode(c.r), srgb_encode(c.g), srgb_encode(c.b));
}
