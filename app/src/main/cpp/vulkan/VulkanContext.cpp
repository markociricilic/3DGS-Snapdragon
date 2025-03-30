#include "VulkanContext.h"
#include <iostream>
#include <set>
#include <unordered_map>

#include "../3dgs.h"
#include "Utils.h"
#include <spdlog/spdlog.h>

#include "../base_utils.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    const char* type = "???";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type = "GENERAL";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type = "VALIDATION";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type = "PERFORMANCE";
    }

    const char* severity = "???";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        LOGD("[%s]: %s",  type, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LOGD("[%s]: %s", type, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOGD("[%s]: %s", type, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOGD("[%s]: %s", type, pCallbackData->pMessage);
    } else {
        LOGD("Debug callback, type not recognized");
    }

    return VK_FALSE;
}

VulkanContext::VulkanContext(const std::vector<std::string>& instance_extensions,
                             const std::vector<std::string>& device_extensions, bool validation_layers_enabled)
    : instanceExtensions(instance_extensions), deviceExtensions(device_extensions),
      validationLayersEnabled(validation_layers_enabled) {
#ifdef __APPLE__
    #ifndef VKGS_ENABLE_METAL
        instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        deviceExtensions.push_back("VK_KHR_portability_subset");
    #endif
#endif
    deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

    if (validation_layers_enabled) {
        deviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

#ifndef VKGS_ENABLE_METAL
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
#else
    void *libvulkan = dlopen("MoltenVK.framework/MoltenVK", RTLD_NOW | RTLD_LOCAL);
    if (!libvulkan) {
        throw std::runtime_error("MoltenVK not found");
    }
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(libvulkan, "vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
#endif
}

void VulkanContext::createInstance() {
    LOGD("Creating new Instance");
    vk::ApplicationInfo appInfo = {
        "Vulkan Splatting", VK_MAKE_VERSION(1, 0, 0), "No Engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3
    };

    std::vector<const char *> requiredLayers;
    if (validationLayersEnabled) {
        requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    auto instanceExtensionsCharPtr = Utils::stringVectorToCharPtrVector(instanceExtensions);

    vk::DebugUtilsMessengerCreateInfoEXT debugInfo = {
            {},
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            debugCallback
    };
    vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> createInfoChain = {
        {
            {}, &appInfo, (uint32_t) requiredLayers.size(), requiredLayers.data(), (uint32_t) instanceExtensions.size(),
            instanceExtensionsCharPtr.data()
        },
        debugInfo
    };

#ifdef __APPLE__
    createInfoChain.get<vk::InstanceCreateInfo>().flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // added for debugging
    // Check if desired layers are in the available list
    for (const char* layerName : requiredLayers) {
        bool layerFound = false;
        for (const auto& layer : availableLayers) {
            if (std::strcmp(layerName, layer.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            LOGD("LAYER %s not found", layerName);
            assert(0);
        }
    }

    if (!validationLayersEnabled) {
        createInfoChain.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
    }

    instance = vk::createInstanceUnique(createInfoChain.get<vk::InstanceCreateInfo>());

    // I tried to get the shader printf working, but idk how
//    vk::DebugUtilsMessengerEXT debugMessenger;
//    debugMessenger = instance->createDebugUtilsMessengerEXT(debugInfo, nullptr);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    LOGD("Vulkan instance created");
}

bool VulkanContext::isDeviceSuitable(vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface) {
    auto properties = device.getProperties();
    auto features = device.getFeatures();

    auto supportedExtensions = device.enumerateDeviceExtensionProperties();
    for (auto& extension: deviceExtensions) {
        if (std::find_if(supportedExtensions.begin(), supportedExtensions.end(),
                         [&extension](const vk::ExtensionProperties& supportedExtension) {
                             return strcmp(extension.c_str(), supportedExtension.extensionName) == 0;
                         }) == supportedExtensions.end()) {
            return false;
        }
    }

    if (surface.has_value()) {
        auto surfaceCapabilities = device.getSurfaceCapabilitiesKHR(surface.value());
        auto surfaceFormats = device.getSurfaceFormatsKHR(surface.value());
        auto presentModes = device.getSurfacePresentModesKHR(surface.value());

        if (surfaceFormats.empty() || presentModes.empty()) {
            return false;
        }
    }

    return true;
}

void VulkanContext::selectPhysicalDevice(std::optional<uint8_t> id, std::optional<vk::SurfaceKHR> surface) {
    if (surface.has_value()) {
        vk::UniqueSurfaceKHR surfaceUnique{surface.value(), *instance};
        this->surface = std::make_optional(std::move(surfaceUnique));
    }
    auto devices = instance->enumeratePhysicalDevices();


    LOGD("Available physical devices:");
    for (auto& device: devices) {
        LOGD("%s", device.getProperties().deviceName);
    }

    if (surface.has_value()) {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    if (id.has_value()) {
        if (devices.size() <= id.value()) {
            throw std::runtime_error("Invalid physical device id");
        }
        physicalDevice = devices[id.value()];
        LOGD("Selected physical device (by index): %s", physicalDevice.getProperties().deviceName);
        return;
    }

    auto suitableDevices = std::vector<vk::PhysicalDevice>{};
    for (auto& device: devices) {
        if (isDeviceSuitable(device, surface)) {
            suitableDevices.push_back(device);
        }
    }

    physicalDevice = suitableDevices[0];
    for (auto& device: suitableDevices) {
        auto properties = device.getProperties();
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            physicalDevice = device;
            break;
        }
    }

    // Use this to find the features/properties the hardware supports
    vk::PhysicalDeviceFeatures features{};
    physicalDevice.getFeatures(&features);

    LOGD("features %i", features.robustBufferAccess);

    vk::PhysicalDeviceVulkan12Features supportedFeatures12{};
    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &supportedFeatures12;
    physicalDevice.getFeatures2(&features2);


    auto properties = physicalDevice.getProperties();
//    LOGD("Selected physical device (automatically): %s. API Version: %d.%d.%d", properties.deviceName,
//         VK_VERSION_MAJOR(properties.apiVersion),
//         VK_VERSION_MINOR(properties.apiVersion),
//         VK_VERSION_PATCH(properties.apiVersion)
//    );
}

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies() {
    QueueFamilyIndices indices;
    auto queueFamilies = physicalDevice.getQueueFamilyProperties();

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        auto& queueFamily = queueFamilies[i];
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }
        if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) {
            indices.computeFamily = i;
        }
        if (surface.has_value()) {
            if (physicalDevice.getSurfaceSupportKHR(i, *surface.value())) {
                indices.presentFamily = i;
            }
        }
        if (indices.isComplete()) {
            break;
        }
    }
    return indices;
}

void VulkanContext::createQueryPool() {
    vk::QueryPoolCreateInfo queryPoolCreateInfo = {};
    queryPoolCreateInfo.queryType = vk::QueryType::eTimestamp;
    queryPoolCreateInfo.queryCount = 20;
    queryPool = device->createQueryPoolUnique(queryPoolCreateInfo);

    auto commandBuffer = beginOneTimeCommandBuffer();
    commandBuffer->resetQueryPool(queryPool.get(), 0, 12);
    endOneTimeCommandBuffer(std::move(commandBuffer), Queue::GRAPHICS);
}

void VulkanContext::createLogicalDevice(vk::PhysicalDeviceFeatures deviceFeatures,
                                        vk::PhysicalDeviceVulkan11Features deviceFeatures11,
                                        vk::PhysicalDeviceVulkan12Features deviceFeatures12) {
    QueueFamilyIndices indices = findQueueFamilies();
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(), indices.computeFamily.value(),
        indices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (auto queueFamily: uniqueQueueFamilies) {
        queueCreateInfos.push_back({{}, queueFamily, 1, &queuePriority});
    }

    deviceFeatures.samplerAnisotropy = VK_TRUE;

    auto deviceExtensionsCharPtr = Utils::stringVectorToCharPtrVector(deviceExtensions);

    vk::DeviceCreateInfo createInfo = {
        {}, (uint32_t) queueCreateInfos.size(), queueCreateInfos.data(), 0, nullptr,
        (uint32_t) deviceExtensionsCharPtr.size(), deviceExtensionsCharPtr.data(), &deviceFeatures
    };
    createInfo.pNext = &deviceFeatures11;
    deviceFeatures11.pNext = &deviceFeatures12;

    vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures {true};
    deviceFeatures12.pNext = &dynamicRenderingFeatures;

    device = physicalDevice.createDeviceUnique(createInfo);

    for (auto unique_queue_family: uniqueQueueFamilies) {
        auto queue = device->getQueue(unique_queue_family, 0);
        std::set<Queue::Type> types;
        if (unique_queue_family == indices.graphicsFamily.value()) {
            types.insert(Queue::Type::GRAPHICS);
        }
        if (unique_queue_family == indices.computeFamily.value()) {
            types.insert(Queue::Type::COMPUTE);
        }
        if (unique_queue_family == indices.presentFamily.value()) {
            types.insert(Queue::Type::PRESENT);
        }

        for (auto type: types) {
            queues[type] = Queue{types, unique_queue_family, 0, queue};
        }
    }

    LOGD("Logical device created");

    // Create VMA
    setupVma();
    createCommandPool();
    createQueryPool();
}

vk::UniqueCommandBuffer VulkanContext::beginOneTimeCommandBuffer() {
    auto info = vk::CommandBufferAllocateInfo()
            .setCommandPool(*commandPool)
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1);
    auto commandBuffer = std::move(device->allocateCommandBuffersUnique(info)[0]);

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    commandBuffer->begin(beginInfo);
    return commandBuffer;
}

void VulkanContext::endOneTimeCommandBuffer(vk::UniqueCommandBuffer&& commandBuffer, Queue::Type queue) {
    commandBuffer->end();
    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;
    queues[queue].queue.submit(submitInfo, nullptr);
    queues[queue].queue.waitIdle();
}

void VulkanContext::setupVma() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = *device;
    allocatorInfo.instance = *instance;
    // allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanContext::createCommandPool() {
    auto queueFamilyIndices = findQueueFamilies();
    vk::CommandPoolCreateInfo poolInfo = {};
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;

    commandPool = device->createCommandPoolUnique(poolInfo);
}

void VulkanContext::createDescriptorPool(uint8_t framesInFlight) {
    // get max number of descriptor sets from physical device
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(framesInFlight * 10)},
        {vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(framesInFlight * 50)},
        {vk::DescriptorType::eStorageImage, static_cast<uint32_t>(framesInFlight * 10)}
    };

    vk::DescriptorPoolCreateInfo poolInfo{
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        100, static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data()
    };

    descriptorPool = device->createDescriptorPoolUnique(poolInfo);
}

VulkanContext::~VulkanContext() {
    vmaDestroyAllocator(allocator);
}
