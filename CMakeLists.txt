cmake_minimum_required(VERSION 3.15)
project(Caustics VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

add_executable(caustics 
    main.cpp 
    include/glad/glad.c
)

target_link_libraries(caustics PRIVATE OpenGL::GL)

if(WIN32)
    target_link_libraries(caustics PRIVATE 
        opengl32
        gdi32
        user32
        kernel32
    )
endif()

target_include_directories(caustics PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
)

find_library(GLFW_LIBRARY 
    NAMES glfw3 glfw
    PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw-3.4.bin.WIN64/lib-mingw-w64
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-mingw-dynamic/lib
    NO_DEFAULT_PATH
)

if(GLFW_LIBRARY)
    message(STATUS "Found GLFW library: ${GLFW_LIBRARY}")
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
else()
    message(FATAL_ERROR "Missing external/glfw-3.4.bin.WIN64/lib-mingw-w64/libglfw3.a. Copy lib-mingw-w64 from the GLFW 3.4 Windows zip. See README.md.")
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
