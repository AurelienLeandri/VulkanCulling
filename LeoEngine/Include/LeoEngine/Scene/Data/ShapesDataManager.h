#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

namespace leo {
	struct MeshData;
	struct SphereData;
	struct TriangleData;

	class ShapesDataManager {
	public:
		template <typename... Args>
		MeshData* createMeshData(Args... args);
		template <typename... Args>
		TriangleData* createTriangleData(Args... args);
		template <typename... Args>
		SphereData* createSphereData(Args... args);

	public:
		~ShapesDataManager();
	private:
		std::vector<std::unique_ptr<MeshData>> _meshesPool;
		std::vector<TriangleData> _trianglesPool;
		std::vector<SphereData> _spheresPool;
	};
}

#include <Scene/Data/ShapesDataManager.hxx>
