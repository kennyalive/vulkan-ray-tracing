#include "mesh.h"

#include <algorithm>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace std {
template<> struct hash<Vertex> {
    size_t operator()(Vertex const& v) const {
        size_t hash = 0;
        hash_combine(hash, v.pos);
        hash_combine(hash, v.normal);
        hash_combine(hash, v.uv);
        return hash;
    }
};
}

static inline bool operator==(const Vertex& v1, const Vertex& v2) {
    return v1.pos == v2.pos && v1.normal == v2.normal && v1.uv == v2.uv;
}

static void compute_normals(Mesh& mesh) {
    std::unordered_map<Vector3, std::vector<uint32_t>> duplicated_vertices; // due to different texture coordinates
    for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); i++) {
        const Vector3& pos = mesh.vertices[i].pos;
        duplicated_vertices[pos].push_back(i);
    }

    std::vector<bool> has_duplicates(mesh.vertices.size());
    for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); i++) {
        const Vector3& pos = mesh.vertices[i].pos;
        uint32_t vertex_count = (uint32_t)duplicated_vertices[pos].size();
        assert(vertex_count > 0);
        has_duplicates[i] = vertex_count > 1;
    }

    for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); i++)
        mesh.vertices[i].normal = Vector3();

    for (uint32_t i = 0; i < (uint32_t)mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i + 0];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];

        Vector3 a = mesh.vertices[i0].pos;
        Vector3 b = mesh.vertices[i1].pos;
        Vector3 c = mesh.vertices[i2].pos;

        Vector3 d1 = b - a;
        assert(d1.length() > 1e-6f);
        Vector3 d2 = c - a;
        assert(d2.length() > 1e-6f);

        Vector3 n = cross(d1, d2).normalized();

        if (has_duplicates[i0]) {
            for (uint32_t vi : duplicated_vertices[a])
                mesh.vertices[vi].normal += n;
        } else {
            mesh.vertices[i0].normal += n;
        }

        if (has_duplicates[i1]) {
            for (uint32_t vi : duplicated_vertices[b])
                mesh.vertices[vi].normal += n;
        } else {
            mesh.vertices[i1].normal += n;
        }

        if (has_duplicates[i2]) {
            for (uint32_t vi : duplicated_vertices[c])
                mesh.vertices[vi].normal += n;
        } else {
            mesh.vertices[i2].normal += n;
        }
    }

    for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); i++) {
        Vector3& n = mesh.vertices[i].normal;
        assert(n.length() > 1e-6f);
        n.normalize();
    }
}

Mesh load_obj_mesh(const std::string& path, float additional_scale) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str()))
        error("failed to load obj model: " + path);

    std::unordered_map<Vertex, std::size_t> unique_vertices;
    Vector3 mesh_min(Infinity);
    Vector3 mesh_max(-Infinity);

    Mesh mesh;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex;

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (!attrib.normals.empty()) {
                assert(index.normal_index != -1);
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2],
                };
            } else {
                vertex.normal = Vector3_Zero;
            }

            if (!attrib.texcoords.empty()) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            } else {
                vertex.uv = Vector2_Zero;
            }

            if (unique_vertices.count(vertex) == 0) {
                unique_vertices[vertex] = mesh.vertices.size();
                mesh.vertices.push_back(vertex);

                // update mesh bounds
                mesh_min.x = std::min(mesh_min.x, vertex.pos.x);
                mesh_min.y = std::min(mesh_min.y, vertex.pos.y);
                mesh_min.z = std::min(mesh_min.z, vertex.pos.z);
                mesh_max.x = std::max(mesh_max.x, vertex.pos.x);
                mesh_max.y = std::max(mesh_max.y, vertex.pos.y);
                mesh_max.z = std::max(mesh_max.z, vertex.pos.z);
            }
            mesh.indices.push_back((uint32_t)unique_vertices[vertex]);
        }
    }

    if (attrib.normals.empty())
        compute_normals(mesh);

    // scale and center the mesh
    Vector3 diag = mesh_max - mesh_min;
    float max_size = std::max(diag.x, std::max(diag.y, diag.z));
    float scale = (2.f / max_size) * additional_scale;

    Vector3 center = (mesh_min + mesh_max) * 0.5f;
    for (auto& v : mesh.vertices) {
        v.pos -= center;
        v.pos *= scale;
    }
    return mesh;
}
