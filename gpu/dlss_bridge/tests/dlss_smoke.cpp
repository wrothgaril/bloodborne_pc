// SPDX-License-Identifier: MIT
// Exercises the real bridge/NGX/Vulkan lifecycle on an RTX GPU without game files.
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include "../bbport_dlss_bridge.h"

void Check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "%s: Vulkan result %d\n", operation, result);
        throw std::runtime_error(operation);
    }
}
void Require(bool result, const char* operation) {
    if (!result)
        throw std::runtime_error(operation);
}
void Log(int warning, const char* message) {
    std::printf("%s%s\n", warning ? "warning: " : "", message);
}

int main() {
    HMODULE library{};
    const BbDlssApi* api{};
    VkInstance instance{};
    VkDevice device{};
    VkCommandPool pool{};
    int result = 1;
    try {
        wchar_t exe[32768]{};
        Require(GetModuleFileNameW(nullptr, exe, 32768) != 0, "Executable path");
        const auto directory = std::filesystem::path{exe}.parent_path();
        const auto data = directory / "smoke-data";
        std::filesystem::create_directories(data);
        library = LoadLibraryW((directory / "bbport_dlss.dll").c_str());
        Require(library != nullptr, "Load bridge");
        const auto get_api = reinterpret_cast<BbDlssGetApiFn>(GetProcAddress(library, "BbDlssGetApi"));
        Require(get_api != nullptr, "Resolve ABI entry point");
        api = get_api();
        Require(api && api->abi == BBPORT_DLSS_BRIDGE_ABI, "Bridge ABI");
        Require(api->Configure(directory.c_str(), data.c_str(), Log) != 0, "Configure");
        uint32_t count{};
        const VkExtensionProperties* properties{};
        Require(api->InstanceExtensions(&count, &properties) != 0, "Instance extensions");
        std::vector<const char*> names;
        for (uint32_t i = 0; i < count; ++i)
            names.push_back(properties[i].extensionName);
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "bbport DLSS smoke";
        app.apiVersion = VK_API_VERSION_1_3;
        VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance_info.pApplicationInfo = &app;
        instance_info.enabledExtensionCount = static_cast<uint32_t>(names.size());
        instance_info.ppEnabledExtensionNames = names.data();
        Check(vkCreateInstance(&instance_info, nullptr, &instance), "Create instance");
        Check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "GPU count");
        std::vector<VkPhysicalDevice> devices(count);
        Check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "Enumerate GPUs");
        VkPhysicalDevice physical{};
        for (auto candidate : devices) {
            VkPhysicalDeviceProperties info{};
            vkGetPhysicalDeviceProperties(candidate, &info);
            if (info.vendorID == 0x10de) {
                physical = candidate;
                std::printf("GPU: %s\n", info.deviceName);
                break;
            }
        }
        Require(physical != VK_NULL_HANDLE, "NVIDIA GPU required");
        Require(api->DeviceExtensions(instance, physical, &count, &properties) != 0,
                "Device extensions and DLSS support");
        names.clear();
        for (uint32_t i = 0; i < count; ++i)
            names.push_back(properties[i].extensionName);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
        uint32_t family = 0;
        while (family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            ++family;
        Require(family < count, "Graphics queue required");
        const float priority = 1.f;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = static_cast<uint32_t>(names.size());
        device_info.ppEnabledExtensionNames = names.data();
        Check(vkCreateDevice(physical, &device_info, nullptr, &device), "Create device");
        Require(api->Initialize(instance, physical, device, vkGetInstanceProcAddr,
                                vkGetDeviceProcAddr) != 0, "NGX initialization");
        VkQueue queue{};
        vkGetDeviceQueue(device, family, 0, &queue);
        VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = family;
        Check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "Create command pool");
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        VkCommandBuffer command{};
        Check(vkAllocateCommandBuffers(device, &allocate, &command), "Allocate command buffer");
        const uint32_t widths[]{1920, 1280, 1114, 960, 640};
        const uint32_t heights[]{1080, 720, 626, 540, 360};
        for (int quality = 0; quality < 5; ++quality) {
            Check(vkResetCommandBuffer(command, 0), "Reset command buffer");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            Check(vkBeginCommandBuffer(command, &begin), "Begin command buffer");
            const BbDlssFeature desc{widths[quality], heights[quality], 1920, 1080,
                                      quality, 0, 0, 0};
            Require(api->CreateFeature(command, &desc) != 0, "DLSS feature creation");
            Check(vkEndCommandBuffer(command), "End command buffer");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command;
            Check(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "Submit feature creation");
            Check(vkQueueWaitIdle(queue), "Wait for GPU");
            api->ReleaseFeature();
        }
        // Exercise shutdown/reinitialization as well as one-shot initialization.
        api->Shutdown();
        Require(api->Initialize(instance, physical, device, vkGetInstanceProcAddr,
                                vkGetDeviceProcAddr) != 0, "NGX reinitialization");
        std::puts("PASS: ABI, Vulkan extensions, NGX initialization, all five quality modes, reinitialization");
        result = 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
    }
    if (device)
        vkDeviceWaitIdle(device);
    if (api)
        api->Shutdown();
    if (pool)
        vkDestroyCommandPool(device, pool, nullptr);
    if (device)
        vkDestroyDevice(device, nullptr);
    if (instance)
        vkDestroyInstance(instance, nullptr);
    if (library)
        FreeLibrary(library);
    return result;
}
