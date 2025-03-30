//
// Created by steven on 11/30/23.
//

#include <fstream>
#include "GSScene.h"

#include <random>
#include <variant>
#include "shaders.h"

#include "vulkan/Utils.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/pipelines/ComputePipeline.h"
#include "spdlog/spdlog.h"
#include "vulkan/Shader.h"

#include "base_utils.h"

struct VertexStorage {
    glm::vec3 position;
    glm::vec3 normal;
    float shs[48];
    float opacity;
    glm::vec3 scale;
    glm::vec4 rotation;
};

struct VertexStorage2 {
    glm::vec3 position;
    glm::vec3 scale;
    float opacity;
    glm::vec4 rotation;
    float shs[48];
};

void GSScene::printVertex(const GSScene::Vertex &v, bool print_shs) {
    LOGO("position: (%f, %f, %f, %f) \nrotation: (%f, %f, %f, %f) \nscale opacity: (%f, %f, %f, %f)\n",
         v.position[0], v.position[1], v.position[2],
         v.rotation[0], v.rotation[1], v.rotation[2],
         v.scale_opacity[0], v.scale_opacity[1], v.scale_opacity[2]
    );
    if(print_shs) {
        std::stringstream shs_stream;
        for (auto shs: v.shs) {
            shs_stream << shs << " ";
        }
        std::string str = shs_stream.str();
        char * cstr = new char[str.size() + 1]; // Allocate memory
        std::strcpy(cstr, str.c_str());
        LOGO("shs: %s", cstr);
    }
}

void readVertexInto(std::istringstream * plyFile, GSScene::Vertex * vertex, int vertexType){
//    static_assert(sizeof(VertexStorage) == 62 * sizeof(float));

    std::variant<VertexStorage, VertexStorage2> vertexStorage;

    if (vertexType == 1) {
        vertexStorage = VertexStorage{};
    } else if (vertexType == 2) {
        vertexStorage = VertexStorage2{};
    } else {
        LOGD("Unknown vertex type");
    }

    std::visit([&](auto& storage) {
        plyFile->read(reinterpret_cast<char *>(&vertexStorage), sizeof(storage));
        vertex->position = glm::vec4(storage.position, 1.0f);

        // verteces[i].normal = glm::vec4(vertexStorage.normal, 0.0f);
        vertex->scale_opacity = glm::vec4(glm::exp(storage.scale), 1.0f / (1.0f + std::exp(-storage.opacity)));
        vertex->rotation = normalize(storage.rotation);
        // memcpy(verteces[i].shs, vertexStorage.shs, 48 * sizeof(float));
        vertex->shs[0] = storage.shs[0];
        vertex->shs[1] = storage.shs[1];
        vertex->shs[2] = storage.shs[2];
        auto SH_N = 16;
        for (auto j = 1; j < SH_N; j++) {
            vertex->shs[j * 3 + 0] = storage.shs[(j - 1) + 3];
            vertex->shs[j * 3 + 1] = storage.shs[(j - 1) + SH_N + 2];
            vertex->shs[j * 3 + 2] = storage.shs[(j - 1) + SH_N * 2 + 1];
        }
    }, vertexStorage);

    // why is this important?
//    assert(vertexStorage.normal.x == 0.0f);
//    assert(vertexStorage.normal.y == 0.0f);
//    assert(vertexStorage.normal.z == 0.0f);
}

void GSScene::loadSmallScene(const std::shared_ptr<VulkanContext>&context){
    std::istringstream plyFile(assetContent, std::ios::binary);

    int vertexType = loadPlyHeader(plyFile);

     header.numVertices = 2;

    auto vertexStagingBuffer = Buffer::staging(context, header.numVertices * sizeof(Vertex));
    auto* verteces = static_cast<Vertex *>(vertexStagingBuffer->allocation_info.pMappedData);

    readVertexInto(&plyFile, &verteces[0], vertexType);

    verteces[1] = verteces[0];
    verteces[1].position[0] += 5.0f;

    printVertex(verteces[0], true);
    printVertex(verteces[1], true);

    vertexBuffer = createBuffer(context, header.numVertices * sizeof(Vertex));
    vertexBuffer->uploadFrom(vertexStagingBuffer);

    precomputeCov3D(context);
}


