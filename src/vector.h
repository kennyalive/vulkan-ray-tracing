#pragma once

#include "common.h"

#include <cmath>

struct Vector {
    float x, y, z;

    explicit Vector(float v = 0.f)
        : x(v), y(v), z(v) {}

    Vector(float x, float y, float z)
        : x(x), y(y), z(z) {}

    bool operator==(Vector v) const {
        return x == v.x && y == v.y && z == v.z;
    }

    bool operator!=(Vector v) const {
        return !(*this == v);
    }

    Vector operator-() const {
        return Vector(-x, -y, -z);
    }

    float operator[](int index) const {
        return (&x)[index];
    }

    float& operator[](int index) {
        return (&x)[index];
    }

    Vector& operator+=(const Vector& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vector& operator-=(const Vector& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Vector& operator*=(const Vector& v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }

    Vector& operator*=(float t) {
        x *= t;
        y *= t;
        z *= t;
        return *this;
    }

    Vector& operator/=(float t) {
        x /= t;
        y /= t;
        z /= t;
        return *this;
    }

    Vector operator/(float t) const {
        return Vector(x/t, y/t, z/t);
    }

    float length() const {
        return std::sqrt(squared_length());
    }

    float squared_length() const {
        return x*x + y*y + z*z;
    }

    Vector normalized() const {
        return *this / length();
    }

    bool is_normalized(float epsilon = 1e-3f) const {
        return std::abs(length() - 1.f) < epsilon;
    }
};

struct Vector2 {
    float x, y;

    explicit Vector2(float v = 0.f)
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

    explicit Vector4(float v = 0.f)
        : x(v), y(v), z(v), w(v) {}

    Vector4(float x, float y, float z, float w)
        : x(x), y(y), z(z), w(w) {}

    Vector4(Vector xyz, float w)
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

inline Vector operator+(const Vector& v1, const Vector& v2) {
    return Vector(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
}

inline Vector operator-(const Vector& v1, const Vector& v2) {
    return Vector(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

inline Vector operator*(const Vector& v1, const Vector& v2) {
    return Vector(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z);
}

inline Vector operator*(const Vector& v, float t) {
    return Vector(v.x * t, v.y * t, v.z * t);
}

inline Vector operator*(float t, const Vector& v) {
    return v * t;
}

inline float dot(const Vector& v1, const Vector& v2) {
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

inline Vector cross(const Vector& v1, const Vector& v2) {
    return Vector(
        v1.y*v2.z - v1.z*v2.y,
        v1.z*v2.x - v1.x*v2.z,
        v1.x*v2.y - v1.y*v2.x);
}

namespace std {
template<> struct hash<Vector> {
    size_t operator()(Vector v) const {
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
