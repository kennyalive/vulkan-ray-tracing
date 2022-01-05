#pragma once

#include "linear_algebra.h"
#include <vector>

struct Vertex {
    Vector3 pos;
    Vector2 uv;
};

struct Triangle_Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

Triangle_Mesh load_obj_model(const std::string& path, float additional_scale);
