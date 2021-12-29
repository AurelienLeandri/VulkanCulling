#pragma once

#include "Light.h"
#include "GeometryIncludes.h"

namespace leoscene {
	class PointLight : public Light {
	public:
		glm::vec3 position;
		glm::vec3 emission;
	};
}
