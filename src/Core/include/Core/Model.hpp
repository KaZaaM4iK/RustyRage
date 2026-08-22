#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

namespace Core {

/**
 * 3D Model loader and renderer using Assimp
 * Supports various 3D model formats
 */
class Model {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
        glm::vec3 color;
        
        Vertex() = default;
        Vertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& tex, const glm::vec3& col)
            : position(pos), normal(norm), texCoords(tex), color(col) {}
    };
    
    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::string materialName;
        uint32_t materialIndex;
        
        Mesh() = default;
        Mesh(const std::vector<Vertex>& verts, const std::vector<uint32_t>& inds, const std::string& matName, uint32_t matIndex)
            : vertices(verts), indices(inds), materialName(matName), materialIndex(matIndex) {}
    };
    
    struct Material {
        std::string name;
        glm::vec3 diffuseColor;
        glm::vec3 specularColor;
        glm::vec3 ambientColor;
        float shininess;
        std::string diffuseTexture;
        std::string normalTexture;
        std::string specularTexture;
        
        Material() : diffuseColor(1.0f), specularColor(1.0f), ambientColor(0.1f), shininess(32.0f) {}
    };

    Model();
    ~Model();

    // Loading
    bool loadFromFile(const std::string& filepath);
    void clear();
    
    // Getters
    const std::vector<Mesh>& getMeshes() const { return m_meshes; }
    const std::vector<Material>& getMaterials() const { return m_materials; }
    const std::string& getFilePath() const { return m_filePath; }
    
    // Bounding box
    const glm::vec3& getMinBounds() const { return m_minBounds; }
    const glm::vec3& getMaxBounds() const { return m_maxBounds; }
    glm::vec3 getCenter() const;
    glm::vec3 getSize() const;
    
    // Validation
    bool isValid() const { return !m_meshes.empty(); }
    size_t getVertexCount() const;
    size_t getIndexCount() const;
    
    // Transform
    void setTransform(const glm::mat4& transform) { m_transform = transform; }
    const glm::mat4& getTransform() const { return m_transform; }

private:
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
    std::string m_filePath;
    
    glm::vec3 m_minBounds;
    glm::vec3 m_maxBounds;
    glm::mat4 m_transform;
    
    // Assimp processing
    void processNode(void* node, const void* scene);
    Mesh processMesh(void* mesh, const void* scene);
    Material processMaterial(void* material);
    void calculateBounds();
    
    // Helper methods
    glm::vec3 aiVector3DToGlm(const void* aiVec) const;
    glm::vec2 aiVector2DToGlm(const void* aiVec) const;
    std::string getTexturePath(const void* material, int type) const;
};

} // namespace Core
