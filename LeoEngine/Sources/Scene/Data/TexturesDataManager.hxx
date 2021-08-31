#pragma once

namespace leo {
	template <typename... Args>
	static ImageTextureData* TexturesDataManager::createImageTextureData(Args... args) {
		_imageTexturesPool.push_back(std::make_unique<ImageTextureData>(args));
	}
}
