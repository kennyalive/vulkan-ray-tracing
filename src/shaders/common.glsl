layout(row_major) uniform;

struct Frag_In {
    vec3 normal;
    vec2 uv;
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

vec3 color_encode_lod(float lod) {
    uint color_mask = (uint(floor(lod)) + 1) & 7;
    vec3 color0 = vec3(float(color_mask&1), float(color_mask&2), float(color_mask&4));
    vec3 color1 = 0.25 * color0;
    return mix(color0, color1, fract(lod));
}

float ray_plane_intersection(vec3 ray_o, vec3 ray_d, vec3 plane_n, float plane_d) {
    return (-plane_d - dot(plane_n, ray_o)) / dot(plane_n, ray_d);
}

vec2 barycentric_interpolate(float b1, float b2, vec2 v0, vec2 v1, vec2 v2) {
    return (1.0 - b1 - b2)*v0 + b1*v1 + b2*v2;
}

vec3 barycentric_interpolate(float b1, float b2, vec3 v0, vec3 v1, vec3 v2) {
    return (1.0 - b1 - b2)*v0 + b1*v1 + b2*v2;
}

void coordinate_system_from_vector(vec3 v, out vec3 v1, out vec3 v2) {
    v1 = normalize(abs(v.x) > abs(v.y) ? vec3(-v.z, 0, v.x) : vec3(0, -v.z, v.y));
    v2 = cross(v, v1);
}
