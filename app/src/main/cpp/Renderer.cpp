#include "Renderer.h"

#include <fstream>

#include "vulkan/Swapchain.h"

#include <memory>
#include "shaders.h"
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "vulkan/Utils.h"
#include "base_utils.h"

#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

void Renderer::initialize() {
    initializeVulkan();
    createGui();
    loadSceneToGPU();
    createPreprocessPipeline();
    createPrefixSumPipeline();
    createRadixSortPipeline();
    createPreprocessSortPipeline();
    createTileBoundaryPipeline();
    createRenderPipeline();
    createCommandPool();
    recordPreprocessCommandBuffer();
}

void Renderer::handleInput() {
    LOGD("Rotation: %f, %f, %f, %f", camera.rotation[0], camera.rotation[1], camera.rotation[2], camera.rotation[3]);
    LOGD("Position: %f, %f, %f", camera.position[0], camera.position[1], camera.position[2]);

    using namespace std::chrono;
    auto now = steady_clock::now();
    const float speed = 1.0f; // speed factor

    // Get screen height from the window (assuming getWindowHeight() returns the height in pixels)
    float w = static_cast<float>(getWindowWidth());
    float h = static_cast<float>(getWindowHeight());
    // Define the resolution toggle region (bottom 5% of the screen)
    bool isInButtonArea = (initialTouchY > h * 0.95f);

    // Define center region
    float centerX1 = w / 3.0f;
    float centerX2 = 2.0f * w / 3.0f;
    float centerY1 = h / 6.0f;
    float centerY2 = 5.0f * h / 6.0f;

    // If the screen is held down, check how long and if it exceeds 500ms, move camera backward
    if (isTouching) {
        auto durationHeld = duration_cast<milliseconds>(now - lastDownTime).count();
        if (durationHeld > 700) {
            holdTap = true;
        } else {
            holdTap = false;
        }
    }

    static bool wasTouchingLastFrame = false;
    if (!isTouching && wasTouchingLastFrame) {
        // Finger was just lifted; let's see if it's a tap (distance check).
        float dx = lastX - initialTouchX;
        float dy = lastY - initialTouchY;
        float dist = std::sqrt(dx*dx + dy*dy);
        const float tapMovementThreshold = 15.0f; // tune as needed
        bool isTap = (dist < tapMovementThreshold);

        if (isTap) {
            if (initialTouchX >= centerX1 && initialTouchX <= centerX2 && initialTouchY >= centerY1 && initialTouchY <= centerY2) {
                // Tap in center region
                if (doubleTap) {
                    // Move forward
                    glm::vec3 forward = glm::normalize(glm::vec3(glm::mat3_cast(camera.rotation) * glm::vec3(0.0f, 0.0f, -1.0f)));
                    camera.position += forward * speed;
                }
            }
            else {
                // It's outside the center region.  Check top, bottom, left, right.

                // Tap left region => move left
                if (initialTouchX < centerX1 && initialTouchY >= centerY1 && initialTouchY <= centerY2) {
                    glm::vec3 right = camera.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                    camera.position -= right * speed; // left
                }
                // Tap right region => move right
                else if (initialTouchX > centerX2 && initialTouchY >= centerY1 && initialTouchY <= centerY2) {
                    glm::vec3 right = camera.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                    camera.position += right * speed; // right
                }
                // Tap top region => move up
                else if (initialTouchY < centerY1 && initialTouchX >= centerX1 && initialTouchX <= centerX2) {
                    glm::vec3 up = camera.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                    camera.position += up * speed;
                }
                // Tap bottom region (but not in the resolution toggle area) => move down
                else if (initialTouchY > centerY2 && initialTouchX >= centerX1 && initialTouchX <= centerX2 && !isInButtonArea) {
                    glm::vec3 up = camera.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                    camera.position -= up * speed;
                }
                else {

                }
            }
        }
        // Reset doubleTap after a tap
        doubleTap = false;
    } else if (isTouching && holdTap && initialTouchX >= centerX1 && initialTouchX <= centerX2 && initialTouchY >= centerY1 && initialTouchY <= centerY2 && (touchDeltaX == 0.0 && touchDeltaY == 0.0)) {
        glm::vec3 backward = glm::normalize(glm::vec3(glm::mat3_cast(camera.rotation) * glm::vec3(0.0f, 0.0f, 1.0f)));
        camera.position += backward * speed;
    }

    // Rotate camera
    if (isTouching && (touchDeltaX != 0.0 || touchDeltaY != 0.0)) {
       camera.rotation = glm::rotate(camera.rotation, static_cast<float>(touchDeltaX) * 0.005f,
                                     glm::vec3(0.0f, -1.0f, 0.0f));
       camera.rotation = glm::rotate(camera.rotation, static_cast<float>(touchDeltaY) * 0.005f,
                                     glm::vec3(-1.0f, 0.0f, 0.0f));
   }
    // Reset touch deltas after processing
    touchDeltaX = 0.0f;
    touchDeltaY = 0.0f;

    wasTouchingLastFrame = isTouching;
}

