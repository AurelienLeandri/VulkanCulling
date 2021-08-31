#include <Scene/Data/ShapesDataManager.h>

#include <Scene/Data/MeshData.h>
#include <Scene/Data/SphereData.h>
#include <Scene/Data/TriangleData.h>

namespace leo {
	std::vector<std::unique_ptr<MeshData>> ShapesDataManager::_meshesPool;
	std::vector<TriangleData> ShapesDataManager::_trianglesPool;
	std::vector<SphereData> ShapesDataManager::_spheresPool;
}
