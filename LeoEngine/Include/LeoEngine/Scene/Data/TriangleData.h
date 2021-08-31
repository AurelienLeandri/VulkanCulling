#pragma once

#include <Scene/Data/Vertex.h>

#include <array>

namespace leo {
	struct TriangleData {
		TriangleData(const Vertex& v0, const Vertex& v1, const Vertex& v2);

		std::array<Vertex, 3> vertices;
	};
}
