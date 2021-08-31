#pragma once

#include <unordered_map>

namespace leo {
	class ImageTextureData;
	class ImageTexture;
	class TexturesLoader {
	private:
		static std::unordered_map<std::string, const ImageTextureData*> _fileTexturesCache;
	};
}
