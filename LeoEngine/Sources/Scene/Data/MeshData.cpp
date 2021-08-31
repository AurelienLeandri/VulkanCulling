#include <Scene/Data/MeshData.h>

namespace leo {
	MeshData::MeshData(size_t nbVertices, size_t nbIndices)
		: vertices(nbVertices), indices(nbIndices)
	{
	}
}
