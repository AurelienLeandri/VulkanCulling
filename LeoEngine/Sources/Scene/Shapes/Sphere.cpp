#include <Scene/Shapes/Sphere.h>

#include <Scene/Data/SphereData.h>
#include <Scene/Data/ShapesDataManager.h>

namespace leo {
	Sphere::Sphere()
		: _data(ShapesDataManager::createSphereData())
	{
	}

	Sphere::Sphere(const glm::vec3 position, float radius)
		: _data(ShapesDataManager::createSphereData(position, radius))
	{
	}

	Sphere::Sphere(const SphereData* data)
		: _data(data)
	{
	}
}
