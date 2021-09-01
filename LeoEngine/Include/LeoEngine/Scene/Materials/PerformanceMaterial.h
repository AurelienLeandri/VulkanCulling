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
		glm::vec3 diffuseValue;
		std::shared_ptr<ImageTexture> diffuseTexture;
		glm::vec3 specularValue;
		std::shared_ptr<ImageTexture> specularTexture;
		glm::vec3 ambientValue;
		std::shared_ptr<ImageTexture> ambientTexture;
		glm::vec3 emissiveValue;
		std::shared_ptr<ImageTexture> emissiveTexture;
		glm::vec3 opacityValue;
		std::shared_ptr<ImageTexture> opacityTexture;
		std::shared_ptr<ImageTexture> normalTexture;
		std::shared_ptr<ImageTexture> occlusionTexture;
	};
}

