cmake_minimum_required(VERSION 3.22)
project(axiom_vr_mod VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------
# ImGui sources (fetched into ./imgui/ — CI clones v1.90.4)
# ---------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
)

# ---------------------------------------------------------------
# Axiom shared library  (libaxiom_vr_mod.so)
# ---------------------------------------------------------------
add_library(axiom_vr_mod SHARED
    src/axiom_mod.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(axiom_vr_mod PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_DIR}
)

target_compile_options(axiom_vr_mod PRIVATE -Wall -Wextra -O2)

# ARM64 flags (applied when cross-compiling with android-cmake toolchain)
if(ANDROID)
    target_compile_options(axiom_vr_mod PRIVATE -march=armv8-a)
    find_library(log-lib log)
    target_link_libraries(axiom_vr_mod ${log-lib} android)
endif()

# ---------------------------------------------------------------
# Desktop example  (skipped on Android)
# ---------------------------------------------------------------
if(NOT ANDROID)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 3.3 QUIET)

    if(glfw3_FOUND)
        add_executable(axiom_example
            example/main_example.cpp
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        )
        target_include_directories(axiom_example PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
        )
        target_link_libraries(axiom_example axiom_vr_mod glfw OpenGL::GL)
    else()
        message(STATUS "GLFW not found – skipping desktop example build")
    endif()
endif()

# ---------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------
install(TARGETS axiom_vr_mod
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/axiom)
