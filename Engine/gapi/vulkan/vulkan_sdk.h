#pragma once

#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#include <vulkan/vulkan.hpp>
