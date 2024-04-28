#include "std.h"
#include "linear_algebra.h"

const Matrix3x4 Matrix3x4::identity = [] {
    Matrix3x4 m{};
    m.a[0][0] = m.a[1][1] = m.a[2][2] = 1.f;
    return m;
}();

const Matrix4x4 Matrix4x4::identity = [] {
    Matrix4x4 m{};
    m.a[0][0] = m.a[1][1] = m.a[2][2] = m.a[3][3] = 1.f;
    return m;
}();

void Matrix3x4::set_column(int column_index, Vector3 c) {
    assert(column_index >= 0 && column_index < 4);
    a[0][column_index] = c.x;
    a[1][column_index] = c.y;
    a[2][column_index] = c.z;
}

Vector3 Matrix3x4::get_column(int c) const {
    assert(c >= 0 && c < 4);
    return Vector3(a[0][c], a[1][c], a[2][c]);
}

void Matrix3x4::set_row(int row_index, Vector4 r) {
    assert(row_index >= 0 && row_index < 3);
    a[row_index][0] = r.x;
    a[row_index][1] = r.y;
    a[row_index][2] = r.z;
    a[row_index][3] = r.w;
}

Vector4 Matrix3x4::get_row(int row) const {
    assert(row >= 0 && row < 3);
    return Vector4(a[row][0], a[row][1], a[row][2], a[row][3]);
}

Matrix3x4 operator*(const Matrix3x4& m1, const Matrix3x4& m2) {
    Matrix3x4 m;
    m.a[0][0] = m1.a[0][0]*m2.a[0][0] + m1.a[0][1]*m2.a[1][0] + m1.a[0][2]*m2.a[2][0];
    m.a[0][1] = m1.a[0][0]*m2.a[0][1] + m1.a[0][1]*m2.a[1][1] + m1.a[0][2]*m2.a[2][1];
    m.a[0][2] = m1.a[0][0]*m2.a[0][2] + m1.a[0][1]*m2.a[1][2] + m1.a[0][2]*m2.a[2][2];
    m.a[0][3] = m1.a[0][0]*m2.a[0][3] + m1.a[0][1]*m2.a[1][3] + m1.a[0][2]*m2.a[2][3] + m1.a[0][3];

    m.a[1][0] = m1.a[1][0]*m2.a[0][0] + m1.a[1][1]*m2.a[1][0] + m1.a[1][2]*m2.a[2][0];
    m.a[1][1] = m1.a[1][0]*m2.a[0][1] + m1.a[1][1]*m2.a[1][1] + m1.a[1][2]*m2.a[2][1];
    m.a[1][2] = m1.a[1][0]*m2.a[0][2] + m1.a[1][1]*m2.a[1][2] + m1.a[1][2]*m2.a[2][2];
    m.a[1][3] = m1.a[1][0]*m2.a[0][3] + m1.a[1][1]*m2.a[1][3] + m1.a[1][2]*m2.a[2][3] + m1.a[1][3];

    m.a[2][0] = m1.a[2][0]*m2.a[0][0] + m1.a[2][1]*m2.a[1][0] + m1.a[2][2]*m2.a[2][0];
    m.a[2][1] = m1.a[2][0]*m2.a[0][1] + m1.a[2][1]*m2.a[1][1] + m1.a[2][2]*m2.a[2][1];
    m.a[2][2] = m1.a[2][0]*m2.a[0][2] + m1.a[2][1]*m2.a[1][2] + m1.a[2][2]*m2.a[2][2];
    m.a[2][3] = m1.a[2][0]*m2.a[0][3] + m1.a[2][1]*m2.a[1][3] + m1.a[2][2]*m2.a[2][3] + m1.a[2][3];
    return m;
}

