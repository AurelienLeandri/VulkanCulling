#pragma once

#include <GeometryIncludes.h>

namespace leo {
	struct alignas(16) SphereData {
		glm::vec3 position = glm::vec3(0);
		float radius = 1.f;
	};
}
