#pragma once

#include "vector.h"

struct Matrix3x4 {
    float a[3][4];
    static const Matrix3x4 identity;

    void set_column(int column_index, Vector3 c);
    void set_row(int row_index, Vector4 r);
    Vector4 get_row(int row) const;
};

struct Matrix4x4 {
    float a[4][4];
    static const Matrix4x4 identity;
};

Matrix3x4 operator*(const Matrix3x4& m1, const Matrix3x4& m2);
Matrix4x4 operator*(const Matrix4x4& m1, const Matrix3x4& m2);

// assumption is that matrix contains only rotation and translation.
Matrix3x4 get_inverse(const Matrix3x4& m);

// rotate_[axis] functions premultiply a given matrix by corresponding rotation matrix.
Matrix3x4 rotate_x(const Matrix3x4& m, float angle);
Matrix3x4 rotate_y(const Matrix3x4& m, float angle);
Matrix3x4 rotate_z(const Matrix3x4& m, float angle);

// Computes world space->eye space transform that positions the camera at point 'from'
// and orients its direction towards the point 'to'. 'up' unit vector specifies reference up direction.
Matrix3x4 look_at_transform(Vector3 from, Vector3 to, Vector3 up);

// Computes traditional perspective matrix that transforms position vector (x,y,z,1) to
// obtain clip coordinates (xc, yc, zc, wc) that can be transformed to normalized deviced
// coordinates (NDC) by perspective division (xd, yd, zd) = (xc/wc, yc/wc, zc/wc).
// Eye-space z-axis points towards the viewer (OpenGL style), right-handed coordinate system.
// z coordinate is mapped to 0 and 1 for near and far planes correspondingly. y axis in NDC
// space points top-down with regard to eye space vertical direction (to match Vulkan viewport).
Matrix4x4 perspective_transform_opengl_z01(float fovy_radians, float aspect_ratio, float near, float far);

Vector3 transform_point(const Matrix3x4& m, Vector3 p);
Vector3 transform_vector(const Matrix3x4& m, Vector3 v);
