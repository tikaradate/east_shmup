cmake_minimum_required(VERSION 3.20)

project(shmup LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 REQUIRED)

add_executable(shmup
    src/main.cpp
)

target_link_libraries(shmup
    PRIVATE glfw
)

target_compile_options(shmup PRIVATE
    -Wall -Wextra -Wpedantic
)