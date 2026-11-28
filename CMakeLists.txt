cmake_minimum_required(VERSION 3.15)
project(Caustics VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

add_executable(caustics
    main.cpp
    include/glad/glad.c
    src/app.cpp
    src/sim/wave_sim.cpp
    src/render/shader_program.cpp
    src/render/caustics_pass.cpp
    src/render/pool_renderer.cpp
    src/render/water_renderer.cpp
    src/render/skybox_renderer.cpp
    src/ui/camera.cpp
    src/ui/overlay_ui.cpp
)

target_link_libraries(caustics PRIVATE OpenGL::GL)

if(WIN32)
    target_link_libraries(caustics PRIVATE
        opengl32
        gdi32
        user32
        kernel32
    )
elseif(APPLE)
    target_link_libraries(caustics PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(caustics PRIVATE
        ${CMAKE_DL_LIBS}
        Threads::Threads
        m
    )
endif()

# GLM headers: prefer repo submodule, fall back to system/vcpkg/user-provided GLM_INCLUDE_DIR
find_path(GLM_INCLUDE_DIR
    NAMES glm/glm.hpp
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-mingw-dynamic/include
)
if(NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "Missing GLM headers (glm/glm.hpp). Run 'git submodule update --init external/glm' or install GLM.")
endif()

target_include_directories(caustics PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GLM_INCLUDE_DIR}
)

# GLFW: preserve bundled Windows MinGW path first, then fall back to CMake package or system library
set(GLFW_FOUND_BUNDLED FALSE)
if(WIN32)
    find_library(GLFW_LIBRARY
        NAMES glfw3 glfw
        PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw-3.4.bin.WIN64/lib-mingw-w64
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-mingw-dynamic/lib
        NO_DEFAULT_PATH
    )
    if(GLFW_LIBRARY)
        set(GLFW_FOUND_BUNDLED TRUE)
        message(STATUS "Found bundled/vcpkg GLFW library: ${GLFW_LIBRARY}")
        target_link_libraries(caustics PRIVATE ${GLFW_LIBRARY})

        find_path(GLFW_INCLUDE_DIR
            NAMES GLFW/glfw3.h
            PATHS
                ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw-3.4.bin.WIN64/include
                ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/include
                ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-mingw-dynamic/include
        )
        if(GLFW_INCLUDE_DIR)
            target_include_directories(caustics PRIVATE ${GLFW_INCLUDE_DIR})
        endif()
    endif()
endif()

if(NOT GLFW_FOUND_BUNDLED)
    find_package(glfw3 QUIET)
    if(TARGET glfw)
        message(STATUS "Found GLFW via CMake package target: glfw")
        target_link_libraries(caustics PRIVATE glfw)
    else()
        find_library(GLFW_LIBRARY NAMES glfw3 glfw)
        find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h)
        if(GLFW_LIBRARY)
            message(STATUS "Found system GLFW library: ${GLFW_LIBRARY}")
            target_link_libraries(caustics PRIVATE ${GLFW_LIBRARY})
            if(GLFW_INCLUDE_DIR)
                target_include_directories(caustics PRIVATE ${GLFW_INCLUDE_DIR})
            endif()
        else()
            message(FATAL_ERROR
                "Missing GLFW library. On Windows MinGW, copy lib-mingw-w64 from the GLFW 3.4 Windows zip into "
                "external/glfw-3.4.bin.WIN64/lib-mingw-w64. On Linux/macOS/vcpkg, install glfw3 (e.g., libglfw3-dev or brew install glfw). See README.md.")
        endif()
    endif()
endif()

set_target_properties(caustics PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(WIN32 AND GLFW_LIBRARY)
    get_filename_component(GLFW_LIB_DIR ${GLFW_LIBRARY} DIRECTORY)
    find_file(GLFW_DLL
        NAMES glfw3.dll
        PATHS ${GLFW_LIB_DIR}
        NO_DEFAULT_PATH
    )

    if(GLFW_DLL)
        add_custom_command(TARGET caustics POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${GLFW_DLL}
            $<TARGET_FILE_DIR:caustics>
        )
    endif()
endif()