void Renderer::moveCameraForProfiling() {
    if(profilingMode == FPS){
        camera.rotation = glm::rotate(camera.rotation, static_cast<float>(1) * 0.005f,
                                      glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if(profilingMode == PSNR){
//        glm::quat rot_quat = camera.rotation;
//        LOGO("QUATERNION: %f, %f, %f, %f", rot_quat[0], rot_quat[1], rot_quat[2], rot_quat[3]);
//
//        glm::vec3 translation = camera.position;
//        LOGO("TRANSLATION: %f, %f, %f", translation[0], translation[1], translation[2]);

        // read from camera pose array

        // change camera rotation
        glm::mat3x3 rotation = rotations[cameraPosIndex];
        glm::quat rot_quat = glm::quat_cast(rotation);

        LOGO("QUATERNION: %f, %f, %f, %f", rot_quat[0], rot_quat[1], rot_quat[2], rot_quat[3]);
        camera.rotation = rot_quat;

        glm::vec3 translation = (-glm::transpose(rotation)) * translations[cameraPosIndex];
        LOGO("TRANSLATION: %f, %f, %f", translation[0], translation[1], translation[2]);

        camera.position = glm::vec3(static_cast<float>(translation[0]), static_cast<float>(translation[1]), static_cast<float>(translation[2]));

        cameraPosIndex = (cameraPosIndex + 1) % rotations.size();
    }
}

void Renderer::retrieveTimestamps() {
    std::vector<uint64_t> timestamps(queryManager->nextId);
    auto res = context->device->getQueryPoolResults(context->queryPool.get(), 0, queryManager->nextId,
                                                    timestamps.size() * sizeof(uint64_t),
                                                    timestamps.data(), sizeof(uint64_t),
                                                    vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
    if (res != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to retrieve timestamps");
    }

    auto metrics = queryManager->parseResults(timestamps);
    for (auto& metric: metrics) {
//        LOGO("METRIC: %s: %i", metric.first.c_str(), metric.second);
        if (configuration.enableGui)
            guiManager.pushMetric(metric.first, metric.second / 1000000.0);
    }
}

void Renderer::recreateSwapchain() {
    auto oldExtent = swapchain->swapchainExtent;
    LOGD("Recreating swapchain");
    swapchain->recreate();
    if (swapchain->swapchainExtent == oldExtent) {
        return;
    }

    auto [width, height] = swapchain->swapchainExtent;
    auto tileX = (width + 16 - 1) / 16;
    auto tileY = (height + 16 - 1) / 16;
    tileBoundaryBuffer->realloc(tileX * tileY * sizeof(uint32_t) * 2);

    recordPreprocessCommandBuffer();
    createRenderPipeline();
}

void Renderer::initializeVulkan() {
    LOGD("Initializing Vulkan");
    window = configuration.window;
    context = std::make_shared<VulkanContext>(
            window->getRequiredInstanceExtensions(),
            std::vector<std::string>{},
            configuration.enableVulkanValidationLayers
            );

    context->createInstance();
    auto surface = static_cast<vk::SurfaceKHR>(window->createSurface(context));

    // physical device represents what your hardware can do
    // e.g. tesselation, shading, etc.
    context->selectPhysicalDevice(configuration.physicalDeviceId, surface);

    vk::PhysicalDeviceFeatures pdf{};
    vk::PhysicalDeviceVulkan11Features pdf11{};
    vk::PhysicalDeviceVulkan12Features pdf12{};
    pdf.shaderStorageImageWriteWithoutFormat = true;

    // we originally had Int64, but the physicalDevice doesn't allow it, only Int16
    pdf.shaderInt16 = true;
    // pdf.shaderInt64 = true;

    // This started commented this out...
    // pdf.robustBufferAccess = true;

    // I added this
     pdf12.shaderFloat16 = true;

    // This is a Vulkan 1.2 feature that isn't available on our physical device. I don't think we can enable it
    // can we replace this with the Int16 feature that we do have access to?

    // pdf12.shaderBufferInt64Atomics = true;
    // pdf12.shaderSharedInt64Atomics = true;

    // represents subset of physical device features that we use
    context->createLogicalDevice(pdf, pdf11, pdf12);
    LOGD("Created Logical Device");
    context->createDescriptorPool(1);

    swapchain = std::make_shared<Swapchain>(context, window, configuration.immediateSwapchain);

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        inflightFences.emplace_back(
            context->device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
    }

    renderFinishedSemaphores.resize(FRAMES_IN_FLIGHT);
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        renderFinishedSemaphores[i] = context->device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
    }
}

void Renderer::loadSceneToGPU() {
    LOGD("Loading scene to GPU");
    scene = std::make_shared<GSScene>(configuration.assetContent);
    scene->load(context);

    // reset descriptor pool
    context->device->resetDescriptorPool(context->descriptorPool.get());
}

void Renderer::createPreprocessPipeline() {
    LOGD("Creating preprocess pipeline");
    uniformBuffer = Buffer::uniform(context, sizeof(UniformBuffer));
    vertexAttributeBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(VertexAttributeBuffer), false);
    tileOverlapBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t), false);

    preprocessPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "preprocess", SPV_PREPROCESS, SPV_PREPROCESS_len));
    inputSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    inputSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        scene->vertexBuffer);
    inputSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        scene->cov3DBuffer);
    inputSet->build();
    preprocessPipeline->addDescriptorSet(0, inputSet);

    auto uniformOutputSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    uniformOutputSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eUniformBuffer,
                                                vk::ShaderStageFlagBits::eCompute,
                                                uniformBuffer);
    uniformOutputSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer,
                                                vk::ShaderStageFlagBits::eCompute,
                                                vertexAttributeBuffer);
    uniformOutputSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer,
                                                vk::ShaderStageFlagBits::eCompute,
                                                tileOverlapBuffer);
    uniformOutputSet->build();

    preprocessPipeline->addDescriptorSet(1, uniformOutputSet);
    preprocessPipeline->build();
}

