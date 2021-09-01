#include <Scene/Materials/PerformanceMaterial.h>

namespace leo {
    Material::Type PerformanceMaterial::getType() const
    {
        return Type::PERFORMANCE;
    }
}
