#ifndef RENDERER_H
#define RENDERER_H

#define GLM_SWIZZLE

#include <atomic>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <cstring>
#include <iostream>
#include "3dgs.h"

#include "vulkan/Window.h"
#include "GSScene.h"
#include "vulkan/pipelines/ComputePipeline.h"
#include "vulkan/Swapchain.h"
#include <glm/gtc/quaternion.hpp>
#include <android/asset_manager_jni.h>

#include "GUIManager.h"
#include "vulkan/ImguiManager.h"
#include "vulkan/QueryManager.h"

class Renderer {
public:
    struct alignas(16) UniformBuffer {
        glm::vec4 camera_position;
        glm::mat4 proj_mat;
        glm::mat4 view_mat;
        uint32_t width;
        uint32_t height;
        float tan_fovx;
        float tan_fovy;
    };

    struct VertexAttributeBuffer {
        glm::vec4 conic_opacity;
        glm::vec4 color_radii;
        glm::uvec4 aabb;
        glm::vec2 uv;
        float depth;
        uint32_t __padding[1];
    };

    struct Camera {
        glm::vec3 position;
        glm::quat rotation;
        float fov;
        float nearPlane;
        float farPlane;

        void translate(glm::vec3 translation) {
            position += rotation * translation;
        }
    };

    std::vector<Camera> initialCameraPoses = {
            {
                    .position = glm::vec3(-2.297218, -0.014667, 1.49225),
                    .rotation = glm::quat(-0.184301, 0.557174, 0.293348, -0.754667),
                    .fov = 60.0f,
                    .nearPlane = 0.1f,
                    .farPlane = 1000.0f
            },
            {
                    .position = glm::vec3(0.019871, -2.197124, 4.127104),
                    .rotation = glm::quat(-0.034677, -0.024273, -0.171879, 0.984200),
                    .fov = 60.0f,
                    .nearPlane = 0.1f,
                    .farPlane = 1000.0f
            },
            {
                    .position = glm::vec3(-0.333627, 0.576428, -4.479182),
                    .rotation = glm::quat(-0.008759, 0.999741, -0.018794, -0.008470),
                    .fov = 60.0f,
                    .nearPlane = 0.1f,
                    .farPlane = 1000.0f
            },
    };

    struct RadixSortPushConstants {
        uint32_t g_num_elements; // == NUM_ELEMENTS
        uint32_t g_shift; // (*)
        uint32_t g_num_workgroups; // == NUMBER_OF_WORKGROUPS as defined in the section above
        uint32_t g_num_blocks_per_workgroup; // == NUM_BLOCKS_PER_WORKGROUP
    };

    explicit Renderer(VulkanSplatting::RendererConfiguration& configuration, int scene_path_index);

    void createGui();

    void initialize();

    void handleInput();

    void moveCameraForProfiling();

    void retrieveTimestamps();

    void recreateSwapchain();

    void draw();

    void run();

    void stop();
    
    // Helper functions to get window dimensions
    uint32_t getWindowWidth() const { return swapchain ? swapchain->swapchainExtent.width : 0; }
    uint32_t getWindowHeight() const { return swapchain ? swapchain->swapchainExtent.height : 0; }
    
    // Resolution control for external access
    void setHalfResolution(bool useHalf) {guiManager.useHalfResolution = useHalf; }
    bool isUsingHalfResolution() const { return guiManager.useHalfResolution; }

    void setGui(bool useGui) {
//        guiManager.showMetrics = useGui;
//        showMetrics = useGui;

        // temporary
        switchScene = true;

    }
    bool isUsingGui() const {return guiManager.showMetrics;}

    ~Renderer();

    Camera camera;

    ProfilingMode profilingMode = NONE;
    std::vector<glm::mat3x3> rotations;
    std::vector<glm::vec3> translations;
    int cameraPosIndex = 0;
    AAssetManager * assetManager;
    bool showMetrics = true;
    bool switchScene = false;

    // Touch movement variables
    float lastX = 0.0f;
    float lastY = 0.0f;
    float initialTouchX = 0.0f;
    float initialTouchY = 0.0f;
    float touchDeltaX = 0.0f;
    float touchDeltaY = 0.0f;
    bool isTouching = false;

