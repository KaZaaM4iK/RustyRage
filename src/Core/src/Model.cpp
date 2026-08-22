#include "Core/Model.hpp"
#include "Core/Logging.hpp"
// #include <assimp/Importer.hpp>
// #include <assimp/scene.h>
// #include <assimp/postprocess.h>

namespace Core {

Model::Model() : m_minBounds(0.0f), m_maxBounds(0.0f), m_transform(1.0f) {
}

Model::~Model() {
    clear();
}

bool Model::loadFromFile(const std::string& filepath) {
    LOG_INFOF("Loading model from: %s", filepath.c_str());
    
    // TODO: Implement Assimp model loading when available
    // Assimp::Importer importer;
    // const aiScene* scene = importer.ReadFile(filepath, 
    //     aiProcess_Triangulate | 
    //     aiProcess_FlipUVs | 
    //     aiProcess_CalcTangentSpace |
    //     aiProcess_GenNormals);
    
    m_filePath = filepath;
    m_meshes.clear();
    m_materials.clear();
    
    // Create a simple default mesh for now
    Mesh defaultMesh;
    defaultMesh.vertices = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}
    };
    defaultMesh.indices = {0, 1, 2, 2, 3, 0};
    defaultMesh.materialName = "default";
    defaultMesh.materialIndex = 0;
    m_meshes.push_back(defaultMesh);
    
    // Create default material
    Material defaultMaterial;
    defaultMaterial.name = "default";
    m_materials.push_back(defaultMaterial);
    
    // Calculate bounding box
    calculateBounds();
    
    LOG_INFOF("Model loaded successfully: %zu meshes, %zu materials", 
              m_meshes.size(), m_materials.size());
    
    return true;
}

void Model::clear() {
    m_meshes.clear();
    m_materials.clear();
    m_filePath.clear();
    m_minBounds = glm::vec3(0.0f);
    m_maxBounds = glm::vec3(0.0f);
    m_transform = glm::mat4(1.0f);
}

glm::vec3 Model::getCenter() const {
    return (m_minBounds + m_maxBounds) * 0.5f;
}

glm::vec3 Model::getSize() const {
    return m_maxBounds - m_minBounds;
}

size_t Model::getVertexCount() const {
    size_t count = 0;
    for (const auto& mesh : m_meshes) {
        count += mesh.vertices.size();
    }
    return count;
}

size_t Model::getIndexCount() const {
    size_t count = 0;
    for (const auto& mesh : m_meshes) {
        count += mesh.indices.size();
    }
    return count;
}

void Model::processNode(void* node, const void* scene) {
    // TODO: Implement Assimp node processing when available
    // aiNode* aiNode = static_cast<aiNode*>(node);
    // const aiScene* aiScene = static_cast<const aiScene*>(scene);
}

Model::Mesh Model::processMesh(void* mesh, const void* scene) {
    // TODO: Implement Assimp mesh processing when available
    // aiMesh* aiMesh = static_cast<aiMesh*>(mesh);
    // const aiScene* aiScene = static_cast<const aiScene*>(scene);
    
    Mesh defaultMesh;
    return defaultMesh;
}

Model::Material Model::processMaterial(void* material) {
    // TODO: Implement Assimp material processing when available
    // aiMaterial* aiMaterial = static_cast<aiMaterial*>(material);
    Material mat;
    return mat;
}

void Model::calculateBounds() {
    if (m_meshes.empty()) {
        m_minBounds = glm::vec3(0.0f);
        m_maxBounds = glm::vec3(0.0f);
        return;
    }
    
    m_minBounds = glm::vec3(FLT_MAX);
    m_maxBounds = glm::vec3(-FLT_MAX);
    
    for (const auto& mesh : m_meshes) {
        for (const auto& vertex : mesh.vertices) {
            m_minBounds = glm::min(m_minBounds, vertex.position);
            m_maxBounds = glm::max(m_maxBounds, vertex.position);
        }
    }
}

glm::vec3 Model::aiVector3DToGlm(const void* aiVec) const {
    // TODO: Implement when Assimp is available
    return glm::vec3(0.0f);
}

glm::vec2 Model::aiVector2DToGlm(const void* aiVec) const {
    // TODO: Implement when Assimp is available
    return glm::vec2(0.0f);
}

std::string Model::getTexturePath(const void* material, int type) const {
    // TODO: Implement when Assimp is available
    return "";
}

} // namespace Core
