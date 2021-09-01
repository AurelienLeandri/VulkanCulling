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
        std::vector<std::shared_ptr<Texture>> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, std::vector<std::shared_ptr<Texture>>& textures, const std::string& directory);
        
        void processNode(
            aiNode* node,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& modelMaterials,
            std::vector<SceneObject>& sceneObjects);

        void processMesh(
            aiMesh* assimpMesh,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& model_materials,
            std::vector<SceneObject>& sceneObjects);

        std::shared_ptr<const ImageTexture> loadMaterialTexture(
            aiMaterial* assimpMaterial,
            aiTextureType assimpTextureType,
            const std::string& fileDirectoryPath);
    }

    const Model ModelLoader::loadModel(const char* filePath, LoadingOptions options)
    {
        auto cacheIterator = _modelsCache.find(filePath);
        if (cacheIterator != _modelsCache.end()) {
            Model model = cacheIterator->second;
            for (SceneObject& object : model.objects) {
                object.setTransform(std::make_shared<Transform>(options.globalTransform->getMatrix() * object.getTransform()->getMatrix()));
            }
            return model;
        }

        _modelsCache[filePath] = {};
        Model& model = _modelsCache[filePath];

        Assimp::Importer importer;
        const aiScene* aiScene = importer.ReadFile(filePath,
            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_SortByPType
        
        );

        if (!aiScene || aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aiScene->mRootNode) // if is Not Zero
        {
            return {};
        }

        std::unordered_map<aiMaterial*, std::shared_ptr<Material>> modelMaterials;
        std::string strFilePath = std::string(filePath);
        std::string fileDirectoryPath = strFilePath.substr(0, strFilePath.find_last_of('/'));
        processNode(aiScene->mRootNode, aiScene, fileDirectoryPath, modelMaterials, model.objects);

        for (SceneObject& object : model.objects) {
            object.setTransform(std::make_shared<Transform>(options.globalTransform->getMatrix() * object.getTransform()->getMatrix()));
        }

        return model;
    }

    namespace {
        void processNode(
            aiNode* node,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& modelMaterials,
            std::vector<SceneObject>& sceneObjects)
        {
            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh* mesh = aiScene->mMeshes[node->mMeshes[i]];
                processMesh(mesh, aiScene, fileDirectoryPath, modelMaterials, sceneObjects);
            }
            for (unsigned int i = 0; i < node->mNumChildren; i++)
            {
                processNode(node->mChildren[i], aiScene, fileDirectoryPath, modelMaterials, sceneObjects);
            }

        }

        void processMesh(
            aiMesh* assimpMesh,
            const aiScene* aiScene,
            const std::string& fileDirectoryPath,
            std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& modelMaterials,
            std::vector<SceneObject>& sceneObjects)
        {
            std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
            SceneObject sceneObject(mesh);

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
                sceneObject.setMaterial(modelMaterials[assimpMaterial]);
            }

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
            for (unsigned int i = 0; i < assimpMesh->mNumVertices; ++i) {
                vertices.push_back({
                        glm::vec3(assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z),  // Position
                        hasNormals ? glm::vec3(assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z) : glm::vec3(0, 0, 1),  // Normal or z+ by default
                        hasUv ? glm::vec2(assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y) : glm::vec2(0, 0)  // UVs if any
                    });
            }
    }

    namespace {
        std::shared_ptr<Material> loadMaterial(aiMaterial* assimpMaterial, const std::string& fileDirectoryPath)
        {
            // TODO: Check which material should be created using what values and textures are present.
            std::shared_ptr<PerformanceMaterial> material = std::make_shared<PerformanceMaterial>();
            static aiTextureType textureTypes[] = {
                aiTextureType_DIFFUSE,
                aiTextureType_SPECULAR,
                aiTextureType_AMBIENT,
                aiTextureType_EMISSIVE,
                aiTextureType_NORMALS,
                aiTextureType_LIGHTMAP,
                aiTextureType_OPACITY
            };
            std::unordered_map<aiTextureType, std::shared_ptr<ImageTexture>*> materialTextureSlots = {
                { aiTextureType_DIFFUSE, &material->diffuseTexture },
                { aiTextureType_SPECULAR, &material->specularTexture },
                { aiTextureType_AMBIENT, &material->ambientTexture },
                { aiTextureType_EMISSIVE, &material->emissiveTexture },
                { aiTextureType_NORMALS, &material->normalsTexture },
                { aiTextureType_LIGHTMAP, &material->occlusionTexture },
                { aiTextureType_OPACITY, &material->opacityTexture }
            };
            for (const aiTextureType& textureType : textureTypes) {
                std::shared_ptr<const ImageTexture> texture = loadMaterialTexture(assimpMaterial, textureType, fileDirectoryPath);
                if (texture) {
                    *materialTextureSlots[textureType] = texture;
                }
                // TODO: else case, for example adding a constant texture from another parameter of the material
            }

            return material;
        }

        std::shared_ptr<const ImageTexture> loadMaterialTexture(
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

            std::shared_ptr<const ImageTexture> texture = TextureLoader::loadTexture(texturePath.c_str());
            return texture;
        }

    }
}
