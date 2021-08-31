#pragma once

#include <Scene/Geometry/Shape.h>
#include <Scene/Geometry/Vertex.h>

#include <vector>

namespace leo {
	class Mesh : public Shape {
	public:
		Mesh() = default;
		Mesh(size_t nbVertices, size_t nbIndices);

	public:
		virtual float area() const;
		virtual glm::vec3 sample(const HitRecord& record, float& pdf) const;
		virtual float pdf(const glm::vec3& point, const HitRecord& record) const;
		virtual Type getType() const;

	public:
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};
}
