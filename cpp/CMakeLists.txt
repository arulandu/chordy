cmake_minimum_required(VERSION 3.20)
project(chordy
    VERSION 0.1.0
    DESCRIPTION "A chord detection software"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

file(GLOB_RECURSE SOURCES ./src/*.cpp) # includes ImGui/ImPlot/KissFFT sources
add_executable(${PROJECT_NAME} ${SOURCES})

# GLFW3 + OpenGL
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} glfw OpenGL::GL)

# ImGui + ImPlot + KissFFT
include_directories(
    ./include
    ./include/vendor/imgui
    ./include/vendor/imgui/backends
    ./include/vendor/implot
    ./include/vendor/kissfft
)

# PortAudio
FetchContent_Declare(portaudio
    GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git"
    GIT_TAG 0b9f8b2f172290c0569e74467520bc961b8b0d18 # as of 08/12/2024 (v19.7.0+)
)
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR}) # using target "PortAudio"
endif()
target_link_libraries(${PROJECT_NAME} PortAudio)

# Resources
add_custom_target(res COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/cmake/res.cmake)
add_dependencies(${PROJECT_NAME} res)
