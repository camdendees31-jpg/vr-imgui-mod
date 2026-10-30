// ============================================================
//  Axiom VR Mod – Standalone example (desktop, for testing)
//  Links against libaxiom_vr_mod.so + ImGui + GLFW + OpenGL
//  [made by TagtusVR]
// ============================================================

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "axiom_theme.h"

#ifdef __ANDROID__
#  include <jni.h>
// On Android / Quest, call axiom_init() and axiom_render_frame()
// from your XrCompositor render loop instead.
#else
#  include <GLFW/glfw3.h>
#  include <cstdio>

extern "C" {
    void axiom_init();
    bool axiom_render_frame();
    void axiom_toggle_menu();
    void axiom_shutdown();
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600,
        "Axiom VR Mod – Preview [made by TagtusVR]", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    axiom_init();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Toggle menu with F1
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
            axiom_toggle_menu();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        axiom_render_frame();

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    axiom_shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
#endif // !__ANDROID__
