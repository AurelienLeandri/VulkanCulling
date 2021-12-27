#include "Transform.h"

namespace leo {

	Transform::Transform()
	{
	}

	Transform::Transform(const TransformParameters& params)
		: Transform(params.translation, params.rotation_rads, params.scaling)
	{
	}

	Transform::Transform(const glm::vec3& translation, const glm::vec3& rotation_rads, const glm::vec3& scaling)
	{
		// NOTE: glm::mat4 are column-major. The first index is the column number.

		glm::mat4 translationMatrix(1.f);
		glm::mat4 scalingMatrix(1.f);

		for (int i = 0; i < 3; ++i) {
			scalingMatrix[i][i] = scaling[i];
			translationMatrix[3][i] = translation[i];
		}

		float sinX = glm::sin(rotation_rads.x);
		float cosX = glm::cos(rotation_rads.x);
		float sinY = glm::sin(rotation_rads.y);
		float cosY = glm::cos(rotation_rads.y);
		float sinZ = glm::sin(rotation_rads.z);
		float cosZ = glm::cos(rotation_rads.z);

		glm::mat4 rotationX(1.f);
		rotationX[1][1] = cosX;
		rotationX[2][1] = -sinX;
		rotationX[1][2] = sinX;
		rotationX[2][2] = cosX;

		glm::mat4 rotationY(1.f);
		rotationY[0][0] = cosY;
		rotationY[2][0] = sinY;
		rotationY[0][2] = -sinY;
		rotationY[2][2] = cosY;

		glm::mat4 rotationZ(1.f);
		rotationZ[0][0] = cosZ;
		rotationZ[1][0] = -sinZ;
		rotationZ[0][1] = sinZ;
		rotationZ[1][1] = cosZ;

		_matrix *= translationMatrix * rotationZ * rotationY * rotationX * scalingMatrix;
		_invMatrix = glm::inverse(_matrix);
	}

	Transform::Transform(const glm::mat4& matrix)
		: _matrix(matrix), _invMatrix(glm::inverse(matrix))
	{
	}

	Transform::Transform(const glm::mat4& matrix, const glm::mat4& invMatrix)
		: _matrix(matrix), _invMatrix(invMatrix)
	{
	}

	Transform Transform::inverse() const
	{
		return Transform(_invMatrix, _matrix);
	}

	Transform Transform::transpose() const
	{
		return Transform(glm::transpose(_matrix), glm::transpose(_invMatrix));
	}

	bool Transform::swapsHandedness() const
	{
		return glm::determinant(_matrix) < 0;
	}

	const glm::mat4& Transform::getMatrix() const
	{
		return _matrix;
	}

	const glm::mat4& Transform::getInvMatrix() const
	{
		return _invMatrix;
	}

	Transform& Transform::operator*=(const Transform& other)
	{
		_matrix *= other._matrix;
		_invMatrix = glm::inverse(_matrix);
		return *this;
	}

	Transform operator*(Transform lhs, const Transform& rhs)
	{
		return lhs *= rhs;
	}

}
