#pragma once

#include <unordered_map>
#include <memory>

namespace leo {
	class ImageTexture;

	class TextureLoader {
	public:
		static std::shared_ptr<const ImageTexture> loadTexture(const char* fileName);

	private:
		static std::unordered_map<std::string, std::shared_ptr<ImageTexture>> _fileTexturesCache;
	};
}
