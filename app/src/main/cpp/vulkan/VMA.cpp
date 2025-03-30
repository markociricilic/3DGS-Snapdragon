#include <cstdio>
#include <vulkan/vulkan.h>

// undefine VK_VERSION_1_3 https://github.com/KhronosGroup/MoltenVK/issues/1708
#undef VK_VERSION_1_3
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"