Matrix4x4 operator*(const Matrix4x4& m1, const Matrix3x4& m2) {
    Matrix4x4 m;
    m.a[0][0] = m1.a[0][0]*m2.a[0][0] + m1.a[0][1]*m2.a[1][0] + m1.a[0][2]*m2.a[2][0];
    m.a[0][1] = m1.a[0][0]*m2.a[0][1] + m1.a[0][1]*m2.a[1][1] + m1.a[0][2]*m2.a[2][1];
    m.a[0][2] = m1.a[0][0]*m2.a[0][2] + m1.a[0][1]*m2.a[1][2] + m1.a[0][2]*m2.a[2][2];
    m.a[0][3] = m1.a[0][0]*m2.a[0][3] + m1.a[0][1]*m2.a[1][3] + m1.a[0][2]*m2.a[2][3] + m1.a[0][3];

    m.a[1][0] = m1.a[1][0]*m2.a[0][0] + m1.a[1][1]*m2.a[1][0] + m1.a[1][2]*m2.a[2][0];
    m.a[1][1] = m1.a[1][0]*m2.a[0][1] + m1.a[1][1]*m2.a[1][1] + m1.a[1][2]*m2.a[2][1];
    m.a[1][2] = m1.a[1][0]*m2.a[0][2] + m1.a[1][1]*m2.a[1][2] + m1.a[1][2]*m2.a[2][2];
    m.a[1][3] = m1.a[1][0]*m2.a[0][3] + m1.a[1][1]*m2.a[1][3] + m1.a[1][2]*m2.a[2][3] + m1.a[1][3];

    m.a[2][0] = m1.a[2][0]*m2.a[0][0] + m1.a[2][1]*m2.a[1][0] + m1.a[2][2]*m2.a[2][0];
    m.a[2][1] = m1.a[2][0]*m2.a[0][1] + m1.a[2][1]*m2.a[1][1] + m1.a[2][2]*m2.a[2][1];
    m.a[2][2] = m1.a[2][0]*m2.a[0][2] + m1.a[2][1]*m2.a[1][2] + m1.a[2][2]*m2.a[2][2];
    m.a[2][3] = m1.a[2][0]*m2.a[0][3] + m1.a[2][1]*m2.a[1][3] + m1.a[2][2]*m2.a[2][3] + m1.a[2][3];

    m.a[3][0] = m1.a[3][0]*m2.a[0][0] + m1.a[3][1]*m2.a[1][0] + m1.a[3][2]*m2.a[2][0];
    m.a[3][1] = m1.a[3][0]*m2.a[0][1] + m1.a[3][1]*m2.a[1][1] + m1.a[3][2]*m2.a[2][1];
    m.a[3][2] = m1.a[3][0]*m2.a[0][2] + m1.a[3][1]*m2.a[1][2] + m1.a[3][2]*m2.a[2][2];
    m.a[3][3] = m1.a[3][0]*m2.a[0][3] + m1.a[3][1]*m2.a[1][3] + m1.a[3][2]*m2.a[2][3] + m1.a[3][3];
    return m;
}

Matrix3x4 get_inverse(const Matrix3x4& m) {
    Vector3 x_axis = m.get_column(0);
    Vector3 y_axis = m.get_column(1);
    Vector3 z_axis = m.get_column(2);
    Vector3 origin = m.get_column(3);

    Matrix3x4 m_inv;
    m_inv.set_row(0, Vector4(x_axis, -dot(x_axis, origin)));
    m_inv.set_row(1, Vector4(y_axis, -dot(y_axis, origin)));
    m_inv.set_row(2, Vector4(z_axis, -dot(z_axis, origin)));
    return m_inv;
}

Matrix3x4 rotate_x(const Matrix3x4& m, float angle) {
    float cs = std::cos(angle);
    float sn = std::sin(angle);

    Matrix3x4 m2;
    m2.a[0][0] = m.a[0][0];
    m2.a[0][1] = m.a[0][1];
    m2.a[0][2] = m.a[0][2];
    m2.a[0][3] = m.a[0][3];

    m2.a[1][0] = cs*m.a[1][0] - sn*m.a[2][0];
    m2.a[1][1] = cs*m.a[1][1] - sn*m.a[2][1];
    m2.a[1][2] = cs*m.a[1][2] - sn*m.a[2][2];
    m2.a[1][3] = cs*m.a[1][3] - sn*m.a[2][3];

    m2.a[2][0] = sn*m.a[1][0] + cs*m.a[2][0];
    m2.a[2][1] = sn*m.a[1][1] + cs*m.a[2][1];
    m2.a[2][2] = sn*m.a[1][2] + cs*m.a[2][2];
    m2.a[2][3] = sn*m.a[1][3] + cs*m.a[2][3];
    return m2;
}

