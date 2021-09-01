#pragma once

#include <Scene/Materials/Material.h>
#include <GeometryIncludes.h>

#include <memory>

namespace leo {
	class ImageTexture;
	class PerformanceMaterial : public Material
	{
	public:
		virtual Type getType() const override;

	public:
		glm::vec3 diffuseValue = glm::vec3(1);
		std::shared_ptr<ImageTexture> diffuseTexture;
		glm::vec3 specularValue = glm::vec3(1);
		std::shared_ptr<ImageTexture> specularTexture;
		glm::vec3 ambientValue = glm::vec3(0);
		std::shared_ptr<ImageTexture> ambientTexture;
		glm::vec3 emissiveValue = glm::vec3(0);
		std::shared_ptr<ImageTexture> emissiveTexture;
		float opacityValue = 1;
		std::shared_ptr<ImageTexture> opacityTexture;  // TODO: default value!
		std::shared_ptr<ImageTexture> normalsTexture;  // TODO: default value!
		std::shared_ptr<ImageTexture> occlusionTexture;  // TODO: default value!
	};
}

