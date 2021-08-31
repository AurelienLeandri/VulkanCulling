#pragma once

#include <memory>
#include <vector>

namespace leo {
	struct ImageTextureData;

	class TexturesDataManager {
	public:
		template <typename... Args>
		static ImageTextureData* createImageTextureData(Args... args);

	private:
		static std::vector<std::unique_ptr<ImageTextureData>> _imageTexturesPool;
	};
}

#include <Scene/Data/TexturesDataManager.hxx>
