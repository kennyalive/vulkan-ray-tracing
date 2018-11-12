struct Payload {
    vec3 rx_dir;
    vec3 ry_dir;
    vec3 color;
};

struct Vertex {
    vec3 p;
    vec3 n;
    vec2 uv;
};

struct Ray {
    vec3 origin;
    vec3 dir;
    vec3 rx_dir;
    vec3 ry_dir;
};

#ifdef RGEN_SHADER
vec3 get_direction(vec2 film_position) {
    const float tan_fovy_over_2 = 0.414; // tan(45/2)

    vec2 uv = film_position / vec2(gl_LaunchSizeNVX.xy);
    float aspect_ratio = float(gl_LaunchSizeNVX.x) / float(gl_LaunchSizeNVX.y);
    float horz_half_dist = aspect_ratio * tan_fovy_over_2;
    float vert_half_dist = tan_fovy_over_2;
    vec2 uv2 = 2.0 * uv - 1.0;
    float dir_x = uv2.x * horz_half_dist;
    float dir_y = -uv2.y * vert_half_dist;
    return normalize(vec3(dir_x, dir_y, -1.f));
}

Ray generate_ray(mat4x3 camera_to_world, vec2 film_position) {
    Ray ray;
    ray.origin = camera_to_world[3];
    ray.dir = camera_to_world * vec4(get_direction(film_position), 0);
    ray.rx_dir = camera_to_world * vec4(get_direction(vec2(film_position.x + 1.f, film_position.y)), 0);
    ray.ry_dir = camera_to_world * vec4(get_direction(vec2(film_position.x, film_position.y + 1.f)), 0);
    return ray;
}
#endif // RGEN_SHADER

#ifdef HIT_SHADER
float compute_texture_lod(Vertex v0, Vertex v1, Vertex v2, vec3 rx_dir, vec3 ry_dir, int mip_levels) {
    const vec3 p0 = v0.p;
    const vec3 p1 = v1.p;
    const vec3 p2 = v2.p;
    const vec2 uv0 = v0.uv;
    const vec2 uv1 = v1.uv;
    const vec2 uv2 = v2.uv;

    vec3 face_normal = normalize(cross(p1 - p0, p2 - p0));

    // compute dp/vu, dp/dv (PBRT, 3.6.2)
    vec3 dpdu, dpdv;
    {
        vec3 p10 = p1 - p0;
        vec3 p20 = p2 - p0;
        vec2 c1, c2;
        solve_2x2_helper(uv1.x - uv0.x, uv1.y - uv0.y, uv2.x - uv0.x, uv2.y - uv0.y, c1, c2);
        dpdu = c1.x*p10 + c1.y*p20;
        dpdv = c2.x*p10 + c2.y*p20;
    }

    // compute offsets from main intersection point to approximated intersections of auxilary rays
    vec3 dpdx, dpdy;
    {
        vec3 p = gl_WorldRayOriginNVX + gl_WorldRayDirectionNVX * gl_HitTNVX;
        float plane_d = -dot(face_normal, p);

        float tx = ray_plane_intersection(gl_WorldRayOriginNVX, rx_dir, face_normal, plane_d);
        float ty = ray_plane_intersection(gl_WorldRayOriginNVX, ry_dir, face_normal, plane_d);

        vec3 px = gl_WorldRayOriginNVX + rx_dir * tx;
        vec3 py = gl_WorldRayOriginNVX + ry_dir * ty;

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

    // To satisfy Nyquist limit the filter width should be twice as large as below and it is
    // achieved implicitly by using bilinear filtering to sample mip levels.
    //float filter_width = max(max(abs(dudx), abs(dvdx)), max(abs(dudy), abs(dvdy)));
    float filter_width = max(length(vec2(abs(dudx), abs(dvdx))), length(vec2(abs(dudy), abs(dvdy))));

    return mip_levels - 1 + log2(filter_width);
}
#endif // HIT_SHADER
