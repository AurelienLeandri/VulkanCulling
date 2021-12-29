#pragma once

#include "GeometryIncludes.h"

namespace leoscene {
    struct TransformParameters {
        glm::vec3 translation = glm::vec3(0);
        glm::vec3 rotation_rads = glm::vec3(0);
        glm::vec3 scaling = glm::vec3(1);
    };

    class Transform {
    public:
        Transform();
        Transform(const TransformParameters& params);
        Transform(const glm::vec3& translation, const glm::vec3& rotation_rads, const glm::vec3& scaling);
        Transform(const glm::mat4& matrix);
        Transform(const glm::mat4& matrix, const glm::mat4& invMatrix);
        Transform inverse() const;
        Transform transpose() const;
        bool swapsHandedness() const;
        const glm::mat4& getMatrix() const;
        const glm::mat4& getInvMatrix() const;
        Transform& operator *=(const Transform& other);

    private:
        glm::mat4 _matrix = glm::mat4(1);
        glm::mat4 _invMatrix = glm::mat4(1);
    };

    Transform operator*(Transform lhs, const Transform& rhs);
}