void GSScene::load(const std::shared_ptr<VulkanContext>&context) {
//    auto startTime = std::chrono::high_resolution_clock::now();

    std::istringstream plyFile(assetContent, std::ios::binary);
    int vertexType = loadPlyHeader(plyFile);

    auto vertexStagingBuffer = Buffer::staging(context, header.numVertices * sizeof(Vertex));
    auto* verteces = static_cast<Vertex *>(vertexStagingBuffer->allocation_info.pMappedData);
    LOGD("num vertexes: %i", header.numVertices);

    for (auto i = 0; i < header.numVertices; i++) {
        readVertexInto(&plyFile, &verteces[i], vertexType);
    }

    vertexBuffer = createBuffer(context, header.numVertices * sizeof(Vertex));
    vertexBuffer->uploadFrom(vertexStagingBuffer);

    precomputeCov3D(context);
}


void GSScene::loadTestScene(const std::shared_ptr<VulkanContext>&context) {
    int testObects = 1;
    header.numVertices = testObects;
    vertexBuffer = createBuffer(context, testObects * sizeof(Vertex));
    auto vertexStagingBuffer = Buffer::staging(context, testObects * sizeof(Vertex));
    auto* verteces = static_cast<Vertex *>(vertexStagingBuffer->allocation_info.pMappedData);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> posgen(-3.0, 3.0);
    std::uniform_real_distribution<> scalegen(100.0, 5000.0);
    std::uniform_real_distribution<> shsgen(-1.0, 1.0);


    for (auto i = 0; i < testObects; i++) {
        verteces[i].position = glm::vec4(posgen(gen), posgen(gen), posgen(gen), 1.0f);
        // verteces[i].normal = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        verteces[i].scale_opacity = glm::vec4(scalegen(gen), scalegen(gen), scalegen(gen), 0.5f);
        verteces[i].rotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        for (auto j = 0; j < 48; j++) {
            verteces[i].shs[j] = shsgen(gen);
        }
    }

    vertexBuffer->uploadFrom(vertexStagingBuffer);

    precomputeCov3D(context);
}

// return the type of VertexStorage struct to use (1 or 2)
// to make it simple, right now it's 1 if there are 62 properties and 2 if not
int GSScene::loadPlyHeader(std::istringstream& plyFile) {
    std::string line;
    bool headerEnd = false;
    int propertyCount = 0;
    while (std::getline(plyFile, line)) {
        std::istringstream iss(line);
        std::string token;

        iss >> token;

        if (token == "ply") {
            // PLY format indicator
        }
        else if (token == "format") {
            iss >> header.format;
        }
        else if (token == "element") {
            iss >> token;

            if (token == "vertex") {
                iss >> header.numVertices;
            }
            else if (token == "face") {
                iss >> header.numFaces;
            }
        }
        else if (token == "property") {
            propertyCount++;
            PlyProperty property;
            iss >> property.type >> property.name;

            if (header.vertexProperties.size() < static_cast<size_t>(header.numVertices)) {
                header.vertexProperties.push_back(property);
            }
            else {
                header.faceProperties.push_back(property);
            }
        }
        else if (token == "end_header") {
            headerEnd = true;
            break;
        }
    }

    if (!headerEnd) {
        throw std::runtime_error("Could not find end of header");
    }
    LOGD("PROPERTY COUNT: %i", propertyCount);
    return (propertyCount == 62) ? 1 : 2;
}

std::shared_ptr<Buffer> GSScene::createBuffer(const std::shared_ptr<VulkanContext>&context, size_t i) {
    return std::make_shared<Buffer>(
        context, i, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_GPU_ONLY, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, false);
}

// After loading verteces from .ply input file
// Calculate the upper triangle of the 3x3 covariance matrix for each Gaussian/vertex
// This matrix captures the scale and rotation of each Gaussian
void GSScene::precomputeCov3D(const std::shared_ptr<VulkanContext>&context) {
    cov3DBuffer = createBuffer(context, header.numVertices * sizeof(float) * 6);

    auto pipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "precomp_cov3d", SPV_PRECOMP_COV3D, SPV_PRECOMP_COV3D_len));

    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             vertexBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             cov3DBuffer);
    descriptorSet->build();

    pipeline->addDescriptorSet(0, descriptorSet);
    pipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(float));
    pipeline->build();

    auto commandBuffer = context->beginOneTimeCommandBuffer();
    pipeline->bind(commandBuffer, 0, 0);
    float scaleFactor = 1.0f;
    commandBuffer->pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0,
                                 sizeof(float), &scaleFactor);
    int numGroups = (header.numVertices + 255) / 256;
    commandBuffer->dispatch(numGroups, 1, 1);
    context->endOneTimeCommandBuffer(std::move(commandBuffer), VulkanContext::Queue::COMPUTE);

    spdlog::info("Precomputed Cov3D");
}
