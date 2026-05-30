#include "vulkan_app.h"

#include <cstdlib>
#include <cstdio>

static void glfw_error_callback(int error, const char *description){
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

int main(){
    int exitCode = EXIT_FAILURE;
    bool glfwInitialized = false;

    GLFWwindow *window = nullptr;
    VulkanApp app = {};

    glfwSetErrorCallback(glfw_error_callback);

    if(!glfwInit()){
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        goto cleanup;
    }

    glfwInitialized = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(
        1280,
        720,
        "East Shmup",
        nullptr,
        nullptr
    );

    if(window == nullptr){
        std::fprintf(stderr, "Failed to create GLFW window\n");
        goto cleanup;
    }

    if(!initVulkan(&app, window)){
        goto cleanup;
    }

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        if(!drawFrame(&app)){
            std::fprintf(stderr, "Failed to draw frame\n");
            goto cleanup;
        }

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    exitCode = EXIT_SUCCESS;

cleanup:
    cleanupVulkan(&app);

    if(window != nullptr){
        glfwDestroyWindow(window);
    }

    if(glfwInitialized){
        glfwTerminate();
    }

    return exitCode;
}