Renderer::Renderer(VulkanSplatting::RendererConfiguration& configuration, int scene_path_index) {
    this->configuration = configuration;
    this->profilingMode = configuration.profilingMode;
    this->rotations = configuration.rotations;
    this->translations = configuration.translations;
    this->assetManager = configuration.assetManager;

    LOGD("SCENE INDEX: %i", scene_path_index);
    this->camera = initialCameraPoses[scene_path_index];
    LOGD("Initial Rotation: %f, %f, %f, %f", this->camera.rotation[0], this->camera.rotation[1], this->camera.rotation[2], this->camera.rotation[3]);
    LOGD("Initial Position: %f, %f, %f", this->camera.position[0], this->camera.position[1], this->camera.position[2]);
}

void Renderer::createGui() {
    if (!configuration.enableGui) {
        return;
    }

    LOGD("Creating GUI");

    imguiManager = std::make_shared<ImguiManager>(context, swapchain, window);
    imguiManager->init();
    guiManager.init();
}

void Renderer::createPrefixSumPipeline() {
    LOGD("Creating prefix sum pipeline");
    prefixSumPingBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t), false);
    prefixSumPongBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t), false);
    totalSumBufferHost = Buffer::staging(context, sizeof(uint32_t));

    prefixSumPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "prefix_sum", SPV_PREFIX_SUM, SPV_PREFIX_SUM_len));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPingBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPongBuffer);
    descriptorSet->build();

    prefixSumPipeline->addDescriptorSet(0, descriptorSet);
    prefixSumPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t));
    prefixSumPipeline->build();
}

void Renderer::createRadixSortPipeline() {
    LOGD("Creating radix sort pipeline");
    sortKBufferEven = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier,
                                      false, 0, "sortKBufferEven");
    sortKBufferOdd = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier,
                                     false, 0, "sortKBufferOdd");
    sortVBufferEven = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier,
                                      false, 0, "sortVBufferEven");
    sortVBufferOdd = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier,
                                     false, 0, "sortVBufferOdd");

    uint32_t globalInvocationSize = scene->getNumVertices() * sortBufferSizeMultiplier / numRadixSortBlocksPerWorkgroup;
    uint32_t remainder = scene->getNumVertices() * sortBufferSizeMultiplier % numRadixSortBlocksPerWorkgroup;
    globalInvocationSize += remainder > 0 ? 1 : 0;

    auto numWorkgroups = (globalInvocationSize + 256 - 1) / 256;

    sortHistBuffer = Buffer::storage(context, numWorkgroups * 256 * sizeof(uint32_t), false);

    sortHistPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "hist", SPV_HIST, SPV_HIST_len));

    // The sort shader is causing the sortPipeline->build() to fail
    sortPipeline = std::make_shared<ComputePipeline>(
            context, std::make_shared<Shader>(context, "sort", SPV_SORT, SPV_SORT_len));

    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortHistBuffer);
    descriptorSet->build();
    sortHistPipeline->addDescriptorSet(0, descriptorSet);
    sortHistPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(RadixSortPushConstants));
    sortHistPipeline->build();

    descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferEven);
    descriptorSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferEven);
    descriptorSet->bindBufferToDescriptorSet(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortHistBuffer);
    descriptorSet->build();
    sortPipeline->addDescriptorSet(0, descriptorSet);
    sortPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(RadixSortPushConstants));
    sortPipeline->build();
}

void Renderer::createPreprocessSortPipeline() {
    LOGD("Creating preprocess sort pipeline");
    preprocessSortPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "preprocess_sort", SPV_PREPROCESS_SORT, SPV_PREPROCESS_SORT_len));

    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             vertexAttributeBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPingBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPongBuffer);
    descriptorSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferEven);
    descriptorSet->build();

    preprocessSortPipeline->addDescriptorSet(0, descriptorSet);
    preprocessSortPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t));
    preprocessSortPipeline->build();
}

void Renderer::createTileBoundaryPipeline() {
    LOGD("Creating tile boundary pipeline");
    auto [width, height] = swapchain->swapchainExtent;
    auto tileX = (width + 16 - 1) / 16;
    auto tileY = (height + 16 - 1) / 16;
    tileBoundaryBuffer = Buffer::storage(context, tileX * tileY * sizeof(uint32_t) * 2, false);

    tileBoundaryPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "tile_boundary", SPV_TILE_BOUNDARY, SPV_TILE_BOUNDARY_len));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    // descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
    //                                          sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             tileBoundaryBuffer);
    descriptorSet->build();

    tileBoundaryPipeline->addDescriptorSet(0, descriptorSet);
    tileBoundaryPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t));
    tileBoundaryPipeline->build();
}

