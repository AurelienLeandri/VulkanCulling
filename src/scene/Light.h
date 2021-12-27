#pragma once

#include "GeometryIncludes.h"

namespace leo {

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
