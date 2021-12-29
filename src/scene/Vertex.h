#pragma once

#include "GeometryIncludes.h"

namespace leoscene {
	struct alignas(16) Vertex {
		glm::vec3 position = glm::vec3(0);
		glm::vec3 normal = glm::vec3(0, 0, 1);
		glm::vec3 tangent = glm::vec3(1, 0, 0);
		glm::vec2 uv = glm::vec2(0, 0);
	};
}
