#include <Scene/Textures/ConstantTexture.h>

leo::ConstantTexture::ConstantTexture(const glm::vec4& color)
	: Texture(Type::CONSTANT), color(color)
{
}

glm::vec4 leo::ConstantTexture::getTexel(float u, float v) const
{
	return color;
}
