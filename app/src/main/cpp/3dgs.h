#ifndef VULKANSPLATTING_H
#define VULKANSPLATTING_H

#include <optional>
#include <string>
#include <memory>
#include <vector>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <glm/gtc/quaternion.hpp>
#include <android/asset_manager_jni.h>

#include "base_utils.h"

class Window;
class Renderer;

class VulkanSplatting {
public:
    struct RendererConfiguration {
        bool enableVulkanValidationLayers = false;
        std::optional<uint8_t> physicalDeviceId = std::nullopt;
        bool immediateSwapchain = false;
        std::string assetContent;

        float fov = 45.0f;
        float cameraNear = 0.2f;
        float cameraFar = 1000.0f;
        bool enableGui = false;

        ProfilingMode profilingMode = NONE;
        std::vector<glm::mat3x3> rotations;
        std::vector<glm::vec3> translations;
        AAssetManager * assetManager;

        std::shared_ptr<Window> window;
    };

    explicit VulkanSplatting(RendererConfiguration configuration) : configuration(configuration) {}

#ifdef VKGS_ENABLE_GLFW
    static std::shared_ptr<Window> createGlfwWindow(std::string name, int width, int height);
#endif

#ifdef VKGS_ENABLE_ANDROID
    static std::shared_ptr<Window> createAndroidWindow(ANativeWindow * window, int width, int height);
#endif

#ifdef VKGS_ENABLE_METAL
    static std::shared_ptr<Window> createMetalWindow(void *caMetalLayer, int width, int height);
#endif

    void start(int scene_path_index);

    void initialize(int scene_path_index);

    void run();

    void draw();

    void logTranslation(float x, float y);

    void logMovement(float x, float y, float z);

    void stop();

    Renderer* getRenderer() { return renderer.get(); }    // marciric

    RendererConfiguration configuration;

private:
    std::shared_ptr<Renderer> renderer;
};

#endif //VULKANSPLATTING_H