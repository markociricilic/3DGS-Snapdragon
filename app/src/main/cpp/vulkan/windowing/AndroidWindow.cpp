#include "AndroidWindow.h"
#include <android/native_window.h>
#include <android/native_activity.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <stdexcept>
#include <vector>
#include <glm/ext/matrix_transform.hpp>

#include "../../base_utils.h"

AndroidWindow::AndroidWindow (ANativeWindow * window, int width, int height){
    this->window = window;
}

VkSurfaceKHR AndroidWindow::createSurface(std::shared_ptr<VulkanContext> context) {
    LOGD("Creating Surface");
    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = window;
    VkResult result = vkCreateAndroidSurfaceKHR(context->instance.get(), &createInfo, nullptr, &surface);
    if (result != VK_SUCCESS) {
        LOGD("Failed to create surface");
        throw std::runtime_error("Failed to create Vulkan surface!");
    }

    return surface;
}

std::pair<uint32_t, uint32_t> AndroidWindow::getFramebufferSize() const {
    if (!window) {
        return {0, 0};
    }
    return {ANativeWindow_getWidth(window), ANativeWindow_getHeight(window)};
}

std::vector<std::string> AndroidWindow::getRequiredInstanceExtensions() {
    return {"VK_KHR_surface", "VK_KHR_android_surface"};
}

// Placeholder for input handling
std::array<bool, 7> AndroidWindow::getKeys() {
    // Implement custom input handling via AInputQueue or Java interop
    return {false, false, false, false, false, false, false};
}

bool AndroidWindow::tick() {
    // Poll input events
//    if (inputQueue) {
//        AInputEvent *event = nullptr;
//        while (AInputQueue_getEvent(inputQueue, &event) >= 0) {
//            if (AInputQueue_preDispatchEvent(inputQueue, event)) {
//                continue;
//            }
//
//            int32_t handled = handleInputEvent(event);
//            AInputQueue_finishEvent(inputQueue, event, handled);
//        }
//    }
//
//    // Check if the app should continue running
//    return running;
//        LOGD("TICKING");
        return true;
}