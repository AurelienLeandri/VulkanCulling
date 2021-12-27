#include "Sphere.h"

namespace leo {
	Sphere::Sphere(const glm::vec3& position, float radius)
		: data({ position, radius })
	{
	}

	float Sphere::area() const
	{
		return 0.0f;
	}

	glm::vec3 Sphere::sample(const HitRecord& record, float& pdf) const
	{
		return glm::vec3();
	}

	float Sphere::pdf(const glm::vec3& point, const HitRecord& record) const
	{
		return 0.0f;
	}

	Shape::Type Sphere::getType() const
	{
		return Type::SPHERE;
	}


}
