#pragma once

#include <Scene/Geometries/Vertex.h>
#include <Scene/Geometries/Shape.h>

#include <array>

namespace leo {
	class Triangle : public Shape {
	public:
		struct alignas(16) Data {
			std::array<Vertex, 3> vertices;
		};

	public:
		Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

	public:
		virtual float area() const;
		virtual glm::vec3 sample(const HitRecord& record, float& pdf) const;
		virtual float pdf(const glm::vec3& point, const HitRecord& record) const;
		virtual Type getType() const;

	public:
		Data data;
		std::array<Vertex, 3>& vertices = data.vertices;
	};
}
