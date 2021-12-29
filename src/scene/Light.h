#pragma once

#include "GeometryIncludes.h"

namespace leoscene {

    class Light
    {
    public:
        enum class Type {
            DIRECTIONAL,
            POINT
        };

    public:
        virtual Type getType() const = 0;
    };
}
