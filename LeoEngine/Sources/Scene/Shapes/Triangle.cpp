#include <Scene/Shapes/Triangle.h>

#include <Scene/Data/Vertex.h>
#include <Scene/Data/TriangleData.h>
#include <Scene/Data/ShapesDataManager.h>

namespace leo {
	Triangle::Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2)
		: Shape(Type::TRIANGLE), _data(ShapesDataManager::createTriangleData(v0, v1, v2))
	{
	}

	Triangle::Triangle(const TriangleData* data)
		: Shape(Type::TRIANGLE), _data(data)
	{
	}
}