void Renderer::createRenderPipeline() {
    LOGD("Creating render pipeline");
    renderPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "render", SPV_RENDER, SPV_RENDER_len));
    auto inputSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    inputSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        vertexAttributeBuffer);
    inputSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        tileBoundaryBuffer);
    inputSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        sortVBufferEven);
    // inputSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
    //                                     sortKBufferOdd);
    inputSet->build();

    auto outputSet = std::make_shared<DescriptorSet>(context, 1);
    for (auto& image: swapchain->swapchainImages) {
        outputSet->bindImageToDescriptorSet(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute,
                                            image);
    }
    outputSet->build();
    renderPipeline->addDescriptorSet(0, inputSet);
    renderPipeline->addDescriptorSet(1, outputSet);
    renderPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t) * 3); // Added useHalfResolution flag
    renderPipeline->build();
}

void Renderer::draw() {
    auto now = std::chrono::high_resolution_clock::now();
    
    // Initialize the 30-second timer on first frame
    if (!thirtySecondIntervalStarted) {
        thirtySecondIntervalStart = now;
        thirtySecondFrameCount = 0;
        thirtySecondIntervalStarted = true;
    }
    
    // Increment frame counters
    fpsCounter++;
    thirtySecondFrameCount++;
    frame_count++;
    
    // Current 1-second FPS counter
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTime).count();
    if (diff > 1000) {
        //LOGO("FPS: %i", fpsCounter);
        realerFps = fpsCounter;
        sum += fpsCounter;
        fpsCounter = 0;
        lastFpsTime = now;
    }
    
    // Check if 30 seconds have elapsed
    if(profilingMode == FPS) {
        auto thirtySecondDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - thirtySecondIntervalStart).count();
        if (thirtySecondDiff >= 30000) {  // 30 seconds in milliseconds
            // Calculate average FPS over the 30-second interval
            thirtySecondAvgFps = static_cast<float>(thirtySecondFrameCount) / 30.0f;

            // Log the 30-second average FPS
            LOGO("30-SECOND AVERAGE FPS: %.2f (%u frames)",
                 thirtySecondAvgFps, thirtySecondFrameCount);

            // Reset for next 30-second interval
            thirtySecondIntervalStart = now;
            thirtySecondFrameCount = 0;
        }
    }
    
    // Existing 100-frame average calculation
    if (frame_count % 100 == 0) {
        realestFps = sum / 100;
        sum = 0;
        //LOGO("AVG FPS (100 frames): %i", realestFps);
    }
    
    auto ret = context->device->waitForFences(inflightFences[0].get(), VK_TRUE, UINT64_MAX);
    if (ret != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to wait for fence");
    }
    context->device->resetFences(inflightFences[0].get());

    auto res = context->device->acquireNextImageKHR(swapchain->swapchain.get(), UINT64_MAX,
                                                    swapchain->imageAvailableSemaphores[0].get(),
                                                    nullptr, &currentImageIndex);
    if (res == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapchain();
        return;
    } else if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

startOfRenderLoop:

#ifdef DEBUG
    handleInput();
#else
    moveCameraForProfiling();
#endif

    updateUniforms();

    auto submitInfo = vk::SubmitInfo{}.setCommandBuffers(preprocessCommandBuffer.get());
    context->queues[VulkanContext::Queue::COMPUTE].queue.submit(submitInfo, inflightFences[0].get());

    ret = context->device->waitForFences(inflightFences[0].get(), VK_TRUE, UINT64_MAX);
    if (ret != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to wait for fence");
    }
    context->device->resetFences(inflightFences[0].get());

    if (!recordRenderCommandBuffer(0)) {
        goto startOfRenderLoop;
    }
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eComputeShader;
    submitInfo = vk::SubmitInfo{}.setWaitSemaphores(swapchain->imageAvailableSemaphores[0].get())
            .setCommandBuffers(renderCommandBuffer.get())
            .setSignalSemaphores(renderFinishedSemaphores[0].get())
            .setWaitDstStageMask(waitStage);
    context->queues[VulkanContext::Queue::COMPUTE].queue.submit(submitInfo, inflightFences[0].get());

    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[0].get();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain->swapchain.get();
    presentInfo.pImageIndices = &currentImageIndex;

    try {
        ret = context->queues[VulkanContext::Queue::PRESENT].queue.presentKHR(presentInfo);
    } catch (vk::OutOfDateKHRError& e) {
        recreateSwapchain();
        return;
    }
    
    if (ret == vk::Result::eErrorOutOfDateKHR || ret == vk::Result::eSuboptimalKHR) {
        recreateSwapchain();
    } else if (ret != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void Renderer::run() {
    const int frames_to_measure = 1000;
    float fps_results[frames_to_measure];
    int num_frames = 0;
    static int frameCounter = 0;

    while (running) {
        if (!window->tick()) {
            break;
        }

//        auto before = std::chrono::high_resolution_clock::now();
        draw();
//        float seconds_per_frame = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - before).count();

        if(showMetrics){
            retrieveTimestamps();
        }

//        frameCounter++;
//        if (profilingMode == PSNR && frameCounter % 100 == 0) {
//            auto renderedImage = retrieveRenderedImage();
//            // Get the dimensions of the rendered image.
//            int renderedWidth  = static_cast<int>(swapchain->swapchainExtent.width);
//            int renderedHeight = static_cast<int>(swapchain->swapchainExtent.height);
//            auto groundTruth = loadGroundTruthImage(assetManager, "DSCF5572.JPG", renderedWidth, renderedHeight, 4);
//            float psnrValue = computePSNR(renderedImage, groundTruth, 1.0f);
//            LOGO("PSNR for current frame: %f dB", psnrValue);
//        }

        if(switchScene){
            LOGD("Swapping scenes");
            uniformBuffer.reset();
            vertexAttributeBuffer.reset();
            tileOverlapBuffer.reset();
            prefixSumPingBuffer.reset();
            prefixSumPongBuffer.reset();
            sortKBufferEven.reset();
            sortKBufferOdd.reset();
            sortHistBuffer.reset();
            totalSumBufferHost.reset();
            tileBoundaryBuffer.reset();
            sortVBufferEven.reset();
            sortVBufferOdd.reset();
            switchScene = false;
            break;
        }
    }
    context->device->waitIdle();
}

void Renderer::stop() {
    // wait till device is idle
    running = false;

    context->device->waitIdle();
}

void Renderer::createCommandPool() {
    LOGD("Creating command pool");
    vk::CommandPoolCreateInfo poolInfo = {};
    poolInfo.queueFamilyIndex = context->queues[VulkanContext::Queue::COMPUTE].queueFamily;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPool = context->device->createCommandPoolUnique(poolInfo, nullptr);
}

void Renderer::writeTimestamp(const std::string &name, vk::UniqueCommandBuffer & buffer){
    if(showMetrics){
        buffer->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, context->queryPool.get(),
                                                queryManager->registerQuery(name));
    }
}

