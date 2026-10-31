// ============================================================
//  Axiom VR Mod – EGL swap-buffer hook + ImGui GLES3 backend
//
//  Intercepts eglSwapBuffers inside libunity.so so we can
//  inject an ImGui draw call before every frame is presented.
//  [made by TagtusVR]
// ============================================================
#ifdef __ANDROID__

#include "hook.h"
#include "axiom_theme.h"    // Axiom::ApplyTheme, GradientTitleBar
#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <chrono>
#include <atomic>
#include <pthread.h>
#include <unistd.h>

#define ETAG "AxiomEGL"
#define ELOGI(...) __android_log_print(ANDROID_LOG_INFO,  ETAG, __VA_ARGS__)
#define ELOGE(...) __android_log_print(ANDROID_LOG_ERROR, ETAG, __VA_ARGS__)

// ---- forward declaration from axiom_mod.cpp -------------------
extern "C" bool axiom_render_frame();

// ---- state ----------------------------------------------------
using SwapFn = EGLBoolean(*)(EGLDisplay, EGLSurface);
static SwapFn          g_real_swap   = nullptr;
static std::atomic<bool> g_initialized{false};

// ---- timing ---------------------------------------------------
static float elapsed_sec() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return duration<float>(steady_clock::now() - start).count();
}

// ---- ImGui GLES3 init (called once on the render thread) ------
static void init_imgui_gles3(int w, int h) {
    ELOGI("Initialising ImGui GLES3  display=%dx%d", w, h);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize  = ImVec2((float)w, (float)h);
    io.DeltaTime    = 1.0f / 72.0f;         // Quest default Hz
    io.IniFilename  = nullptr;               // no imgui.ini on Android
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // GLES 3.0  (#version 300 es)
    ImGui_ImplOpenGL3_Init("#version 300 es");

    // Scale UI for headset pixel density
    float scale = (w > 2000) ? 2.5f : (w > 1000 ? 1.8f : 1.2f);
    io.FontGlobalScale = scale;
    ImGui::GetStyle().ScaleAllSizes(scale);

    ELOGI("ImGui GLES3 ready (scale=%.1f)", scale);
}

// ---- our replacement for eglSwapBuffers -----------------------
static EGLBoolean hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // First call: initialise ImGui on this render thread.
    if (!g_initialized.exchange(true)) {
        EGLint w = 0, h = 0;
        eglQuerySurface(dpy, surface, EGL_WIDTH,  &w);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
        if (w == 0 || h == 0) { w = 1920; h = 1080; }
        init_imgui_gles3(w, h);
    }

    // Update display size every frame (handles orientation changes).
    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surface, EGL_WIDTH,  &w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    ImGuiIO& io       = ImGui::GetIO();
    io.DisplaySize    = ImVec2((float)w, (float)h);
    io.DeltaTime      = 1.0f / 72.0f;

    // Apply animated gradient theme.
    float t = elapsed_sec();
    Axiom::ApplyTheme(t);

    // Render ImGui overlay.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    axiom_render_frame();   // draws the Axiom mod menu

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Present the frame (with our overlay baked in).
    return g_real_swap(dpy, surface);
}

// ---- public: install the hook ---------------------------------
// Called repeatedly by the unified retry thread in axiom_mod.cpp so
// that late-loaded engine libs (libunity/libil2cpp/libmain) are also
// patched. Safe to call many times (patch_slot skips already-ours).
extern "C" bool install_egl_hook() {
    bool ok = axiom_hook_unity("eglSwapBuffers",
                               (void*)hooked_eglSwapBuffers,
                               (void**)&g_real_swap);
    if (ok) ELOGI("eglSwapBuffers hook active  real=%p", (void*)g_real_swap);
    return ok;
}

#endif // __ANDROID__
