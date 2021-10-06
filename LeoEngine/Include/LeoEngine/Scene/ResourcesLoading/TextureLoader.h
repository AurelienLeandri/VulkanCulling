#pragma once

#include <Scene/Textures/ImageTexture.h>

#include <unordered_map>
#include <memory>

namespace leo {
	class TextureLoader {
	public:
		struct LoadingOptions {
			ImageTexture::Layout forceLayout = ImageTexture::Layout::INVALID;
			int desiredChannels = 0;
		};

	public:
		static std::shared_ptr<ImageTexture> loadTexture(const char* filePath, TextureLoader::LoadingOptions options = {});

	private:
		static std::unordered_map<std::string, std::shared_ptr<ImageTexture>> _fileTexturesCache;
	};
}
