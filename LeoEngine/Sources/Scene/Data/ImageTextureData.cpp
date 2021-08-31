#include <Scene/Data/ImageTextureData.h>

namespace leo {
	ImageTextureData::ImageTextureData(size_t width, size_t height, Type type, Layout layout)
		: width(width), height(height), type(type), layout(layout), data(new float[width * height * static_cast<size_t>(layout)])
	{
	}

	ImageTextureData::~ImageTextureData()
	{
		delete[] data;
		data = nullptr;
	}
}
