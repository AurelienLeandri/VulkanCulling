#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer(Options options) :
	_options(options)
{
}

void VulkanRenderer::setScene(std::shared_ptr<leo::Scene> scene)
{
	_scene = scene;
}
