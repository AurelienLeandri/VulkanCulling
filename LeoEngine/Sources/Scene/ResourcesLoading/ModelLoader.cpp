#pragma once

#include <Scene/ResourcesLoading/ModelLoader.h>

#include <Scene/ResourcesLoading/TextureLoader.h>
#include <Scene/Textures/Texture.h>
#include <Scene/Geometries/Mesh.h>
#include <Scene/Materials/PerformanceMaterial.h>
#include <Scene/SceneObject.h>
#include <Scene/Transform.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace leo {
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
        processNode(aiScene->mRootNode, aiScene, fileDirectoryPath, modelMaterials, modelMeshes, model.objects, aiMatrix4x4());

        if (options.globalTransform) {
            for (SceneObject& object : model.objects) {
                if (object.transform) {
                    object.transform = std::make_shared<Transform>(options.globalTransform->getMatrix() * object.transform->getMatrix());
                }
            }
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
                float radius = glm::length(halfway);
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
