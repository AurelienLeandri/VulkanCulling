#pragma once

#include <Scene/Shapes/Shape.h>

namespace leo {
	struct TriangleData;
	struct Vertex;

	class Triangle : public Shape
	{
	public:
		Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);
		Triangle(const TriangleData* data);

	private:
		const TriangleData* const _data;
	};
}
