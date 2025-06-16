cmake_minimum_required(VERSION 3.10)
project(caustics)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenGL REQUIRED)

# Set GLFW paths
set(GLFW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/glfw-3.4.bin.WIN64/include")
set(GLFW_LIBRARY "${CMAKE_SOURCE_DIR}/external/glfw-3.4.bin.WIN64/lib-mingw-w64/libglfw3dll.a")

# Set GLM path
set(GLM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/glm")

# Include directories
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${GLFW_INCLUDE_DIR})
include_directories(${GLM_INCLUDE_DIR})

# Add local libraries
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/glad
    ${CMAKE_SOURCE_DIR}/external/glfw-3.4.bin.WIN64/include
    ${CMAKE_SOURCE_DIR}/external/glm
)

# Add source files
set(SOURCES
    main.cpp
    raytracer.cpp
    wave_sim.cpp
    renderer.cpp
    ${CMAKE_SOURCE_DIR}/include/glad/glad.c
)

# Create executable
add_executable(caustics ${SOURCES})

# Link libraries
target_link_libraries(caustics
    OpenGL::GL
    ${GLFW_LIBRARY}
)

# Include directories
target_include_directories(caustics PRIVATE
    ${OPENGL_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)

# Copy DLL to output directory
add_custom_command(TARGET caustics POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/external/glfw-3.4.bin.WIN64/lib-mingw-w64/glfw3.dll"
        $<TARGET_FILE_DIR:caustics>
) 