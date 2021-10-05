#include <Scene/Textures/ImageTexture.h>

namespace leo {

	const std::shared_ptr<const ImageTexture> ImageTexture::white = std::make_shared<const ImageTexture>(1, 1, Type::FLOAT, Layout::RGB, new float[3]{1, 1, 1});
	const std::shared_ptr<const ImageTexture> ImageTexture::black = std::make_shared<const ImageTexture>(1, 1, Type::FLOAT, Layout::RGB, new float[3]{ 0, 0, 0 });
	const std::shared_ptr<const ImageTexture> ImageTexture::blue = std::make_shared<const ImageTexture>(1, 1, Type::FLOAT, Layout::RGB, new float[3]{ 0, 0, 1 });

	ImageTexture::ImageTexture(size_t width, size_t height, Type type, Layout layout, float* data) :
		Texture(Texture::Type::IMAGE),
		width(width),
		height(height),
		nbChannels(getNbChannelsFromLayout(layout)),
		type(type),
		layout(layout),
		data(data ? data : new float[width * height * nbChannels])
	{
	}

	ImageTexture::~ImageTexture()
	{
		if (data) {
			delete[] data;
		}
	}

	glm::vec4 ImageTexture::getTexel(float u, float v) const
	{
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

	size_t ImageTexture::getNbChannelsFromLayout(ImageTexture::Layout layout)
	{
		switch (layout) {
		case ImageTexture::Layout::A:
			return 1;
		case ImageTexture::Layout::RGB:
			return 3;
		case ImageTexture::Layout::RGBA:
			return 4;
		case ImageTexture::Layout::LUMINANCE:
			return 1;
		default:
			return 0;
		}
	}

}