void Renderer::recordPreprocessCommandBuffer() {
    LOGD("Recording preprocess command buffer");
    if (!preprocessCommandBuffer) {
        vk::CommandBufferAllocateInfo allocateInfo = {commandPool.get(), vk::CommandBufferLevel::ePrimary, 1};
        auto buffers = context->device->allocateCommandBuffersUnique(allocateInfo);
        preprocessCommandBuffer = std::move(buffers[0]);
    }
    preprocessCommandBuffer->reset();

    auto numGroups = (scene->getNumVertices() + 255) / 256;

    preprocessCommandBuffer->begin(vk::CommandBufferBeginInfo{});

    preprocessCommandBuffer->resetQueryPool(context->queryPool.get(), 0, 12);

    preprocessPipeline->bind(preprocessCommandBuffer, 0, 0);
    writeTimestamp("preprocess_start", preprocessCommandBuffer);
    preprocessCommandBuffer->dispatch(numGroups, 1, 1);
    tileOverlapBuffer->computeWriteReadBarrier(preprocessCommandBuffer.get());

    vk::BufferCopy copyRegion = {0, 0, tileOverlapBuffer->size};
    preprocessCommandBuffer->copyBuffer(tileOverlapBuffer->buffer, prefixSumPingBuffer->buffer, 1, &copyRegion);

    prefixSumPingBuffer->computeWriteReadBarrier(preprocessCommandBuffer.get());

    writeTimestamp("preprocess_end", preprocessCommandBuffer);

    prefixSumPipeline->bind(preprocessCommandBuffer, 0, 0);
    writeTimestamp("prefix_sum_start", preprocessCommandBuffer);
    const auto iters = static_cast<uint32_t>(std::ceil(std::log2(static_cast<float>(scene->getNumVertices()))));
    for (uint32_t timestep = 0; timestep <= iters; timestep++) {
        preprocessCommandBuffer->pushConstants(prefixSumPipeline->pipelineLayout.get(),
                                               vk::ShaderStageFlagBits::eCompute, 0,
                                               sizeof(uint32_t), &timestep);
        preprocessCommandBuffer->dispatch(numGroups, 1, 1);

        if (timestep % 2 == 0) {
            prefixSumPongBuffer->computeWriteReadBarrier(preprocessCommandBuffer.get());
            prefixSumPingBuffer->computeReadWriteBarrier(preprocessCommandBuffer.get());
        } else {
            prefixSumPingBuffer->computeWriteReadBarrier(preprocessCommandBuffer.get());
            prefixSumPongBuffer->computeReadWriteBarrier(preprocessCommandBuffer.get());
        }
    }

    auto totalSumRegion = vk::BufferCopy{(scene->getNumVertices() - 1) * sizeof(uint32_t), 0, sizeof(uint32_t)};
    if (iters % 2 == 0) {
        preprocessCommandBuffer->copyBuffer(prefixSumPingBuffer->buffer, totalSumBufferHost->buffer, 1,
                                            &totalSumRegion);
    } else {
        preprocessCommandBuffer->copyBuffer(prefixSumPongBuffer->buffer, totalSumBufferHost->buffer, 1,
                                            &totalSumRegion);
    }

    writeTimestamp("prefix_sum_end", preprocessCommandBuffer);

    preprocessCommandBuffer->end();
}


