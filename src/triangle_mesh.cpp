#include "std.h"
#include "triangle_mesh.h"
#include "lib.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <unordered_map>

Triangle_Mesh load_obj_model(const std::string& path, float additional_scale) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str()))
        error("failed to load obj model: " + path);

	assert(shapes.size() == 1);
	const tinyobj::mesh_t& obj_mesh = shapes[0].mesh;

	for (uint8_t num_face_vertices : obj_mesh.num_face_vertices)
		assert(num_face_vertices == 3);

	struct Index_Hasher {
		size_t operator()(const tinyobj::index_t& index) const {
			size_t hash = 0;
			hash_combine(hash, index.vertex_index);
			hash_combine(hash, index.normal_index);
			hash_combine(hash, index.texcoord_index);
			return hash;
		}
	};
	struct Index_Comparator {
		bool operator()(const tinyobj::index_t& a, const tinyobj::index_t& b) const {
			return
				a.vertex_index == b.vertex_index &&
				a.normal_index == b.normal_index &&
				a.texcoord_index == b.texcoord_index;
		}
	};
	std::unordered_map<tinyobj::index_t, int, Index_Hasher, Index_Comparator> index_mapping;

	Vector3 mesh_min(Infinity);
	Vector3 mesh_max(-Infinity);

	Triangle_Mesh mesh;
	mesh.indices.reserve(obj_mesh.indices.size());

	for (const tinyobj::index_t& index : obj_mesh.indices) {
		auto it = index_mapping.find(index);
		if (it == index_mapping.end()) {
			it = index_mapping.insert(std::make_pair(index, (int)mesh.vertices.size())).first;

			// add new vertex
			Vertex vertex;
			assert(index.vertex_index != -1);
			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};
			if (index.texcoord_index != -1) {
				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.f - attrib.texcoords[2 * index.texcoord_index + 1]
				};
			}
			mesh.vertices.push_back(vertex);

			// update mesh bounds
			mesh_min.x = std::min(mesh_min.x, vertex.pos.x);
			mesh_min.y = std::min(mesh_min.y, vertex.pos.y);
			mesh_min.z = std::min(mesh_min.z, vertex.pos.z);
			mesh_max.x = std::max(mesh_max.x, vertex.pos.x);
			mesh_max.y = std::max(mesh_max.y, vertex.pos.y);
			mesh_max.z = std::max(mesh_max.z, vertex.pos.z);
		}
		mesh.indices.push_back(it->second);
	}

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
