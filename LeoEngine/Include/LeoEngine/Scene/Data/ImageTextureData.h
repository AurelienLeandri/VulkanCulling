#pragma once

namespace leo {
	struct ImageTextureData {
        enum class Type {
            FLOAT
        };
        enum class Layout : size_t {
            RGB = 3,
            RGBA = 4,
            LUMINANCE = 1
        };

        ImageTextureData(size_t width, size_t height, Type type, Layout layout);
        ~ImageTextureData();

        const size_t width = 0;
        const size_t height = 0;
        const Type type;
        const Layout layout;
        float* data;
    };
}

