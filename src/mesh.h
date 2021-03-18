#pragma once

#include "vector.h"
#include <vector>

struct Vertex {
    Vector3 pos;
    Vector3 normal;
    Vector2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

Mesh load_obj_mesh(const std::string& path, float additional_scale);
