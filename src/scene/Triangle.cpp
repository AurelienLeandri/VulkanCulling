#include "Triangle.h"

namespace leoscene {
	Triangle::Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2)
		: data({v0, v1, v2})
	{
	}

	float Triangle::area() const
	{
		return 0.0f;
	}

	glm::vec3 Triangle::sample(const HitRecord& record, float& pdf) const
	{
		return glm::vec3();
	}

	float Triangle::pdf(const glm::vec3& point, const HitRecord& record) const
	{
		return 0.0f;
	}

	Shape::Type Triangle::getType() const
	{
		return Type::TRIANGLE;
	}
}
