#pragma once

#include <cmath>

struct Vector4;

struct Vector3 {
    float x, y, z;

    Vector3()
        : x(0.f), y(0.f), z(0.f) {}

    constexpr explicit Vector3(float v)
        : x(v), y(v), z(v) {}

    explicit Vector3(Vector4 v);

    Vector3(float x, float y, float z)
        : x(x), y(y), z(z) {}

    bool operator==(Vector3 v) const {
        return x == v.x && y == v.y && z == v.z;
    }

    bool operator!=(Vector3 v) const {
        return !(*this == v);
    }

    Vector3 operator-() const {
        return Vector3(-x, -y, -z);
    }

    float operator[](int index) const {
        return (&x)[index];
    }

    float& operator[](int index) {
        return (&x)[index];
    }

    Vector3& operator+=(const Vector3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vector3& operator-=(const Vector3& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Vector3& operator*=(const Vector3& v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }

    Vector3& operator*=(float t) {
        x *= t;
        y *= t;
        z *= t;
        return *this;
    }

    Vector3& operator/=(float t) {
        x /= t;
        y /= t;
        z /= t;
        return *this;
    }

    Vector3 operator/(float t) const {
        return Vector3(x/t, y/t, z/t);
    }

    float length() const {
        return std::sqrt(squared_length());
    }

    float squared_length() const {
        return x*x + y*y + z*z;
    }

    Vector3 normalized() const {
        return *this / length();
    }

    void normalize() {
        *this /= length();
    }

    bool is_normalized(float epsilon = 1e-3f) const {
        return std::abs(length() - 1.f) < epsilon;
    }
};

struct Vector2 {
    float x, y;

    Vector2()
        : x(0.f), y(0.f) {}

    constexpr explicit Vector2(float v)
        : x(v), y(v) {}

    Vector2(float x, float y)
        : x(x), y(y) {}

    bool operator==(Vector2 v) const {
        return x == v.x && y == v.y;
    }

    bool operator!=(Vector2 v) const {
        return !(*this == v);
    }

    float operator[](int index) const {
        return (&x)[index];
    }

    float& operator[](int index) {
        return (&x)[index];
    }
};

struct Vector4 {
    float x, y, z, w;

    Vector4()
        : x(0.f), y(0.f), z(0.f), w(0.f) {}

    constexpr explicit Vector4(float v)
        : x(v), y(v), z(v), w(v) {}

    Vector4(float x, float y, float z, float w)
        : x(x), y(y), z(z), w(w) {}

    Vector4(Vector3 xyz, float w)
        : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}

    bool operator==(Vector4 v) const {
        return x == v.x && y == v.y && z == v.z && w == v.w;
    }

    bool operator!=(Vector4 v) const {
        return !(*this == v);
    }

    float operator[](int index) const {
        return (&x)[index];
    }

    float& operator[](int index) {
        return (&x)[index];
    }
};

inline Vector3::Vector3(Vector4 v)
    : x(v.x), y(v.y), z(v.z) 
{}

inline Vector3 operator+(const Vector3& v1, const Vector3& v2) {
    return Vector3(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
}

inline Vector3 operator-(const Vector3& v1, const Vector3& v2) {
    return Vector3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

inline Vector3 operator*(const Vector3& v1, const Vector3& v2) {
    return Vector3(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z);
}

inline Vector3 operator*(const Vector3& v, float t) {
    return Vector3(v.x * t, v.y * t, v.z * t);
}

inline Vector3 operator*(float t, const Vector3& v) {
    return v * t;
}

inline float dot(const Vector3& v1, const Vector3& v2) {
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

inline Vector3 cross(const Vector3& v1, const Vector3& v2) {
    return Vector3(
        v1.y*v2.z - v1.z*v2.y,
        v1.z*v2.x - v1.x*v2.z,
        v1.x*v2.y - v1.y*v2.x);
}

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
