#pragma once

#include <Scene/Textures/Texture.h>

namespace leo {
	class ConstantTexture : public Texture
	{
    public:
        ConstantTexture(const glm::vec4& color = glm::vec4(1));

    public:
        virtual glm::vec4 getTexel(float u, float v) const override;

    public:
        glm::vec4 color;
	};
}
