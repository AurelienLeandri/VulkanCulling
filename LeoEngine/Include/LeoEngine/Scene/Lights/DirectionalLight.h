#pragma once

#include <Scene/Lights/Light.h>

#include <GeometryIncludes.h>

namespace leo {
	class DirectionalLight : public Light {
	public:
		DirectionalLight(glm::vec3 direction, glm::vec3 emission);

	public:
		virtual Type getType() const;

	public:
		glm::vec3 direction = glm::vec3(0, -1, 0);
		glm::vec3 emission = glm::vec3(0);
	};
}
