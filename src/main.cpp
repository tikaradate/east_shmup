#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

struct QueueFamilyIndices{
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily = 0;

    bool hasGraphicsFamily = false;
    bool hasPresentFamily = false;

    bool isComplete() const {
        return hasGraphicsFamily && hasPresentFamily;
    }
};

struct SwapchainSupportDetails{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

static const char* requiredDeviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static const uint32_t requiredDeviceExtensionCount =
    sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]);

static const bool enableValidationLayers = true;

static const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};

static bool readBinaryFile(const char *path, std::vector<char> *buffer){
    FILE *file = std::fopen(path, "rb");

    if(file == nullptr){
        std::fprintf(stderr, "failed to open file: %s\n", path);
        return false;
    }

    if(std::fseek(file, 0, SEEK_END) != 0){
        std::fprintf(stderr, "failed to seek file: %s\n", path);
        std::fclose(file);
        return false;
    }

    long fileSize = std::ftell(file);

    if(fileSize < 0){
        std::fprintf(stderr, "failed to get file size: %s\n", path);
        std::fclose(file);
        return false;
    }

    std::rewind(file);

    buffer->resize(static_cast<size_t>(fileSize));

    size_t bytesRead = std::fread(
        buffer->data(),
        1,
        buffer->size(),
        file
    );

    std::fclose(file);

    if(bytesRead != buffer->size()){
        std::fprintf(stderr, "failed to read full file: %s\n", path);
        return false;
    }

    return true;
}

static bool createShaderModule(VkDevice device, const char *path, VkShaderModule *shaderModule){
    std::vector<char> code;

    if(!readBinaryFile(path, &code)){
        return false;
    }

    if(code.empty() || code.size() % 4 != 0){
        std::fprintf(stderr, "invalid SPIR-V file: %s\n", path);
        return false;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkResult result = vkCreateShaderModule(
        device,
        &createInfo,
        nullptr,
        shaderModule
    );

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create shader module: %s\n", path);
        return false;
    }

    return true;
}

static bool createFramebuffers(VkDevice device, VkRenderPass renderPass, const std::vector<VkImageView> &swapchainImageViews, VkExtent2D swapchainExtent, std::vector<VkFramebuffer> *swapchainFramebuffers){
    swapchainFramebuffers->resize(swapchainImageViews.size());

    for(size_t i = 0; i < swapchainImageViews.size(); i++){
        VkImageView attachments[] = {
            swapchainImageViews[i]
        };

        VkFramebufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapchainExtent.width;
        createInfo.height = swapchainExtent.height;
        createInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(
            device,
            &createInfo,
            nullptr,
            &(*swapchainFramebuffers)[i]
        );

        if(result != VK_SUCCESS){
            std::fprintf(stderr, "failed to create framebuffer\n");

            for(size_t j = 0; j < i; j++){
                vkDestroyFramebuffer(device, (*swapchainFramebuffers)[j], nullptr);
            }

            swapchainFramebuffers->clear();
            return false;
        }
    }

    return true;
}

static bool createRenderPass(VkDevice device, VkFormat swapchainImageFormat, VkRenderPass *renderPass){
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;

    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;

    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(device, &createInfo, nullptr, renderPass);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create render pass\n");
        return false;
    }

    return true;
}

