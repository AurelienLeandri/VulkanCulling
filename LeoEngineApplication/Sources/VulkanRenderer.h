#pragma once

#include <memory>

namespace leo {
	class Scene;
}

class VulkanRenderer
{
public:
	struct Options {
	};

	VulkanRenderer(Options options = {});

	void setScene(std::shared_ptr<leo::Scene> scene);

private:
	std::shared_ptr<leo::Scene> _scene;
	Options _options;
};

