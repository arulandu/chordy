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
    ./lib
    ./lib/imgui
    ./lib/imgui/backends
    ./lib/implot
    ./lib/kissfft
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

FetchContent_Declare(eigen
    GIT_REPOSITORY "https://gitlab.com/libeigen/eigen.git"
    GIT_TAG ab310943d62982a45734f52fed782ec57c9aa6a0 # as of 08/12/2024 (v3.4.0)
)
FetchContent_GetProperties(eigen)
if(NOT eigen_POPULATED)
    FetchContent_Populate(eigen)
    add_subdirectory(${eigen_SOURCE_DIR}) # using target "PortAudio"
endif()
include_directories(
    ${eigen_SOURCE_DIR}
)
target_link_libraries(${PROJECT_NAME} eigen)
