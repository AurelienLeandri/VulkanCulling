#pragma once

#include <GeometryIncludes.h>

namespace leo {
	struct alignas(16) SphereData {
		SphereData(const glm::vec3& position = glm::vec3(0), float radius = 1.f) : position(position), radius(radius) {}

		glm::vec3 position = glm::vec3(0);
		float radius = 1.f;
	};
}
