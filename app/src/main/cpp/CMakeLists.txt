cmake_minimum_required(VERSION 3.26)
project(3dgs_cpp_root)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a) # Adjust as needed
set(CMAKE_ANDROID_STL c++_shared)

# What is this for? I forget
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")
set(THIRD_PARTY_DIR ../../../../third_party)

add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR=1)

add_compile_definitions(VKGS_ENABLE_ANDROID)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()

#-------------------------------------------------------------------------
# Android GameActivity
# ------------------------------------------------------------------------

# Include the GameActivity static lib to the project.
find_package(game-activity REQUIRED CONFIG)
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u \
    Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# FROM HERE https://stackoverflow.com/questions/69144529/vulkan-hpp-with-cmake
#-------------------------------------------------------------------------
# Fetch Vulkan C Headers
# ------------------------------------------------------------------------

find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)

if (NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR "glslangValidator not found. Please ensure it is installed and VULKAN_SDK is set.")
else()
    message(STATUS "glslangValidator found at: ${GLSLANG_VALIDATOR_EXECUTABLE}")
endif()

# Set this for the shaders cmake
set(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE ${GLSLANG_VALIDATOR_EXECUTABLE})

# Declare required Vulkan version
set(VULKAN_VER_MAJOR 1)
set(VULKAN_VER_MINOR 3)
set(VULKAN_VER_PATCH 278)
set(VULKAN_VERSION ${VULKAN_VER_MAJOR}.${VULKAN_VER_MINOR}.${VULKAN_VER_PATCH})

message(STATUS "Include Vulkan C Headers")
FetchContent_Declare(
        vulkanheaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG v${VULKAN_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
set(PROJECT_IS_TOP_LEVEL OFF)
FetchContent_MakeAvailable(vulkanheaders) # vulkanheaders has a proper CMakeLists

#-------------------------------------------------------------------------
# Fetch Vulkan C++ Headers
#-------------------------------------------------------------------------
message(STATUS "Include Vulkan C++ Headers")
FetchContent_Declare(
        vulkanhpp
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
        GIT_TAG v${VULKAN_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
# vulkanhpp does not have a proper CMakeLists
FetchContent_GetProperties(vulkanhpp)
FetchContent_Populate(vulkanhpp)

# Define a library for Vulkan
add_library(vulkan_ext INTERFACE)
target_include_directories(vulkan_ext INTERFACE ${vulkanhpp_SOURCE_DIR})
target_link_libraries(vulkan_ext INTERFACE Vulkan::Headers)


# -------------------------------------------------
# libervpp library
# -------------------------------------------------
FetchContent_Declare(libenvpp
        GIT_REPOSITORY https://github.com/ph3at/libenvpp.git
        GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(libenvpp)

# -------------------------------------------------
# spdlog library
# -------------------------------------------------
FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
)
FetchContent_GetProperties(spdlog)
if (NOT spdlog_POPULATED)
    FetchContent_Populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

# -------------------------------------------------
# imgui library
# -------------------------------------------------
FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.3
)
FetchContent_MakeAvailable(imgui)


# ---------------------
# Build shaders (app/src/main/cpp/shaders)
# ---------------------

# STOP FOR NOW
add_subdirectory(shaders)


# Store list of these files into the variable `EXTERNAL_SOURCE`
file(GLOB EXTERNAL_SOURCE
        ${imgui_SOURCE_DIR}/*.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_android.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        ${THIRD_PARTY_DIR}/implot/implot.cpp
        ${THIRD_PARTY_DIR}/implot/implot_demo.cpp
        ${THIRD_PARTY_DIR}/implot/implot_items.cpp
)

# Import the CMakeLists.txt for the glm library
add_subdirectory(${THIRD_PARTY_DIR}/glm ${CMAKE_CURRENT_BINARY_DIR}/glm)

# Store list of these files into the variable `SOURCE`
file(GLOB SOURCE
        *.cpp
        vulkan/*.cpp
        vulkan/pipelines/*.cpp
#        vulkan/windowing/GLFWWindow.cpp
        vulkan/windowing/AndroidWindow.cpp
)

message("SOURCE IS ${SOURCE}")

# Remove DummyGUIManager.cpp from source list
list(REMOVE_ITEM SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/DummyGUIManager.cpp)

# Configure .cpp files
add_library(${PROJECT_NAME} SHARED
        ${SOURCE}
        ${EXTERNAL_SOURCE}
)

# Configure .h files
target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include/3dgs
        PRIVATE
        # I added
        ${THIRD_PARTY_DIR}/glm/glm
        ${THIRD_PARTY_DIR}/stb_image
        ${THIRD_PARTY_DIR}/stb_image_resize
        ${THIRD_PARTY_DIR}/vk
        ${THIRD_PARTY_DIR}/args
        ${THIRD_PARTY_DIR}/implot

        # We need vulkanheaders?
        ${vulkanheaders_SOURCE_DIR}/include

        ${GLM_INCLUDE_DIRS}
        ${glm_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${spdlog_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/shaders
)

# REMOVE FOR NOW
add_dependencies(${PROJECT_NAME} shaders)


target_link_libraries(${PROJECT_NAME} PUBLIC
        spdlog::spdlog

        # I added
        vulkan

        vulkan_ext
        libenvpp::libenvpp
        game-activity::game-activity_static
        android
        glm
        log
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${CMAKE_DL_LIBS})






