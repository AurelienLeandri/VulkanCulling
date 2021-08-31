#include <Scene/Shapes/Sphere.h>

#include <Scene/Data/SphereData.h>
#include <Scene/Data/ShapesDataManager.h>

namespace leo {
	Sphere::Sphere()
		: Shape(Type::SPHERE), _data(ShapesDataManager::createSphereData())
	{
	}

	Sphere::Sphere(const glm::vec3 position, float radius)
		: Shape(Type::SPHERE), _data(ShapesDataManager::createSphereData(position, radius))
	{
	}

	Sphere::Sphere(const SphereData* data)
		: Shape(Type::SPHERE), _data(data)
	{
	}
}