bool Renderer::recordRenderCommandBuffer(uint32_t currentFrame) {
    if (!renderCommandBuffer) {
        renderCommandBuffer = std::move(context->device->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo(commandPool.get(), vk::CommandBufferLevel::ePrimary, 1))[0]);
    }

    uint32_t numInstances = totalSumBufferHost->readOne<uint32_t>();
//    LOGD("Num instances: %i, Num Vertices: %i", numInstances, scene->getNumVertices());
    guiManager.pushTextMetric("instances", numInstances);
    guiManager.pushTextMetric("fps", realerFps);
    if (numInstances > scene->getNumVertices() * sortBufferSizeMultiplier) {
        auto old = sortBufferSizeMultiplier;
        while (numInstances > scene->getNumVertices() * sortBufferSizeMultiplier) {
            sortBufferSizeMultiplier++;
        }
        LOGD("Reallocating sort buffers. %i -> %i", old, sortBufferSizeMultiplier);
        sortKBufferEven->realloc(scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier);
        sortKBufferOdd->realloc(scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier);
        sortVBufferEven->realloc(scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier);
        sortVBufferOdd->realloc(scene->getNumVertices() * sizeof(uint32_t) * sortBufferSizeMultiplier);

        uint32_t globalInvocationSize = scene->getNumVertices() * sortBufferSizeMultiplier /
                                        numRadixSortBlocksPerWorkgroup;
        uint32_t remainder = scene->getNumVertices() * sortBufferSizeMultiplier % numRadixSortBlocksPerWorkgroup;
        globalInvocationSize += remainder > 0 ? 1 : 0;

        auto numWorkgroups = (globalInvocationSize + 256 - 1) / 256;

        sortHistBuffer->realloc(numWorkgroups * 256 * sizeof(uint32_t));

        recordPreprocessCommandBuffer();
        return false;
    }

    renderCommandBuffer->reset({});
    renderCommandBuffer->begin(vk::CommandBufferBeginInfo{});

#ifdef VKGS_ENABLE_METAL
    if (numInstances == 0 && __APPLE__) {
        renderCommandBuffer->end();
        return true;
    }
