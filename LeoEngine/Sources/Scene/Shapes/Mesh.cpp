#include <Scene/Shapes/Mesh.h>

#include <Scene/Data/MeshData.h>
#include <Scene/Data/ShapesDataManager.h>

namespace leo {
	Mesh::Mesh(const MeshData* data)
		: Shape(Type::MESH), _data(data)
	{
	}
}
