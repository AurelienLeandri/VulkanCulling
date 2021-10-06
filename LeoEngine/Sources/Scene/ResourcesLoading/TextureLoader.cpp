#include <Scene/ResourcesLoading/TextureLoader.h>

#include <Scene/Textures/ImageTexture.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace leo {
    namespace {
        ImageTexture::Layout pickLayout(TextureLoader::LoadingOptions options, int nbChannels);
        bool isImageInfoValid(ImageTexture::Layout& layout, int nbChannels);
        void rgbToLuminance(const float* src, float* dest, int width, int height, int srcNbCHannels);
    }

    std::unordered_map<std::string, std::shared_ptr<ImageTexture>> TextureLoader::_fileTexturesCache;

    std::shared_ptr<ImageTexture> TextureLoader::loadTexture(const char* filePath, TextureLoader::LoadingOptions options)
    {
        auto cacheIterator = _fileTexturesCache.find(filePath);
        if (cacheIterator != _fileTexturesCache.end()) {
            return cacheIterator->second;
        }

        stbi_set_flip_vertically_on_load(false);
        int width = 0, height = 0, nbChannels = 0;
        float* data = stbi_loadf(filePath, &width, &height, &nbChannels, options.desiredChannels);

        if (!data) {
            return nullptr;
        }

        if (options.desiredChannels && nbChannels != options.desiredChannels) {
            nbChannels = options.desiredChannels;
        }

        ImageTexture::Layout layout = pickLayout(options, nbChannels);
        if (!isImageInfoValid(layout, nbChannels)) {
            stbi_image_free(data);
            return nullptr;
        }

        if (layout == ImageTexture::Layout::LUMINANCE) {
            if (nbChannels == 3) {  // Convert the first three channels of each texel to luminance
                float* luminanceData = new float[(size_t)width * height];
                rgbToLuminance(data, luminanceData, width, height, nbChannels);
                stbi_image_free(data);
                data = luminanceData;
                nbChannels = 1;
            }
        }

        _fileTexturesCache[filePath] = std::make_shared<ImageTexture>(
            static_cast<size_t>(width),
            static_cast<size_t>(height),
            ImageTexture::Type::FLOAT,
            layout,
            data);

        return _fileTexturesCache[filePath];
    }

    namespace {
        ImageTexture::Layout pickLayout(TextureLoader::LoadingOptions options, int nbChannels) {
            if (options.forceLayout != ImageTexture::Layout::INVALID) {
                return options.forceLayout;
            }
            else {
                switch (nbChannels) {
                case 1:
                    return ImageTexture::Layout::R;
                case 3:
                    return ImageTexture::Layout::RGB;
                case 4:
                    return ImageTexture::Layout::RGBA;
                default:
                    return ImageTexture::Layout::INVALID;
                }
            }
        }

        bool isImageInfoValid(ImageTexture::Layout& layout, int nbChannels) {
            switch (layout) {
            case ImageTexture::Layout::RGB:
                return nbChannels == 3;
            case ImageTexture::Layout::RGBA:
                return nbChannels == 4;
            case ImageTexture::Layout::R:
                return nbChannels == 1;
            case ImageTexture::Layout::LUMINANCE:
                return nbChannels == 1 || nbChannels == 3;
            default:
                return false;
            }
        }

        void rgbToLuminance(const float* src, float* dest, int width, int height, int srcNbCHannels) {
            for (int i = 0; i < width * height * srcNbCHannels; i += srcNbCHannels) {
                dest[i / 3] = src[i] * 0.3f + src[i + 1] * 0.59f + src[i + 2] * 0.11f;
            }
        }
    }

}