#endif

    vertexAttributeBuffer->computeWriteReadBarrier(renderCommandBuffer.get());

    const auto iters = static_cast<uint32_t>(std::ceil(std::log2(static_cast<float>(scene->getNumVertices()))));
    auto numGroups = (scene->getNumVertices() + 255) / 256;
    preprocessSortPipeline->bind(renderCommandBuffer, 0, iters % 2 == 0 ? 0 : 1);
    writeTimestamp("preprocess_sort_start", renderCommandBuffer);
    uint32_t tileX = (swapchain->swapchainExtent.width + 16 - 1) / 16;
    // assert(tileX == 50);
    renderCommandBuffer->pushConstants(preprocessSortPipeline->pipelineLayout.get(),
                                           vk::ShaderStageFlagBits::eCompute, 0,
                                           sizeof(uint32_t), &tileX);
    renderCommandBuffer->dispatch(numGroups, 1, 1);

    sortKBufferEven->computeWriteReadBarrier(renderCommandBuffer.get());

    writeTimestamp("preprocess_sort_end", renderCommandBuffer);

    // std::cout << "Num instances: " << numInstances << std::endl;

    assert(numInstances <= scene->getNumVertices() * sortBufferSizeMultiplier);
    renderCommandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, context->queryPool.get(),
                                                queryManager->registerQuery("sort_start"));
    for (auto i = 0; i < 4; i++) {
        sortHistPipeline->bind(renderCommandBuffer, 0, i % 2 == 0 ? 0 : 1);
        auto invocationSize = (numInstances + numRadixSortBlocksPerWorkgroup - 1) / numRadixSortBlocksPerWorkgroup;
        invocationSize = (invocationSize + 255) / 256;

        RadixSortPushConstants pushConstants{};
        pushConstants.g_num_elements = numInstances;
        pushConstants.g_num_blocks_per_workgroup = numRadixSortBlocksPerWorkgroup;
        // THIS AFFECTS THE NOISE!
        pushConstants.g_shift = i * 8;
        pushConstants.g_num_workgroups = invocationSize;
        renderCommandBuffer->pushConstants(sortHistPipeline->pipelineLayout.get(),
                                           vk::ShaderStageFlagBits::eCompute, 0,
                                           sizeof(RadixSortPushConstants), &pushConstants);

        renderCommandBuffer->dispatch(invocationSize, 1, 1);

        sortHistBuffer->computeWriteReadBarrier(renderCommandBuffer.get());

        sortPipeline->bind(renderCommandBuffer, 0, i % 2 == 0 ? 0 : 1);
        renderCommandBuffer->pushConstants(sortPipeline->pipelineLayout.get(),
                                           vk::ShaderStageFlagBits::eCompute, 0,
                                           sizeof(RadixSortPushConstants), &pushConstants);
        renderCommandBuffer->dispatch(invocationSize, 1, 1);

        if (i % 2 == 0) {
            sortKBufferOdd->computeWriteReadBarrier(renderCommandBuffer.get());
            sortVBufferOdd->computeWriteReadBarrier(renderCommandBuffer.get());
        } else {
            sortKBufferEven->computeWriteReadBarrier(renderCommandBuffer.get());
            sortVBufferEven->computeWriteReadBarrier(renderCommandBuffer.get());
        }
    }
    writeTimestamp("sort_end", renderCommandBuffer);

    renderCommandBuffer->fillBuffer(tileBoundaryBuffer->buffer, 0, VK_WHOLE_SIZE, 0);

    Utils::BarrierBuilder().queueFamilyIndex(context->queues[VulkanContext::Queue::COMPUTE].queueFamily)
            .addBufferBarrier(tileBoundaryBuffer, vk::AccessFlagBits::eTransferWrite,
                              vk::AccessFlagBits::eShaderWrite)
            .build(renderCommandBuffer.get(), vk::PipelineStageFlagBits::eTransfer,
                   vk::PipelineStageFlagBits::eComputeShader);

    // Since we have 64 bit keys, the sort result is always in the even buffer
    tileBoundaryPipeline->bind(renderCommandBuffer, 0, 0);
    writeTimestamp("tile_boundary_start", renderCommandBuffer);
    renderCommandBuffer->pushConstants(tileBoundaryPipeline->pipelineLayout.get(),
                                       vk::ShaderStageFlagBits::eCompute, 0,
                                       sizeof(uint32_t), &numInstances);
    renderCommandBuffer->dispatch((numInstances + 255) / 256, 1, 1);

    tileBoundaryBuffer->computeWriteReadBarrier(renderCommandBuffer.get());
    writeTimestamp("tile_boundary_end", renderCommandBuffer);

    renderPipeline->bind(renderCommandBuffer, 0, std::vector<uint32_t>{0, currentImageIndex});
    writeTimestamp("render_start", renderCommandBuffer);
    auto [width, height] = swapchain->swapchainExtent;
    uint32_t constants[3] = {width, height, isUsingHalfResolution() ? 1u : 0u};
    renderCommandBuffer->pushConstants(renderPipeline->pipelineLayout.get(),
                                       vk::ShaderStageFlagBits::eCompute, 0,
                                       sizeof(uint32_t) * 3, constants);

    // image layout transition: undefined -> general
    vk::ImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.image = swapchain->swapchainImages[currentImageIndex]->image;
    imageMemoryBarrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                         vk::PipelineStageFlagBits::eComputeShader,
                                         vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);

    // When in half resolution mode, we dispatch half the work groups
    if (isUsingHalfResolution()) {
        uint32_t halfWidth = (width + 1) / 2;
        uint32_t halfHeight = (height + 1) / 2;
        renderCommandBuffer->dispatch((halfWidth + 15) / 16, (halfHeight + 15) / 16, 1);
    } else {
        renderCommandBuffer->dispatch((width + 15) / 16, (height + 15) / 16, 1);
    }

    // image layout transition: general -> present
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    if (configuration.enableGui) {
        imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                             vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);
    } else {
        imageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
        renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eBottomOfPipe,
                                             vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);
    }
    writeTimestamp("render_end", renderCommandBuffer);

    if (configuration.enableGui) {
        imguiManager->draw(renderCommandBuffer.get(), currentImageIndex, std::bind(&GUIManager::buildGui, &guiManager));

        imageMemoryBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        imageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;

        renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                             vk::PipelineStageFlagBits::eComputeShader,
                                             vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);
    }
    renderCommandBuffer->end();

    return true;
}

void Renderer::updateUniforms() {
    UniformBuffer data{};
    auto [width, height] = swapchain->swapchainExtent;
    data.width = width;
    data.height = height;
    data.camera_position = glm::vec4(camera.position, 1.0f);

    auto rotation = glm::mat4_cast(camera.rotation);
    auto translation = glm::translate(glm::mat4(1.0f), camera.position);
    auto view = glm::inverse(translation * rotation);

    float tan_fovx = std::tan(glm::radians(camera.fov) / 2.0);
    float tan_fovy = tan_fovx * static_cast<float>(height) / static_cast<float>(width);
    data.view_mat = view;
    data.proj_mat = glm::perspective(std::atan(tan_fovy) * 2.0f,
                                     static_cast<float>(width) / static_cast<float>(height),
                                     camera.nearPlane,
                                     camera.farPlane) * view;

    data.view_mat[0][1] *= -1.0f;
    data.view_mat[1][1] *= -1.0f;
    data.view_mat[2][1] *= -1.0f;
    data.view_mat[3][1] *= -1.0f;
    data.view_mat[0][2] *= -1.0f;
    data.view_mat[1][2] *= -1.0f;
    data.view_mat[2][2] *= -1.0f;
    data.view_mat[3][2] *= -1.0f;

    data.proj_mat[0][1] *= -1.0f;
    data.proj_mat[1][1] *= -1.0f;
    data.proj_mat[2][1] *= -1.0f;
    data.proj_mat[3][1] *= -1.0f;
    data.tan_fovx = tan_fovx;
    data.tan_fovy = tan_fovy;
    uniformBuffer->upload(&data, sizeof(UniformBuffer), 0);
}

