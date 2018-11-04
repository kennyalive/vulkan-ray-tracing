#pragma once

#include "common.h"

#include <cmath>

struct Vector4;

struct Vector3 {
    float x, y, z;

    Vector3() {}
    
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

constexpr Vector3 Vector3_Zero = Vector3(0.f);

struct Vector2 {
    float x, y;

    Vector2() {}

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

constexpr Vector2 Vector2_Zero = Vector2(0.f);

struct Vector4 {
    float x, y, z, w;

    Vector4() {}

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

constexpr Vector4 Vector4_Zero = Vector4(0.f);

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

namespace std {
template<> struct hash<Vector3> {
    size_t operator()(Vector3 v) const {
        size_t hash = 0;
        hash_combine(hash, v.x);
        hash_combine(hash, v.y);
        hash_combine(hash, v.z);
        return hash;
    }
};

template<> struct hash<Vector2> {
    size_t operator()(Vector2 v) const {
        size_t hash = 0;
        hash_combine(hash, v.x);
        hash_combine(hash, v.y);
        return hash;
    }
};

template<> struct hash<Vector4> {
    size_t operator()(Vector4 v) const {
        size_t hash = 0;
        hash_combine(hash, v.x);
        hash_combine(hash, v.y);
        hash_combine(hash, v.z);
        hash_combine(hash, v.w);
        return hash;
    }
};
}
