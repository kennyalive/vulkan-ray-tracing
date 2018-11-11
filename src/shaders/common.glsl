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

void solve_2x2_helper(float a, float b, float c, float d, out vec2 c1, out vec2 c2) {
    // |a b| |x1|  |b1|
    // |c d| |x2|  |b2|
    float det = a*d - b*c;

    float inv_det = 0.0;
    if (abs(det) > 1e-6)
         inv_det = 1.0 / det;

    c1 = inv_det * vec2(d, -b);
    c2 = inv_det * vec2(-c, a);
}

float ray_plane_intersection(vec3 ray_o, vec3 ray_d, vec3 plane_n, float plane_d)
{
    return (-plane_d - dot(plane_n, ray_o)) / dot(plane_n, ray_d);
}

vec2 barycentric_interpolate(float b1, float b2, vec2 v0, vec2 v1, vec2 v2)
{
    return (1.0 - b1 - b2)*v0 + b1*v1 + b2*v2;
}

vec3 barycentric_interpolate(float b1, float b2, vec3 v0, vec3 v1, vec3 v2)
{
    return (1.0 - b1 - b2)*v0 + b1*v1 + b2*v2;
}