float Renderer::computePSNR(const std::vector<float>& rendered, const std::vector<float>& groundTruth, float maxValue) {
    if (rendered.size() != groundTruth.size()) {
        throw std::runtime_error("Rendered and ground truth images must have the same number of pixels");
    }

    double mse = 0.0;
    for (size_t i = 0; i < rendered.size(); ++i) {
        double diff = rendered[i] - groundTruth[i];
        mse += diff * diff;
    }
    mse /= rendered.size();

    if (mse == 0.0)
        return std::numeric_limits<float>::infinity(); // perfect match

    float psnr = 20.0f * std::log10(maxValue) - 10.0f * std::log10(mse);
    return psnr;
}

std::vector<unsigned char> Renderer::loadAssetBytes(AAssetManager* assetManager, const char* filename) {
    AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        throw std::runtime_error(std::string("Failed to open asset: ") + filename);
    }
    size_t size = AAsset_getLength(asset);
    std::vector<unsigned char> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
}

// Resize ground-truth image to match the rendered image dimensions.
std::vector<float> Renderer::resizeGroundTruth(const std::vector<float>& groundTruthFloat, int gtWidth, int gtHeight,
                                               int targetWidth, int targetHeight, int channels)
{
    // Allocate output vector with the new size.
    std::vector<float> resizedImage(targetWidth * targetHeight * channels);

    // stbir_resize_float expects the input and output strides in bytes.
    int input_stride = gtWidth * channels * sizeof(float);
    int output_stride = targetWidth * channels * sizeof(float);

    int result = stbir_resize_float(groundTruthFloat.data(), gtWidth, gtHeight, input_stride,
                                    resizedImage.data(), targetWidth, targetHeight, output_stride,
                                    channels);
    if (result == 0) {
        throw std::runtime_error("Failed to resize ground truth image");
    }
    return resizedImage;
}

std::vector<float> Renderer::loadGroundTruthImage(AAssetManager* assetManager, const char* filename, int targetWidth, int targetHeight, int req_channels) {
    // Load asset bytes into memory.
    auto fileData = loadAssetBytes(assetManager, filename);
    int width, height, channels;
    unsigned char* imgData = stbi_load_from_memory(fileData.data(),static_cast<int>(fileData.size()),
                                                   &width, &height, &channels,req_channels);
    if (!imgData) {
        throw std::runtime_error(std::string("Failed to load image from asset: ") + filename);
    }
    // Convert to float [0,1]
    size_t pixelCount = static_cast<size_t>(width) * height * req_channels;
    std::vector<float> imageFloat(pixelCount);
    for (size_t i = 0; i < pixelCount; i++) {
        imageFloat[i] = imgData[i] / 255.0f;
    }
    stbi_image_free(imgData);

    // If the loaded image size doesn't match the target size, resize it.
    if (width != targetWidth || height != targetHeight) {
        std::vector<float> resizedImage = resizeGroundTruth(imageFloat, width, height,
                                                            targetWidth, targetHeight, req_channels);
        return resizedImage;
    }
    return imageFloat;
}

std::vector<float> Renderer::retrieveRenderedImage() {
    uint32_t width = swapchain->swapchainExtent.width;
    uint32_t height = swapchain->swapchainExtent.height;
    // Assuming the swapchain image is stored as 8-bit per channel RGBA.
    vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(width) * height * 4 * sizeof(unsigned char);

    // Create a staging buffer that is host-visible.
    std::shared_ptr<Buffer> stagingBuffer = Buffer::staging(context, static_cast<uint32_t>(imageSize));

    // Begin a one-time command buffer.
    vk::UniqueCommandBuffer cmdBuffer = context->beginOneTimeCommandBuffer();

    // Transition the swapchain image from PRESENT to TRANSFER_SRC.
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
    barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swapchain->swapchainImages[currentImageIndex]->image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    cmdBuffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            vk::DependencyFlagBits::eByRegion,
            nullptr, nullptr, barrier
    );

    // Define the region to copy.
    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;   // Tightly packed.
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{width, height, 1};

    // Copy the image into the staging buffer.
    cmdBuffer->copyImageToBuffer(
            swapchain->swapchainImages[currentImageIndex]->image,
            vk::ImageLayout::eTransferSrcOptimal,
            stagingBuffer->buffer,
            region
    );

    // Transition the swapchain image back to PRESENT layout.
    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
    cmdBuffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::DependencyFlagBits::eByRegion,
            nullptr, nullptr, barrier
    );

    cmdBuffer->end();

    // Submit the command buffer and wait for it to finish.
    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*cmdBuffer;
    context->queues[VulkanContext::Queue::PRESENT].queue.submit(submitInfo, nullptr);
    context->queues[VulkanContext::Queue::PRESENT].queue.waitIdle();

    // Retrieve the pixel data from the staging buffer as unsigned char.
    std::vector<unsigned char> rawPixels(width * height * 4);
    std::memcpy(rawPixels.data(), stagingBuffer->allocation_info.pMappedData, static_cast<size_t>(imageSize));

    // Convert the raw pixel values (0-255) to floats in the range [0, 1].
    std::vector<float> pixels;
    pixels.reserve(width * height * 4);
    for (unsigned char val : rawPixels) {
        pixels.push_back(static_cast<float>(val) / 255.0f);
    }
    return pixels;
}

Renderer::~Renderer() {
}
