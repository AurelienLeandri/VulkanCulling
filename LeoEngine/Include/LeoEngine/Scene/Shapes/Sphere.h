#pragma once

#include <Scene/Shapes/Shape.h>

namespace leo {
	struct SphereData;

	class Sphere : public Shape
	{
	public:
		Sphere();
		Sphere(const glm::vec3 position, float radius);
		Sphere(const SphereData* data);

	private:
		const SphereData* const _data;
	};
}
