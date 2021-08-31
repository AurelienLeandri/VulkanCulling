#pragma once

#include <Scene/Data/Vertex.h>

#include <vector>

namespace leo {
	struct MeshData {
		MeshData() = default;
		MeshData(size_t nbVertices, size_t nbIndices);
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};
}
