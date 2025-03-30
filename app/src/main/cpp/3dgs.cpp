#include "3dgs.h"
#include "Renderer.h"

#ifdef VKGS_ENABLE_GLFW
#include "vulkan/windowing/GLFWWindow.h"
std::shared_ptr<Window> VulkanSplatting::createGlfwWindow(std::string name, int width, int height) {
    return std::make_shared<GLFWWindow>(name, width, height);
}
#endif

#ifdef VKGS_ENABLE_ANDROID
#include "vulkan/windowing/AndroidWindow.h"
std::shared_ptr<Window> VulkanSplatting::createAndroidWindow(ANativeWindow * window, int width, int height) {
    return std::make_shared<AndroidWindow>(window, width, height);
}
#endif

#ifdef VKGS_ENABLE_METAL
#include "vulkan/windowing/MetalWindow.h"
std::shared_ptr<Window> VulkanSplatting::createMetalWindow(void *caMetalLayer, int width, int height) {
    return std::make_shared<MetalWindow>(caMetalLayer, width, height);
}
#endif

void VulkanSplatting::start(int scene_path_index) {
    // Create the renderer
    renderer = std::make_shared<Renderer>(configuration, scene_path_index);
    renderer->initialize();
    renderer->run();
}

void VulkanSplatting::initialize(int scene_path_index) {
    renderer = std::make_shared<Renderer>(configuration, scene_path_index);
    renderer->initialize();
}

void VulkanSplatting::run() {
    LOGD("Running");
    renderer->run();
}

void VulkanSplatting::draw() {
    renderer->draw();
}

void VulkanSplatting::logTranslation(float x, float y) {
    configuration.window->logTranslation(x, y);
}

void VulkanSplatting::logMovement(float x, float y, float z) {
    renderer->camera.translate(glm::vec3(x, y, z));
}

void VulkanSplatting::stop() {
    renderer->stop();
}