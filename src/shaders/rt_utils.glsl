struct Ray_Payload {
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

    vec2 uv = 2.0 * (film_position / vec2(gl_LaunchSizeNV.xy)) - 1.0;
    float aspect_ratio = float(gl_LaunchSizeNV.x) / float(gl_LaunchSizeNV.y);

    float dir_x =  uv.x *  aspect_ratio * tan_fovy_over_2;
    float dir_y = -uv.y * tan_fovy_over_2;
    return normalize(vec3(dir_x, dir_y, -1.f));
}

Ray generate_ray(mat4x3 camera_to_world, vec2 film_position) {
    Ray ray;
    ray.origin  = camera_to_world[3];
    ray.dir     = camera_to_world * vec4(get_direction(film_position), 0);
    ray.rx_dir  = camera_to_world * vec4(get_direction(vec2(film_position.x + 1.f, film_position.y)), 0);
    ray.ry_dir  = camera_to_world * vec4(get_direction(vec2(film_position.x, film_position.y + 1.f)), 0);
    return ray;
}
#endif // RGEN_SHADER

#ifdef HIT_SHADER
float compute_texture_lod(Vertex v0, Vertex v1, Vertex v2, vec3 rx_dir, vec3 ry_dir, int mip_levels) {
    vec3 face_normal = normalize(cross(v1.p - v0.p, v2.p - v0.p));

    // compute dp/vu, dp/dv (PBRT, 3.6.2)
    vec3 dpdu, dpdv;
    {
        vec3 p10 = v1.p - v0.p;
        vec3 p20 = v2.p - v0.p;

        float a00 = v1.uv.x - v0.uv.x; float a01 = v1.uv.y - v0.uv.y;
        float a10 = v2.uv.x - v0.uv.x; float a11 = v2.uv.y - v0.uv.y;

        float det = a00*a11 - a01*a10;
        if (abs(det) < 1e-10) {
            coordinate_system_from_vector(face_normal, dpdu, dpdv);
        } else {
            float inv_det = 1.0/det;
            dpdu = ( a11*p10 - a01*p20) * inv_det;
            dpdv = (-a10*p10 + a00*p20) * inv_det;
        }
    }

    // compute offsets from main intersection point to approximated intersections of auxilary rays
    vec3 dpdx, dpdy;
    {
        vec3 p = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
        float plane_d = -dot(face_normal, p);

        float tx = ray_plane_intersection(gl_WorldRayOriginNV, rx_dir, face_normal, plane_d);
        float ty = ray_plane_intersection(gl_WorldRayOriginNV, ry_dir, face_normal, plane_d);

        vec3 px = gl_WorldRayOriginNV + rx_dir * tx;
        vec3 py = gl_WorldRayOriginNV + ry_dir * ty;

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

        float a00 = dpdu[dim0]; float a01 = dpdv[dim0];
        float a10 = dpdu[dim1]; float a11 = dpdv[dim1];

        float det = a00*a11 - a01*a10;
        if (abs(det) < 1e-10)
            dudx = dvdx = dudy = dvdy = 0;
        else {
            float inv_det = 1.0/det;
            dudx = ( a11*dpdx[dim0] - a01*dpdx[dim1]) * inv_det;
            dvdx = (-a10*dpdx[dim0] - a00*dpdx[dim1]) * inv_det;

            dudy = ( a11*dpdy[dim0] - a01*dpdy[dim1]) * inv_det;
            dvdy = (-a10*dpdy[dim0] - a00*dpdy[dim1]) * inv_det;
        }
    }

    // To satisfy Nyquist limit the filter width should be twice as large as below and it is
    // achieved implicitly by using bilinear filtering to sample mip levels.
    //float filter_width = max(max(abs(dudx), abs(dvdx)), max(abs(dudy), abs(dvdy)));
    float filter_width = max(length(vec2(dudx, dvdx)), length(vec2(dudy, dvdy)));

    return mip_levels - 1 + log2(clamp(filter_width, 1e-6, 1.0));
}
#endif // HIT_SHADER
