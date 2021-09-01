#pragma once

#include <Scene/Textures/Texture.h>

namespace leo {
	class ImageTexture : public Texture
	{
	public:
		enum class Type {
			FLOAT
		};
		enum class Layout : size_t {
			RGB = 3,
			RGBA = 4,
			LUMINANCE = 1,
			A = 1
		};

	public:
		ImageTexture(size_t width, size_t height, Type type, Layout layout);
		~ImageTexture();

	public:
		virtual glm::vec4 getTexel(float u, float v) const override;

	public:
		const size_t width = 0;
		const size_t height = 0;
		const Type type;
		const Layout layout;
		float* data;
	};
}
