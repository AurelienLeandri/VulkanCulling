#pragma once

#include "Texture.h"

#include <memory>

namespace leo {
	class ImageTexture : public Texture
	{
	public:
		enum class Type {
			INVALID = 0,
			FLOAT
		};
		enum class Layout {
			INVALID = 0,
			RGB,
			RGBA,
			LUMINANCE,
			R
		};

	public:
		ImageTexture(size_t width, size_t height, Type type, Layout layout, unsigned char* data = nullptr);
		~ImageTexture();

	public:
		virtual glm::vec4 getTexel(float u, float v) const override;

	public:
		static size_t getNbChannelsFromLayout(ImageTexture::Layout layout);

	public:
		const size_t width = 0;
		const size_t height = 0;
		const size_t nbChannels = 0;
		const Type type = Type::INVALID;
		const Layout layout = Layout::INVALID;
		const unsigned char* data = nullptr;

	public:
		static const std::shared_ptr<const ImageTexture> white;
		static const std::shared_ptr<const ImageTexture> black;
		static const std::shared_ptr<const ImageTexture> blue;
	};
}
