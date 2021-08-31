#pragma once

#include <Scene/Data/Vertex.h>

#include <array>

namespace leo {
	class TriangleData {
	public:
		TriangleData(const Vertex& v0, const Vertex& v1, const Vertex& v2);

	public:
		const std::array<const Vertex, 3> vertices;
	};
}
