#include <Scene/Textures/ImageTexture.h>

namespace leo {

	ImageTexture::ImageTexture(const ImageTextureData* data)
		: Texture(Texture::Type::IMAGE), _data(data)
	{
	}

	glm::vec4 ImageTexture::getTexel(float u, float v) const
	{
		size_t width, height, nbChannels;
		getDimensions(width, height, nbChannels);
		float dummy;
		u = glm::modf(u, dummy);
		v = glm::modf(v, dummy);
		if (u < 0) u += 1.f;
		if (v < 0) v += 1.f;
		size_t i = static_cast<size_t>(u * width);
		size_t j = static_cast<size_t>((1.f - v) * height);
		size_t index = (nbChannels * width) * j + (i * nbChannels);
		glm::vec4 texel(0);
		for (size_t channel = 0; channel < nbChannels; ++channel) {
			texel[channel] = _data->data[static_cast<glm::vec4::length_type>(index + channel)];
		}
		return texel;
	}

	void ImageTexture::getDimensions(size_t& width, size_t& height, size_t& nbChannels) const
	{
		width = _data->width;
		height = _data->height;
		nbChannels = static_cast<size_t>(_data->layout);
	}

	ImageTextureData::Type ImageTexture::getType() const
	{
		return _data->type;
	}

	ImageTextureData::Layout ImageTexture::getLayout() const
	{
		return _data->layout;
	}

}
