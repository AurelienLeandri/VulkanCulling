#pragma once

#include <memory>
#include <vector>

namespace leo {
	struct MeshData;
	struct SphereData;
	struct TriangleData;

	class ShapesDataManager {
	public:
		template <typename... Args>
		static MeshData* createMeshData(Args... args);
		template <typename... Args>
		static TriangleData* createTriangleData(Args&&... args);
		template <typename... Args>
		static SphereData* createSphereData(Args&&... args);

	private:
		static std::vector<std::unique_ptr<MeshData>> _meshesPool;
		static std::vector<TriangleData> _trianglesPool;
		static std::vector<SphereData> _spheresPool;
	};
}

#include <Scene/Data/ShapesDataManager.hxx>
