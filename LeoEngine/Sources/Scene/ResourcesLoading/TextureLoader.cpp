#include <Scene/ResourcesLoading/TextureLoader.h>

#include <Scene/Textures/ImageTexture.h>

#include <stb_image.h>

namespace leo {
    namespace {
        ImageTexture::Layout pickLayout(TextureLoader::LoadingOptions options, int nbChannels);
        bool isImageInfoValid(ImageTexture::Layout& layout, int nbChannels);
        void rgbToLuminance(const float* src, float* dest, int width, int height, int srcNbCHannels);
    }

    std::shared_ptr<const ImageTexture> TextureLoader::loadTexture(const char* fileName, TextureLoader::LoadingOptions options)
    {
        auto cacheIterator = _fileTexturesCache.find(fileName);
        if (cacheIterator != _fileTexturesCache.end()) {
            return cacheIterator->second;
        }

        stbi_set_flip_vertically_on_load(false);
        int width = 0, height = 0, nbChannels = 0;
        float* data = stbi_loadf(fileName, &width, &height, &nbChannels, 0);
        if (!data) {
            return nullptr;
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

        std::shared_ptr<ImageTexture> newTexture = std::make_shared<ImageTexture>(
            static_cast<size_t>(width),
            static_cast<size_t>(height),
            ImageTexture::Type::FLOAT,
            layout,
            data);

        return newTexture;
    }

    namespace {
        ImageTexture::Layout pickLayout(TextureLoader::LoadingOptions options, int nbChannels) {
            if (options.forceLayout != ImageTexture::Layout::INVALID) {
                return options.forceLayout;
            }
            else {
                switch (nbChannels) {
                case 1:
                    return ImageTexture::Layout::A;
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
            case ImageTexture::Layout::INVALID:
                return false;
            case ImageTexture::Layout::RGB:
                return nbChannels == 3;
            case ImageTexture::Layout::RGBA:
                return nbChannels == 4;
            case ImageTexture::Layout::A:
                return nbChannels == 1;
            case ImageTexture::Layout::LUMINANCE:
                return nbChannels == 1 || nbChannels == 3;
            }
        }

        void rgbToLuminance(const float* src, float* dest, int width, int height, int srcNbCHannels) {
            for (int i = 0; i < width * height * srcNbCHannels; i += srcNbCHannels) {
                dest[i / 3] = src[i] * 0.3f + src[i + 1] * 0.59f + src[i + 2] * 0.11f;
            }
        }
    }

}