static bool createSwapchainImageViews(VkDevice device, const std::vector<VkImage> &swapchainImages, VkFormat swapchainImageFormat, std::vector<VkImageView> *swapchainImageViews){
    swapchainImageViews->resize(swapchainImages.size());

    for(uint32_t i = 0; i < swapchainImages.size(); i++){
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat;
    
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device, &createInfo, nullptr, &(*swapchainImageViews)[i]);

        if(result != VK_SUCCESS){
            std::fprintf(stderr, "failed to create swapchain image view\n");
            
            for(uint32_t j = 0; j < i; j++){
                vkDestroyImageView(device, (*swapchainImageViews)[j], nullptr);
            }

            swapchainImageViews->clear();

            return false;
        }
    }

    return true;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window){
    if(capabilities.currentExtent.width != UINT32_MAX){
        return capabilities.currentExtent;
    } 
    int width = 0;
    int height = 0;

    glfwGetFramebufferSize(window, &width, &height);
    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(
        actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width
    );

    actualExtent.height = std::clamp(
        actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height
    );

    return actualExtent;
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes){
    for(const VkPresentModeKHR &presentMode : availablePresentModes){
        if(presentMode == VK_PRESENT_MODE_MAILBOX_KHR){
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats){
    for(const VkSurfaceFormatKHR &surfaceFormat : availableFormats){
        if(surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return surfaceFormat;
        }
    }

    return availableFormats[0];
}

static bool querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface, SwapchainSupportDetails *details){
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

    if(result != VK_SUCCESS){
        return false;
    }

    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if(result != VK_SUCCESS){
        return false;
    }

    details->formats.resize(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details->formats.data());    
    
    if(result != VK_SUCCESS){
        return false;
    }

    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if(result != VK_SUCCESS){
        return false;
    }

    details->presentModes.resize(presentModeCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details->presentModes.data());

    if(result != VK_SUCCESS){
        return false;
    }

    return true;
}

static bool createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, GLFWwindow *window, QueueFamilyIndices queueFamilyIndices, VkSwapchainKHR *swapchain, std::vector<VkImage> *swapchainImages, VkFormat *swapchainImageFormat, VkExtent2D *swapchainExtent){
    SwapchainSupportDetails swapchainSupport = {};

    if(!querySwapchainSupport(physicalDevice, surface, &swapchainSupport)){
        return false;
    }

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);

    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);

    VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities, window);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;

    if(swapchainSupport.capabilities.maxImageCount > 0 &&
       imageCount > swapchainSupport.capabilities.maxImageCount){
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndicesArray[] = {queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily};

    if(queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily){
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;

    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, swapchain);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "failed to create swapchain\n");
        return false;
    }

    uint32_t actualImageCount = 0;

    VkResult imageResult = vkGetSwapchainImagesKHR(device, *swapchain, &actualImageCount, nullptr);

    if(imageResult != VK_SUCCESS){
        std::fprintf(stderr, "failed to get swapchain image count\n");
        return false;
    }

    swapchainImages->resize(actualImageCount);

    imageResult = vkGetSwapchainImagesKHR(device, *swapchain, &actualImageCount, swapchainImages->data());

    if(imageResult != VK_SUCCESS){
        std::fprintf(stderr, "failed to get swapchain images\n");
        return false;
    }

    *swapchainImageFormat = surfaceFormat.format;
    *swapchainExtent = extent;
    
    return true;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice device){
    uint32_t extensionCount = 0;

    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extensionCount,
        nullptr
    );

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);

    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extensionCount,
        availableExtensions.data()
    );

    for(uint32_t i = 0; i < requiredDeviceExtensionCount; i++){
        bool found = false;

        for(uint32_t j = 0; j < extensionCount; j++){
            if(std::strcmp(
                requiredDeviceExtensions[i],
                availableExtensions[j].extensionName
            ) == 0){
                found = true;
                break;
            }
        }

        if(!found){
            return false;
        }
    }

    return true;
}

static bool createLogicalDevice(VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilyIndices, VkDevice *device, VkQueue *graphicsQueue, VkQueue *presentQueue){
    float queuePriority = 1.0f;
    
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {};
    uint32_t queueCreateInfoCount = 0;

    queueCreateInfos[queueCreateInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[queueCreateInfoCount].queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    queueCreateInfos[queueCreateInfoCount].queueCount = 1;
    queueCreateInfos[queueCreateInfoCount].pQueuePriorities = &queuePriority;

    queueCreateInfoCount++;

    if(queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily){
        queueCreateInfos[queueCreateInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[queueCreateInfoCount].queueFamilyIndex = queueFamilyIndices.presentFamily;
        queueCreateInfos[queueCreateInfoCount].queueCount = 1;
        queueCreateInfos[queueCreateInfoCount].pQueuePriorities = &queuePriority;

        queueCreateInfoCount++;
    }
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCreateInfoCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = requiredDeviceExtensionCount;
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    
    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, device);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create logical device\n");
        return false;
    }
        std::fprintf(stdout, "created logical device\n");

    vkGetDeviceQueue(*device, queueFamilyIndices.graphicsFamily, 0, graphicsQueue);

    if(queueFamilyIndices.presentFamily == queueFamilyIndices.graphicsFamily){
        *presentQueue = *graphicsQueue;
    } else {
        vkGetDeviceQueue(*device, queueFamilyIndices.presentFamily, 0, presentQueue);
    }

    return true;
}

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface){
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for(uint32_t i = 0; i < queueFamilyCount; i++){
        if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){
            indices.graphicsFamily = i;
            indices.hasGraphicsFamily = true;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if(presentSupport){
            indices.presentFamily = i;
            indices.hasPresentFamily = true;
        }

        if(indices.isComplete()){
            break;
        }
    }

    return indices;
}