Matrix3x4 rotate_y(const Matrix3x4& m, float angle) {
    float cs = std::cos(angle);
    float sn = std::sin(angle);

    Matrix3x4 m2;
    m2.a[0][0] = cs*m.a[0][0] + sn*m.a[2][0];
    m2.a[0][1] = cs*m.a[0][1] + sn*m.a[2][1];
    m2.a[0][2] = cs*m.a[0][2] + sn*m.a[2][2];
    m2.a[0][3] = cs*m.a[0][3] + sn*m.a[2][3];

    m2.a[1][0] = m.a[1][0];
    m2.a[1][1] = m.a[1][1];
    m2.a[1][2] = m.a[1][2];
    m2.a[1][3] = m.a[1][3];

    m2.a[2][0] = -sn*m.a[0][0] + cs*m.a[2][0];
    m2.a[2][1] = -sn*m.a[0][1] + cs*m.a[2][1];
    m2.a[2][2] = -sn*m.a[0][2] + cs*m.a[2][2];
    m2.a[2][3] = -sn*m.a[0][3] + cs*m.a[2][3];
    return m2;
}

Matrix3x4 rotate_z(const Matrix3x4& m, float angle) {
    float cs = std::cos(angle);
    float sn = std::sin(angle);

    Matrix3x4 m2;
    m2.a[0][0] = cs*m.a[0][0] - sn*m.a[1][0];
    m2.a[0][1] = cs*m.a[0][1] - sn*m.a[1][1];
    m2.a[0][2] = cs*m.a[0][2] - sn*m.a[1][2];
    m2.a[0][3] = cs*m.a[0][3] - sn*m.a[1][3];

    m2.a[1][0] = sn*m.a[0][0] + cs*m.a[1][0];
    m2.a[1][1] = sn*m.a[0][1] + cs*m.a[1][1];
    m2.a[1][2] = sn*m.a[0][2] + cs*m.a[1][2];
    m2.a[1][3] = sn*m.a[0][3] + cs*m.a[1][3];
    
    m2.a[2][0] = m.a[2][0];
    m2.a[2][1] = m.a[2][1];
    m2.a[2][2] = m.a[2][2];
    m2.a[2][3] = m.a[2][3];
    return m2;
}

Matrix3x4 look_at_transform(Vector3 from, Vector3 to, Vector3 up) {
    assert(up.is_normalized());

    Vector3 f = to - from;
    float d = f.length();

    // degenerated cases, just return matrix with identity orientation
    if (d < 1e-5f || std::abs(dot(f, up) - 1.f) < 1e-3f) {
        Matrix3x4 m = Matrix3x4::identity;
        m.set_column(3, from);
        return m;
    }

    f /= d;
    Vector3 r = cross(f, up).normalized();
    Vector3 u = cross(r, f);

    Matrix3x4 m;
    m.set_row(0, Vector4(r, -dot(from, r)));
    m.set_row(1, Vector4(u, -dot(from, u)));
    m.set_row(2, Vector4(-f, -dot(from, -f)));
    return m;
}

Matrix4x4 perspective_transform_opengl_z01(float fovy_radians, float aspect_ratio, float near, float far) {
    float h = std::tan(fovy_radians/2.f) * near;
    float w = aspect_ratio * h;

    Matrix4x4 proj{};
    proj.a[0][0] = near / w;
    proj.a[1][1] = -near / h;
    proj.a[2][2] = -far / (far - near);
    proj.a[2][3] = -far*near / (far - near);
    proj.a[3][2] = -1.f;
    return proj;
}

Vector3 transform_point(const Matrix3x4& m, Vector3 p) {
    Vector3 p2;
    p2.x = m.a[0][0]*p.x + m.a[0][1]*p.y + m.a[0][2]*p.z + m.a[0][3];
    p2.y = m.a[1][0]*p.x + m.a[1][1]*p.y + m.a[1][2]*p.z + m.a[1][3];
    p2.z = m.a[2][0]*p.x + m.a[2][1]*p.y + m.a[2][2]*p.z + m.a[2][3];
    return p2;
}

Vector3 transform_vector(const Matrix3x4& m, Vector3 v) {
    Vector3 v2;
    v2.x = m.a[0][0]*v.x + m.a[0][1]*v.y + m.a[0][2]*v.z;
    v2.y = m.a[1][0]*v.x + m.a[1][1]*v.y + m.a[1][2]*v.z;
    v2.z = m.a[2][0]*v.x + m.a[2][1]*v.y + m.a[2][2]*v.z;
    return v2;
}
