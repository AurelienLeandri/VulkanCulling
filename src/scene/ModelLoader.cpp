#pragma once

#include "ModelLoader.h"

#include "TextureLoader.h"
#include "Texture.h"
#include "Mesh.h"
#include "PerformanceMaterial.h"
#include "Scene/SceneObject.h"
#include "Scene/Transform.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace leoscene {
    namespace {
        void processNode(
            aiNode* node,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& modelMaterials,
            std::unordered_map<aiMesh*, std::shared_ptr<Mesh>>& modelMeshes,
            std::vector<SceneObject>& sceneObjects,
            aiMatrix4x4 transform);

        void processMesh(
            aiMesh* assimpMesh,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& model_materials,
            std::unordered_map<aiMesh*, std::shared_ptr<Mesh>>& modelMeshes,
            std::vector<SceneObject>& sceneObjects,
            aiMatrix4x4 transform);

        std::shared_ptr<Material> loadMaterial(aiMaterial* assimpMaterial, const std::string& fileDirectoryPath);

        std::shared_ptr<ImageTexture> loadMaterialTexture(
            aiMaterial* assimpMaterial,
            aiTextureType assimpTextureType,
            const std::string& fileDirectoryPath);
    }

    std::unordered_map<std::string, Model> ModelLoader::_modelsCache;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, Model>> ModelLoader::_spheresCache;
    const std::shared_ptr<Material> ModelLoader::_defaultMaterial = std::make_shared<PerformanceMaterial>();


    const Model ModelLoader::loadModel(const char* filePath, LoadingOptions options)
    {
        auto cacheIterator = _modelsCache.find(filePath);
        if (cacheIterator != _modelsCache.end()) {
            Model model = cacheIterator->second;
            for (SceneObject& object : model.objects) {
                object.transform = std::make_shared<Transform>(options.globalTransform->getMatrix() * object.transform->getMatrix());
            }
            return model;
        }

        _modelsCache[filePath] = {};
        Model& model = _modelsCache[filePath];

        Assimp::Importer importer;
        const aiScene* aiScene = importer.ReadFile(filePath,
            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_SortByPType

        );

        if (!aiScene || aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aiScene->mRootNode) // if is Not Zero
        {
            return {};
        }

        std::unordered_map<aiMaterial*, std::shared_ptr<Material>> modelMaterials;
        std::unordered_map<aiMesh*, std::shared_ptr<Mesh>> modelMeshes;
        std::string strFilePath = std::string(filePath);
        std::string fileDirectoryPath = strFilePath.substr(0, strFilePath.find_last_of('/'));
        aiMatrix4x4 transform;
        processNode(aiScene->mRootNode, aiScene, fileDirectoryPath, modelMaterials, modelMeshes, model.objects, transform);

        if (options.globalTransform) {
            for (SceneObject& object : model.objects) {
                if (object.transform) {
                    object.transform = std::make_shared<Transform>(options.globalTransform->getMatrix() * object.transform->getMatrix());
                }
            }
        }

        return model;
    }

    const Model ModelLoader::loadSphereModel(uint32_t xSegments, uint32_t ySegments, LoadingOptions options)
    {
        auto xSegmentFind = _spheresCache.find(xSegments);
        if (xSegmentFind != _spheresCache.end()) {
            auto ySegmentFind = xSegmentFind->second.find(ySegments);
            if (ySegmentFind != xSegmentFind->second.end()) {
                Model model = ySegmentFind->second;
                return model;
            }
        }

        _spheresCache[xSegments] = {};
        _spheresCache[xSegments][ySegments] = {};
        Model& model = _spheresCache[xSegments][ySegments];

        model.objects.emplace_back();
        SceneObject& object = model.objects.back();

        std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
        mesh->boundingSphere = glm::vec4(0, 0, 0, 1);
        object.shape = mesh;

        object.transform = options.globalTransform;

        object.material = _defaultMaterial;
        
        static const float PI = 3.14159265359f;
        for (unsigned int y = 0; y <= ySegments; ++y)
        {
            for (unsigned int x = 0; x <= xSegments; ++x)
            {
                float xSegment = static_cast<float>(x) / xSegments;
                float ySegment = static_cast<float>(y) / ySegments;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                glm::vec3 p(xPos, yPos, zPos);
                glm::vec2 uv(xSegment, ySegment);
                glm::vec3 n = glm::normalize(p);
                glm::vec3 t(0, 0, 1);  // TODO: Compute tangents. Not needed for now.

                mesh->vertices.push_back({p, n, t, uv});
            }
        }

        bool oddRow = false;
        for (uint32_t y = 0; y < ySegments; ++y)
        {
            if (!oddRow) // even rows: y == 0, y == 2; and so on
            {
                for (uint32_t x = 0; x <= xSegments; ++x)
                {
                    mesh->indices.push_back(y * (xSegments + 1) + x);
                    mesh->indices.push_back((y + 1) * (xSegments + 1) + x);
                    mesh->indices.push_back(y * (xSegments + 1) + ((x + 1) % (xSegments + 1)));
                    mesh->indices.push_back(y * (xSegments + 1) + ((x + 1) % (xSegments + 1)));
                    mesh->indices.push_back((y + 1) * (xSegments + 1) + x);
                    mesh->indices.push_back((y + 1) * (xSegments + 1) + ((x + 1) % (xSegments + 1)));
                }
            }
            else
            {
                for (int x = xSegments; x >= 0; --x)
                {
                    mesh->indices.push_back(y * (xSegments + 1) + x);
                    mesh->indices.push_back((y + 1) * (xSegments + 1) + x);
                    mesh->indices.push_back(y * (xSegments + 1) + (x - 1 < 0 ? xSegments : x - 1));
                    mesh->indices.push_back(y * (xSegments + 1) + (x - 1 < 0 ? xSegments : x - 1));
                    mesh->indices.push_back((y + 1) * (xSegments + 1) + x);
                    mesh->indices.push_back((y + 1) * (xSegments + 1) + (x - 1 < 0 ? xSegments : x - 1));
                }
            }
            oddRow = !oddRow;
        }

        return model;
    }

    namespace {
        void processNode(
            aiNode* node,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& modelMaterials,
            std::unordered_map<aiMesh*, std::shared_ptr<Mesh>>& modelMeshes,
            std::vector<SceneObject>& sceneObjects,
            aiMatrix4x4 transform)
        {
            aiMatrix4x4 childTransform = node->mTransformation * transform;
            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh* mesh = aiScene->mMeshes[node->mMeshes[i]];
                processMesh(mesh, aiScene, fileDirectoryPath, modelMaterials, modelMeshes, sceneObjects, childTransform);
            }
            for (unsigned int i = 0; i < node->mNumChildren; i++)
            {
                processNode(node->mChildren[i], aiScene, fileDirectoryPath, modelMaterials, modelMeshes, sceneObjects, childTransform);
            }

        }

        void processMesh(
            aiMesh* assimpMesh,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& modelMaterials,
            std::unordered_map<aiMesh*, std::shared_ptr<Mesh>>& modelMeshes,
            std::vector<SceneObject>& sceneObjects,
            aiMatrix4x4 transform)
        {
            sceneObjects.push_back({});
            SceneObject& sceneObject = sceneObjects.back();

            if (modelMeshes.find(assimpMesh) != modelMeshes.end()) {
                sceneObject.shape = modelMeshes[assimpMesh];
            }
            else {
                std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
                sceneObject.shape = mesh;

                // Populate indices vector
                std::vector<uint32_t>& indices = mesh->indices;
                for (unsigned int i = 0; i < assimpMesh->mNumFaces; ++i)
                {
                    const aiFace& face = assimpMesh->mFaces[i];
                    for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                        indices.push_back(face.mIndices[j]);
                    }
                }

                // Populate vertices vector
                std::vector<Vertex>& vertices = mesh->vertices;
                bool hasUv = assimpMesh->mTextureCoords[0];
                bool hasNormals = assimpMesh->HasNormals();
                bool hasTangents = assimpMesh->HasTangentsAndBitangents();
                glm::vec3 minV(assimpMesh->mVertices[0].x, assimpMesh->mVertices[0].y, assimpMesh->mVertices[0].z);
                glm::vec3 maxV(assimpMesh->mVertices[0].x, assimpMesh->mVertices[0].y, assimpMesh->mVertices[0].z);
                for (unsigned int i = 0; i < assimpMesh->mNumVertices; ++i) {
                    const aiVector3D& v = assimpMesh->mVertices[i];
                    for (int k = 0; k < 3; ++k) {
                        if (v[k] < minV[k]) { minV[k] = v[k]; }
                        if (v[k] > maxV[k]) { maxV[k] = v[k]; }
                    }
                    vertices.push_back({
                            glm::vec3(v.x, v.y, v.z),  // Position
                            hasNormals ? glm::vec3(assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z) : glm::vec3(0, 0, 1),  // Normal or z+ by default
                            hasTangents ? glm::vec3(assimpMesh->mTangents[i].x, assimpMesh->mTangents[i].y, assimpMesh->mTangents[i].z) : glm::vec3(1, 0, 0),  // Tangents or x+ by default
                            hasUv ? glm::vec2(assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y) : glm::vec2(0, 0)  // UVs if any
                        });
                }
                glm::vec3 halfway = (maxV - minV) / 2.f;
                glm::vec3 center = minV + halfway;
                float radius = glm::length(halfway) * 2.0f;
                mesh->boundingSphere = glm::vec4(center, radius);
            }

            if (!transform.IsIdentity()) {
                glm::mat4 glmTransform;
                for (int i = 0; i < 16; ++i) {
                    glmTransform[i % 4][i / 4] = *transform[i];
                }
                sceneObject.transform = std::make_shared<Transform>(glmTransform);
            }

            // Process material
            aiMaterial* assimpMaterial = aiScene->mMaterials[assimpMesh->mMaterialIndex];

            // There is a material attached to the mesh
            if (assimpMaterial) {
                if (modelMaterials.find(assimpMaterial) == modelMaterials.end()) {  // First time seeing the material
                    // Creating the material
                    std::shared_ptr<Material> material = loadMaterial(assimpMaterial, fileDirectoryPath);

                    // Add the material to the Scene and also the cache to avoid duplicates.
                    modelMaterials[assimpMaterial] = material;
                }
                sceneObject.material = modelMaterials[assimpMaterial];
            }
        }

        std::shared_ptr<Material> loadMaterial(aiMaterial* assimpMaterial, const std::string& fileDirectoryPath)
        {
            // TODO: Check which material should be created using what values and textures are present.
            std::shared_ptr<PerformanceMaterial> material = std::make_shared<PerformanceMaterial>();
            static aiTextureType textureTypes[] = {
                aiTextureType_DIFFUSE,
                aiTextureType_SPECULAR,
                aiTextureType_AMBIENT,
                aiTextureType_NORMALS,
                aiTextureType_HEIGHT,
            };
            std::unordered_map<aiTextureType, std::shared_ptr<const ImageTexture>*> materialTextureSlots = {
                { aiTextureType_DIFFUSE, &material->diffuseTexture },
                { aiTextureType_SPECULAR, &material->specularTexture },
                { aiTextureType_AMBIENT, &material->ambientTexture },
                { aiTextureType_NORMALS, &material->normalsTexture },
                { aiTextureType_HEIGHT, &material->heightTexture },
            };
            for (const aiTextureType& textureType : textureTypes) {
                std::shared_ptr<ImageTexture> texture = loadMaterialTexture(assimpMaterial, textureType, fileDirectoryPath);
                if (texture) {
                    *materialTextureSlots[textureType] = texture;
                }
            }

            return material;
        }

        std::shared_ptr<ImageTexture> loadMaterialTexture(
            aiMaterial* assimpMaterial,
            aiTextureType assimpTextureType,
            const std::string& fileDirectoryPath)
        {
            if (!assimpMaterial->GetTextureCount(assimpTextureType)) {
                return nullptr;
            }

            aiString str;
            assimpMaterial->GetTexture(assimpTextureType, 0, &str);  // "0" for texture at index 0. The rest is ignored because unsupported by all renderers.
            std::string texturePath = fileDirectoryPath + "/" + str.C_Str();

            TextureLoader::LoadingOptions loadingOptions = {};
            if (assimpTextureType == aiTextureType_DIFFUSE ||
                assimpTextureType == aiTextureType_SPECULAR ||
                assimpTextureType == aiTextureType_AMBIENT ||
                assimpTextureType == aiTextureType_NORMALS)
            {
                loadingOptions.desiredChannels = 4;  // Vulkan implementation in Nvidia apparently rarely supports RGB.
            }
            else if (assimpTextureType == aiTextureType_HEIGHT)
            {
                loadingOptions.desiredChannels = 1;  // Vulkan implementation in Nvidia apparently rarely supports RGB.
            }

            std::shared_ptr<ImageTexture> texture = TextureLoader::loadTexture(texturePath.c_str(), loadingOptions);
            return texture;
        }

    }
}