static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface){
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapchainAdequate = false;
    if(extensionsSupported){
        SwapchainSupportDetails swapchainSupport;
        if(querySwapchainSupport(device, surface, &swapchainSupport)){
            swapchainAdequate =
                !swapchainSupport.formats.empty()
                && !swapchainSupport.presentModes.empty();
        }
    }

    return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

static bool pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice *device, QueueFamilyIndices *queueFamilyIndices){
    uint32_t physicalCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalCount, nullptr);

    if((physicalCount == 0 ) || (result != VK_SUCCESS)) {
        return false;
    }

    
    std::vector<VkPhysicalDevice> physicalDevices(physicalCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalCount, physicalDevices.data());
    
    if(result != VK_SUCCESS){
        return false;
    }
    
    
    for(const VkPhysicalDevice &current_device : physicalDevices){
        if(isDeviceSuitable(current_device, surface)){
            *device = current_device;
            *queueFamilyIndices = findQueueFamilies(*device, surface);
            return true;
        }
    }
    
    return false;
}

static void glfw_error_callback(int error, const char *description){
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

bool checkValidationLayerSupport(){
    uint32_t layerCount = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    if(result != VK_SUCCESS){
        return false;
    }

    std::vector<VkLayerProperties> availableLayers(layerCount);

    result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    if(result != VK_SUCCESS){
        return false;
    }

    for(const VkLayerProperties &layer : availableLayers){
        if(std::strcmp(layer.layerName, validationLayers[0]) == 0){
            return true;
        }
    }

    return false;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
){
    std::fprintf(stderr, "%s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

static bool createSurface(VkInstance instance, GLFWwindow *window, VkSurfaceKHR *surface){
    VkResult surfaceResult = glfwCreateWindowSurface(instance, window, nullptr, surface);

    if(surfaceResult != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create window surface: VkResult %d\n", surfaceResult);

        return false;
    }

    return true;
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT *createInfo){
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pfnUserCallback = debugCallback;
    createInfo->pUserData = nullptr;
}

static bool setupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT *debugMessenger){
    if(!enableValidationLayers) {
        return true;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(&createInfo);

    PFN_vkCreateDebugUtilsMessengerEXT func = 
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
        );

    if(func == nullptr){
        std::fprintf(stderr, "Failed to load vkCreateDebugUtilsMessengerEXT\n");
        return false;
    }

    VkResult result = func(instance, &createInfo, nullptr, debugMessenger);

    if(result != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create debug messenger: VkResult %d\n", result);
        return false;
    }

    return true;
}

static void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT *debugMessenger){
    if(!enableValidationLayers) {
        return;
    }

    if(*debugMessenger == VK_NULL_HANDLE){
        return;
    }

    PFN_vkDestroyDebugUtilsMessengerEXT func = 
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
        );

    if(func == nullptr){
        std::fprintf(stderr, "Failed to load vkDestroyDebugUtilsMessengerEXT\n");
        return;
    } else {
        func(instance, *debugMessenger, nullptr);
    }
}

