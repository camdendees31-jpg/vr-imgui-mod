// ============================================================
//  Axiom VR Mod – Core module system + ImGui menu
//  Builds to libaxiom_vr_mod.so (ARM64)
//  [made by TagtusVR]
// ============================================================

#include "axiom_theme.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

// ---- clock helper --------------------------------------------
static float GetTimeSec() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return duration<float>(steady_clock::now() - start).count();
}

// ---- module system -------------------------------------------
struct AxiomModule {
    std::string name;
    bool        enabled = false;
    std::function<void(float)> onDraw;
};

static std::vector<AxiomModule> g_modules;
static bool   g_menuOpen  = true;
static char   g_logBuf[4096] = {};
static size_t g_logLen = 0;

static void AxiomLog(const char* msg) {
    size_t avail = sizeof(g_logBuf) - g_logLen;
    if (avail < 64) { g_logLen = 0; avail = sizeof(g_logBuf); }
    int written = snprintf(g_logBuf + g_logLen, avail, "[Axiom] %s\n", msg);
    if (written > 0) g_logLen += (size_t)written;
}

// ---- public C API --------------------------------------------
extern "C" {

void axiom_init() {
    g_modules.clear();
    g_logLen = 0;
    memset(g_logBuf, 0, sizeof(g_logBuf));

    g_modules.push_back({"Fly", false, [](float) {
        static float speed = 5.0f;
        ImGui::Text("Fly Speed"); ImGui::SameLine();
        ImGui::SliderFloat("##fly", &speed, 1.f, 50.f);
    }});

    g_modules.push_back({"NoClip", false, [](float) {
        ImGui::TextUnformatted("NoClip – walk through walls");
    }});

    g_modules.push_back({"Infinite Jump", false, [](float) {
        static int extra = 3;
        ImGui::SliderInt("Extra Jumps", &extra, 1, 10);
    }});

    g_modules.push_back({"Speed Hack", false, [](float) {
        static float mult = 2.0f;
        ImGui::SliderFloat("Multiplier", &mult, 1.f, 20.f);
    }});

    g_modules.push_back({"ESP", false, [](float) {
        static bool players = true, items = false;
        ImGui::Checkbox("Players", &players);
        ImGui::SameLine();
        ImGui::Checkbox("Items", &items);
    }});

    g_modules.push_back({"Auto-Farm", false, [](float) {
        static float radius = 10.f;
        ImGui::SliderFloat("Radius", &radius, 2.f, 100.f);
    }});

    AxiomLog("Axiom initialised");
}

// Called every frame by egl_hook.cpp's hooked_eglSwapBuffers.
// Theme and NewFrame are handled by the hook; we just build the window.
bool axiom_render_frame() {
    if (!g_menuOpen) return false;

    float t = GetTimeSec();

    ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar  |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize;

    ImGuiIO& io = ImGui::GetIO();
    // Position menu centre-top on the EGL surface
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.04f),
        ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(
        ImVec2(io.DisplaySize.x * 0.38f, io.DisplaySize.y * 0.55f),
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);

    if (!ImGui::Begin("##AxiomRoot", &g_menuOpen, wflags)) {
        ImGui::End();
        return g_menuOpen;
    }

    // Animated gradient title bar
    Axiom::GradientTitleBar("  Axiom VR Mod  [TagtusVR]", 28.f, t);
    ImGui::Separator();

    // Module list
    float listH = ImGui::GetContentRegionAvail().y - 100.f;
    ImGui::BeginChild("##mods", ImVec2(0, listH), true);
    for (auto& mod : g_modules) {
        ImVec4 accent = Axiom::GradientColor(t);
        ImGui::PushID(mod.name.c_str());

        if (mod.enabled) ImGui::PushStyleColor(ImGuiCol_Text, accent);
        bool open = ImGui::CollapsingHeader(mod.name.c_str());
        if (mod.enabled) ImGui::PopStyleColor();

        float w = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(w - ImGui::GetFrameHeight());
        ImGui::Checkbox("##en", &mod.enabled);

        if (open && mod.enabled && mod.onDraw) {
            ImGui::Indent(12.f);
            mod.onDraw(t);
            ImGui::Unindent(12.f);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Log
    ImGui::BeginChild("##log", ImVec2(0, 55.f), true);
    ImGui::TextUnformatted(g_logBuf);
    ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();

    // Buttons
    ImVec4 accent = Axiom::GradientColor(t);
    ImGui::PushStyleColor(ImGuiCol_Button, accent);
    if (ImGui::Button("Close", ImVec2(80, 0))) g_menuOpen = false;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Reset All", ImVec2(90, 0))) {
        for (auto& m : g_modules) m.enabled = false;
        AxiomLog("All modules disabled");
    }

    ImGui::End();
    return g_menuOpen;
}

void axiom_toggle_menu() { g_menuOpen = !g_menuOpen; }
void axiom_shutdown()    { AxiomLog("Axiom shutdown"); }

} // extern "C"

// ================================================================
//  Android / JNI entry points
// ================================================================
#ifdef __ANDROID__
#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

// Renderer hooks (defined in egl_hook.cpp / vk_hook.cpp / xr_hook.cpp)
extern "C" bool install_egl_hook();   // OpenGL ES games  -> renders menu
extern "C" bool install_vk_hook();    // Vulkan games     -> renders menu
extern "C" bool install_xr_hook();    // OpenXR (in-headset) -> detects path

#define AVLOG(...) __android_log_print(ANDROID_LOG_INFO, "AxiomVR", __VA_ARGS__)

// Engine graphics libs load lazily (often AFTER our .so), and the game
// may use GLES *or* Vulkan — we don't know which up front. So we retry
// every renderer hook for ~24s and let whichever one the game actually
// uses take effect. Each install is idempotent.
static void* axiom_hook_worker(void*) {
    bool egl_ok = false, vk_ok = false, xr_ok = false;
    for (int i = 0; i < 60; ++i) {
        if (!egl_ok) egl_ok = install_egl_hook();
        if (!vk_ok)  vk_ok  = install_vk_hook();
        if (!xr_ok)  xr_ok  = install_xr_hook();
        if (i == 0 || i == 10 || i == 30)
            AVLOG("hook status pass %d: EGL=%d VK=%d XR=%d", i, egl_ok, vk_ok, xr_ok);
        usleep(400 * 1000);
    }
    AVLOG("hook worker finished  (EGL=%d VK=%d XR=%d). "
          "If all 0, the game uses an unsupported present path.", egl_ok, vk_ok, xr_ok);
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* /*vm*/, void* /*reserved*/) {
    AVLOG("libaxiom_vr_mod.so loaded – installing GLES + Vulkan + OpenXR hooks");
    axiom_init();

    pthread_t th;
    if (pthread_create(&th, nullptr, axiom_hook_worker, nullptr) == 0)
        pthread_detach(th);
    else
        AVLOG("pthread_create failed – running single-pass install");

    return JNI_VERSION_1_6;
}

// JNI helpers callable from Java/Kotlin (optional)
extern "C" JNIEXPORT void JNICALL
Java_com_axiom_vr_AxiomLib_toggleMenu(JNIEnv*, jclass) {
    axiom_toggle_menu();
}
extern "C" JNIEXPORT void JNICALL
Java_com_axiom_vr_AxiomLib_shutdown(JNIEnv*, jclass) {
    axiom_shutdown();
}
#endif // __ANDROID__
