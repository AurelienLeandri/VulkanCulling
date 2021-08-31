#include <Scene/Textures/ImageTexture.h>

namespace leo {

	ImageTexture::ImageTexture(size_t width, size_t height, Type type, Layout layout)
		: Texture(Texture::Type::IMAGE), width(width), height(height), type(type), layout(layout), data(new float[width * height * static_cast<size_t>(layout)])
	{
	}

	ImageTexture::~ImageTexture()
	{
		delete[] data;
		data = nullptr;
	}

	glm::vec4 ImageTexture::getTexel(float u, float v) const
	{
		size_t nbChannels = static_cast<size_t>(layout);
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
			texel[static_cast<glm::vec4::length_type>(channel)] = data[index + channel];
		}
		return texel;
	}

}
