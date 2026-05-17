#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>

static void glfw_error_callback(int error, const char *description){
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

int main(){
    glfwSetErrorCallback(glfw_error_callback);

    if(!glfwInit()){
        std::cerr << "Failed to initialized GLFW\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        "East Shmup",
        nullptr,
        nullptr
    );

    if(!window){
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}