#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>


static const bool enableValidationLayers = true;

static const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};

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

    if(debugMessenger == VK_NULL_HANDLE){
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

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkResult surfaceResult = glfwCreateWindowSurface(instance, window, nullptr, &surface);

    if(surfaceResult != VK_SUCCESS){
        std::fprintf(stderr, "Failed to create window surface: VkResult %d\n", surfaceResult);

        destroyDebugMessenger(instance, &debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        return EXIT_FAILURE;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    destroyDebugMessenger(instance, &debugMessenger);
    vkDestroyInstance(instance, nullptr);
    std::fprintf(stdout, "Vulkan instance destroyed\n");

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
