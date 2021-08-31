#pragma once

#include <Scene/Textures/Texture.h>

#include <Scene/Data/ImageTextureData.h>

namespace leo {
	class ImageTexture : public Texture
	{
	public:
		ImageTexture(const ImageTextureData* data);

	public:
		virtual glm::vec4 getTexel(float u, float v) const override;

	public:
		void getDimensions(size_t& width, size_t& height, size_t& nbChannels) const;
		ImageTextureData::Type getType() const;
		ImageTextureData::Layout getLayout() const;

	private:
		const ImageTextureData* const _data;
	};
}