    // Movement-related variables
    bool doubleTap = false;                             // Set when a double tap occurs
    bool holdTap = false;                               // True when a long press is detected
    std::chrono::steady_clock::time_point lastDownTime; // Time of current ACTION_DOWN
    std::chrono::steady_clock::time_point lastTapTime;  // Time of last tap

private:
    VulkanSplatting::RendererConfiguration configuration;
    std::shared_ptr<Window> window;
    std::shared_ptr<VulkanContext> context;
    std::shared_ptr<ImguiManager> imguiManager;
    std::shared_ptr<GSScene> scene;
    std::shared_ptr<QueryManager> queryManager = std::make_shared<QueryManager>();
    GUIManager guiManager {};

    std::shared_ptr<ComputePipeline> preprocessPipeline;
    std::shared_ptr<ComputePipeline> renderPipeline;
    std::shared_ptr<ComputePipeline> prefixSumPipeline;
    std::shared_ptr<ComputePipeline> preprocessSortPipeline;
    std::shared_ptr<ComputePipeline> sortHistPipeline;
    std::shared_ptr<ComputePipeline> sortPipeline;
    std::shared_ptr<ComputePipeline> tileBoundaryPipeline;

    std::shared_ptr<Buffer> uniformBuffer;
    std::shared_ptr<Buffer> vertexAttributeBuffer;
    std::shared_ptr<Buffer> tileOverlapBuffer;
    std::shared_ptr<Buffer> prefixSumPingBuffer;
    std::shared_ptr<Buffer> prefixSumPongBuffer;
    std::shared_ptr<Buffer> sortKBufferEven;
    std::shared_ptr<Buffer> sortKBufferOdd;
    std::shared_ptr<Buffer> sortHistBuffer;
    std::shared_ptr<Buffer> totalSumBufferHost;
    std::shared_ptr<Buffer> tileBoundaryBuffer;
    std::shared_ptr<Buffer> sortVBufferEven;
    std::shared_ptr<Buffer> sortVBufferOdd;

    std::shared_ptr<DescriptorSet> inputSet;

    std::atomic<bool> running = true;

    std::vector<vk::UniqueFence> inflightFences;

    std::shared_ptr<Swapchain> swapchain;

    vk::UniqueCommandPool commandPool;

    vk::UniqueCommandBuffer preprocessCommandBuffer;
    vk::UniqueCommandBuffer renderCommandBuffer;

    uint32_t currentImageIndex;

    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;

#ifdef __APPLE__
    uint32_t numRadixSortBlocksPerWorkgroup = 256;
#else
    uint32_t numRadixSortBlocksPerWorkgroup = 32;
#endif

    int realFps = 0;
    int realerFps = 0;
    int realestFps = 0;
    int fpsCounter = 0;
    int frame_count = 0;
    int sum = 0;
    std::chrono::high_resolution_clock::time_point lastFpsTime = std::chrono::high_resolution_clock::now();

    unsigned int sortBufferSizeMultiplier = 1;
    
    // Removed half resolution toggle - now using guiManager.useHalfResolution

    // Add these variables for 30-second FPS tracking
    std::chrono::time_point<std::chrono::high_resolution_clock> thirtySecondIntervalStart;
    uint32_t thirtySecondFrameCount = 0;
    float thirtySecondAvgFps = 0.0f;
    bool thirtySecondIntervalStarted = false;

    void initializeVulkan();

    void loadSceneToGPU();

    void createPreprocessPipeline();

    void createPrefixSumPipeline();

    void createRadixSortPipeline();

    void createPreprocessSortPipeline();

    void createTileBoundaryPipeline();

    void createRenderPipeline();

    void writeTimestamp(const std::string &name, vk::UniqueCommandBuffer & buffer);

    void recordPreprocessCommandBuffer();

    bool recordRenderCommandBuffer(uint32_t currentFrame);

    void createCommandPool();

    void updateUniforms();

    static float computePSNR(const std::vector<float>& rendered, const std::vector<float>& groundTruth, float maxValue = 1.0f);

    std::vector<unsigned char> loadAssetBytes(AAssetManager* assetManager, const char* filename);

    std::vector<float> loadGroundTruthImage(AAssetManager* assetManager, const char* filename, int targetWidth, int targetHeight, int req_channels);

    std::vector<float> resizeGroundTruth(const std::vector<float>& groundTruthFloat, int gtWidth, int gtHeight,
                                         int targetWidth, int targetHeight, int channels);

    std::vector<float> retrieveRenderedImage();
};


#endif //RENDERER_H
