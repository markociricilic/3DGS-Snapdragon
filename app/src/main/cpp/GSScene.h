#ifndef GSSCENE_H
#define GSSCENE_H

#include <filesystem>
#include <iostream>
#include <glm/glm.hpp>
#include "vulkan/VulkanContext.h"
#include "vulkan/Buffer.h"

struct PlyProperty {
    std::string type;
    std::string name;
};

struct PlyHeader {
    std::string format;
    int numVertices;
    int numFaces;
    std::vector<PlyProperty> vertexProperties;
    std::vector<PlyProperty> faceProperties;
};

class GSScene {
public:
    explicit GSScene(std::string & assetContent)
        : assetContent(assetContent) {}

    void load(const std::shared_ptr<VulkanContext>& context);

    void loadTestScene(const std::shared_ptr<VulkanContext>& context);

    void loadSmallScene(const std::shared_ptr<VulkanContext>&context);

    uint64_t getNumVertices() const {
        return header.numVertices;
    }

    struct Vertex {
        glm::vec4 position;
        glm::vec4 scale_opacity;
        glm::vec4 rotation;
        float shs[48];
    };

    void printVertex(const Vertex & v, bool print_shs = false);

    struct Cov3DUpperRight {
        float mat[6];
    };

    std::shared_ptr<Buffer> vertexBuffer;
    std::shared_ptr<Buffer> cov3DBuffer;
private:
    std::string assetContent;
    std::string poseContent;
    PlyHeader header;

    std::shared_ptr<Buffer> createStagingBuffer(const std::shared_ptr<VulkanContext>& sharedPtr, unsigned long i);

    int loadPlyHeader(std::istringstream & plyFile);

    static std::shared_ptr<Buffer> createBuffer(const std::shared_ptr<VulkanContext>& sharedPtr, size_t i);

    void precomputeCov3D(const std::shared_ptr<VulkanContext>& context);
};



#endif //GSSCENE_H