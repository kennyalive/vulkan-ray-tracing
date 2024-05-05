#pragma once

#include <chrono>
#include <vector>

constexpr float Pi = 3.14159265f;
constexpr float Infinity = std::numeric_limits<float>::infinity();

inline float radians(float degrees) {
    constexpr float deg_2_rad = Pi / 180.f;
    return degrees * deg_2_rad;
}

inline float degrees(float radians) {
    constexpr float rad_2_deg = 180.f / Pi;
    return radians * rad_2_deg;
}

void error(const std::string& message);
std::string get_resource_path(const std::string& path_relative_data_directory);
std::vector<uint8_t> read_binary_file(const std::string& file_name);

struct Timestamp {
    Timestamp() : t(std::chrono::steady_clock::now()) {}
    std::chrono::time_point<std::chrono::steady_clock> t;
};
uint64_t elapsed_milliseconds(Timestamp timestamp);
uint64_t elapsed_nanoseconds(Timestamp timestamp);

// Boost hash combine.
template <typename T>
inline void hash_combine(std::size_t& seed, T value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline float srgb_encode(float f) {
    if (f <= 0.0031308f)
        return 12.92f * f;
    else
        return 1.055f * std::pow(f, 1.f/2.4f) - 0.055f;
}

template <typename T>
inline T round_up(T k, T alignment) {
    return (k + alignment - 1) & ~(alignment - 1);
}

