cmake_minimum_required(VERSION 3.22)
project(axiom_vr_mod VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------
# Dear ImGui (fetched by CI / developer into ./imgui/)
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
# Axiom shared library  →  libaxiom_vr_mod.so
# ---------------------------------------------------------------
add_library(axiom_vr_mod SHARED
    src/axiom_mod.cpp
    src/hook.cpp
    src/egl_hook.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(axiom_vr_mod PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# GLES3 backend (used by egl_hook.cpp on Android, and desktop preview).
target_sources(axiom_vr_mod PRIVATE
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Vulkan + OpenXR paths only make sense (and only have headers) on Android.
if(ANDROID)
    target_sources(axiom_vr_mod PRIVATE
        src/vk_hook.cpp
        src/xr_hook.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    )
endif()

target_compile_definitions(axiom_vr_mod PRIVATE
    IMGUI_IMPL_OPENGL_ES3
    GL_GLEXT_PROTOTYPES
)

target_compile_options(axiom_vr_mod PRIVATE -Wall -Wextra -O2 -fvisibility=hidden)

# Android / Quest
if(ANDROID)
    target_compile_options(axiom_vr_mod PRIVATE -march=armv8-a)
    find_library(log-lib     log)
    find_library(android-lib android)
    find_library(egl-lib     EGL)
    find_library(gles3-lib   GLESv3)
    find_library(vulkan-lib  vulkan)
    target_link_libraries(axiom_vr_mod
        ${log-lib}
        ${android-lib}
        ${egl-lib}
        ${gles3-lib}
        ${vulkan-lib}
    )
endif()

# ---------------------------------------------------------------
# Desktop preview  (Linux, skip on Android)
# ---------------------------------------------------------------
if(NOT ANDROID)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 3.3 QUIET)

    if(glfw3_FOUND)
        # Self-contained preview: compile the core + ImGui directly so
        # it does not depend on the hidden-visibility .so symbols.
        add_executable(axiom_example
            example/main_example.cpp
            src/axiom_mod.cpp
            ${IMGUI_SOURCES}
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        )
        target_include_directories(axiom_example PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
        )
        target_link_libraries(axiom_example glfw OpenGL::GL)
    else()
        message(STATUS "GLFW not found – skipping desktop example build")
    endif()
endif()

# ---------------------------------------------------------------
# Install
# ---------------------------------------------------------------
install(TARGETS axiom_vr_mod LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/axiom)
