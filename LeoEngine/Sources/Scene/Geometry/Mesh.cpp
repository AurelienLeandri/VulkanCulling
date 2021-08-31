#include <Scene/Geometry/Mesh.h>

namespace leo {
	Mesh::Mesh(size_t nbVertices, size_t nbIndices)
		: vertices(nbVertices), indices(nbIndices)
	{
	}

	float Mesh::area() const
	{
		return 0.0f;
	}

	glm::vec3 Mesh::sample(const HitRecord& record, float& pdf) const
	{
		return glm::vec3();
	}

	float Mesh::pdf(const glm::vec3& point, const HitRecord& record) const
	{
		return 0.0f;
	}

	Shape::Type Mesh::getType() const
	{
		return Type::MESH;
	}
}