bool createInstance(VkInstance *instance) {
    if(enableValidationLayers && !checkValidationLayerSupport()){
        std::fprintf(stderr, "Validation layers requested, but not available\n");
        return false;
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "East Shmup";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "East Shmup Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t extensionCount = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

    if (extensions == nullptr || extensionCount == 0) {
        std::fprintf(stderr, "GLFW did not return required Vulkan extensions\n");
        return false;
    }

    std::vector<const char*> enabledExtensions;
    for(uint32_t i = 0; i < extensionCount; i++){
        enabledExtensions.push_back(extensions[i]);
    }
    if(enableValidationLayers){
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    if(enableValidationLayers){
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, instance);

    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "Failed to create Vulkan instance: VkResult %d\n", result);
        return false;
    }

    return true;
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(
        1280,
        720,
        "East Shmup",
        nullptr,
        nullptr
    );

    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    VkInstance instance = VK_NULL_HANDLE;

    if (!createInstance(&instance)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Vulkan instance created\n");

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    setupDebugMessenger(instance, &debugMessenger);

    std::fprintf(stdout, "DebugMessenger created\n");

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    
    if(!createSurface(instance, window, &surface)){
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Surface created\n");

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices;

    if(!pickPhysicalDevice(instance, surface, &physicalDevice, &queueFamilyIndices)){
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Vulkan physical device selected\n");
    std::fprintf(stdout, "graphics family: %d\n", queueFamilyIndices.graphicsFamily);
    std::fprintf(stdout, "present family: %d\n", queueFamilyIndices.presentFamily);


    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    if(!createLogicalDevice(physicalDevice, queueFamilyIndices, &device, &graphicsQueue, &presentQueue)){
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Vulkan logical device created\n");

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;

    if(!createSwapchain(physicalDevice, device, surface, window, queueFamilyIndices, &swapchain, &swapchainImages, &swapchainImageFormat, &swapchainExtent)){
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Swapchain created\n");
    std::fprintf(stdout, "swapchain image count: %zu\n", swapchainImages.size());
    
    std::vector<VkImageView> swapchainImageViews;
    if(!createSwapchainImageViews(device, swapchainImages, swapchainImageFormat, &swapchainImageViews)){
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;        
    }
    
    std::fprintf(stdout, "Swapchain image views created\n");

    VkRenderPass renderPass = VK_NULL_HANDLE;

    if(!createRenderPass(device, swapchainImageFormat, &renderPass)){
        for(size_t i = 0; i < swapchainImageViews.size(); i++){
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Render pass created\n");

    std::vector<VkFramebuffer> swapchainFramebuffers;

    if(!createFramebuffers(device, renderPass, swapchainImageViews, swapchainExtent, &swapchainFramebuffers)){
        vkDestroyRenderPass(device, renderPass, nullptr);

        for(size_t i = 0; i < swapchainImageViews.size(); i++){
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Framebuffers created\n");

    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;

    if(!createShaderModule(device, "src/shaders/triangle.vert.spv", &vertShaderModule)){
        for(size_t i = 0; i < swapchainFramebuffers.size(); i++){
            vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
        }

        vkDestroyRenderPass(device, renderPass, nullptr);

        for(size_t i = 0; i < swapchainImageViews.size(); i++){
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    if(!createShaderModule(device, "src/shaders/triangle.frag.spv", &fragShaderModule)){
        vkDestroyShaderModule(device, vertShaderModule, nullptr);

        for(size_t i = 0; i < swapchainFramebuffers.size(); i++){
            vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
        }

        vkDestroyRenderPass(device, renderPass, nullptr);

        for(size_t i = 0; i < swapchainImageViews.size(); i++){
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "Shader modules created\n");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    std::fprintf(stdout, "Shader modules destroyed\n");

    for(size_t i = 0; i < swapchainFramebuffers.size(); i++){
        vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
    }

    std::fprintf(stdout, "Framebuffers destroyed\n");

    vkDestroyRenderPass(device, renderPass, nullptr);
    std::fprintf(stdout, "Render pass destroyed\n");

    for(size_t i = 0; i < swapchainImageViews.size(); i++){
       vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
    std::fprintf(stdout, "Swapchain image views destroyed\n");

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    std::fprintf(stdout, "Swapchain destroyed\n");

    vkDestroyDevice(device, nullptr);
    std::fprintf(stdout, "Device destroyed\n");

    vkDestroySurfaceKHR(instance, surface, nullptr);
    std::fprintf(stdout, "Surface destroyed\n");
    
    destroyDebugMessenger(instance, &debugMessenger);
    std::fprintf(stdout, "DebugMessenger destroyed\n");
    
    vkDestroyInstance(instance, nullptr);
    std::fprintf(stdout, "Vulkan instance destroyed\n");

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
