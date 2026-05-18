#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstdio>

static void glfw_error_callback(int error, const char *description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

bool createInstance(VkInstance *instance) {
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

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledLayerCount = 0;

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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    vkDestroyInstance(instance, nullptr);
    std::fprintf(stdout, "Vulkan instance destroyed\n");

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
