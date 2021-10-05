#pragma once

#include <Scene/Materials/Material.h>
#include <Scene/Textures/ImageTexture.h>
#include <GeometryIncludes.h>

#include <memory>

namespace leo {
	class ImageTexture;
	class PerformanceMaterial : public Material
	{
	public:
		virtual Type getType() const override;

	public:
		std::shared_ptr<const ImageTexture> diffuseTexture = ImageTexture::white;
		std::shared_ptr<const ImageTexture> specularTexture = ImageTexture::white;
		std::shared_ptr<const ImageTexture> ambientTexture = ImageTexture::white;
		std::shared_ptr<const ImageTexture> normalsTexture = ImageTexture::blue;
		std::shared_ptr<const ImageTexture> heightTexture = ImageTexture::black;
	};
}

