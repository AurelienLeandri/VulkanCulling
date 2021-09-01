#pragma once

#include <GeometryIncludes.h>

namespace leo {

    class Transform;
    struct HitRecord;
    class Shape;
    class ImageTexture;
    class Distribution2D;

    class Light
    {
    public:
        enum class Type {
            AREA,
            INFINITE_AREA
        };

    public:
        virtual Type getType() const = 0;
    };
}
