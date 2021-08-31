#include <Scene/Data/TexturesDataManager.h>

#include <Scene/Data/ImageTextureData.h>

namespace leo {
	std::vector<std::unique_ptr<ImageTextureData>> TexturesDataManager::_imageTexturesPool;
}
