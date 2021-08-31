#pragma once

namespace leo {
	template <typename... Args>
	MeshData* ShapesDataManager::createMeshData(Args... args)
	{
		_meshesPool.push_back(std::make_unique<MeshData>(args));
		return _meshesPool.back().get();
	}

	template <typename... Args>
	TriangleData* ShapesDataManager::createTriangleData(Args&&... args)
	{
		_trianglesPool.emplace_back(std::forward<Args>(args)...);
		return &_trianglesPool.back();
	}

	template <typename... Args>
	SphereData* ShapesDataManager::createSphereData(Args&&... args)
	{
		_spheresPool.emplace_back(std::forward<Args>(args)...);
		return &_spheresPool.back();
	}
}
