#ifndef GAUSSIAN_SPLATTING_ANDROIDWINDOW_H
#define GAUSSIAN_SPLATTING_ANDROIDWINDOW_H

#include "../Window.h"

class AndroidWindow final : public Window {
public:
    AndroidWindow(ANativeWindow * window, int width, int height);

    VkSurfaceKHR createSurface(std::shared_ptr<VulkanContext> context) override;

//    std::array<bool, 3> getMouseButton() override;

    std::vector<std::string> getRequiredInstanceExtensions() override;

    [[nodiscard]] std::pair<uint32_t, uint32_t> getFramebufferSize() const override;

//    std::array<double, 2> getCursorTranslation() override;

    std::array<bool, 7> getKeys() override;

//    void mouseCapture(bool capture) override;

    bool tick() override;

    ANativeWindow * window;

private:
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    double lastX = 0.0;
    double lastY = 0.0;
};

#endif GAUSSIAN_SPLATTING_ANDROIDWINDOW_H
