#include <Scene/Lights/DirectionalLight.h>

namespace leo {
	DirectionalLight::DirectionalLight(glm::vec3 direction, glm::vec3 emission) :
		direction(direction), emission(emission)
	{
	}

	Light::Type DirectionalLight::getType() const
	{
		return Type::DIRECTIONAL;
	}
}
