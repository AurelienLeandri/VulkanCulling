#pragma once

#include <Scene/Lights/Light.h>

#include <GeometryIncludes.h>

namespace leo {
	class PointLight : public Light {
	public:
		glm::vec3 direction;
		glm::vec3 emission;
	};
}
