// ============================================================
//  Axiom VR Mod – Core library entry point
//  Builds to libaxiom_vr_mod.so (ARM64)
//  [made by TagtusVR]
// ============================================================

#include "axiom_theme.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

// ---- simple clock helper -------------------------------------------
static float GetTime() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return duration<float>(steady_clock::now() - start).count();
}

// ---- module system -------------------------------------------------
struct AxiomModule {
    std::string  name;
    bool         enabled = false;
    std::function<void(float)> onDraw; // called every frame when enabled
};

static std::vector<AxiomModule> g_modules;
static bool   g_menuOpen  = true;
static char   g_logBuf[4096] = {};
static int    g_logOffset = 0;

static void AxiomLog(const char* msg) {
    int written = snprintf(g_logBuf + g_logOffset,
                           sizeof(g_logBuf) - g_logOffset, "[Axiom] %s\n", msg);
    if (written > 0) g_logOffset += written;
    if (g_logOffset >= (int)sizeof(g_logBuf) - 256) g_logOffset = 0;
}

// ---- public C API (for JNI / native bridge) -----------------------
extern "C" {

void axiom_init() {
    g_modules.clear();
    g_logOffset = 0;

    // -- Register built-in VR mod modules --
    g_modules.push_back({"Fly",   false, [](float t){
        ImGui::Text("Fly speed:");
        static float speed = 5.0f;
        ImGui::SliderFloat("##fly_speed", &speed, 1.0f, 50.0f);
    }});

    g_modules.push_back({"NoClip", false, [](float t){
        ImGui::Text("NoClip active – walk through walls");
    }});

    g_modules.push_back({"Infinite Jump", false, [](float t){
        static int jumps = 3;
        ImGui::SliderInt("Extra Jumps", &jumps, 1, 10);
    }});

    g_modules.push_back({"Speed Hack", false, [](float t){
        static float mult = 2.0f;
        ImGui::SliderFloat("Speed Multiplier", &mult, 1.0f, 20.0f);
    }});

    g_modules.push_back({"ESP", false, [](float t){
        static bool players = true, items = false;
        ImGui::Checkbox("Players", &players);
        ImGui::SameLine();
        ImGui::Checkbox("Items", &items);
    }});

    g_modules.push_back({"Auto-Farm", false, [](float t){
        static float radius = 10.0f;
        ImGui::SliderFloat("Farm Radius", &radius, 2.0f, 100.0f);
    }});

    AxiomLog("Axiom VR Mod initialised");
}

// Call every frame from your rendering loop.
// Returns true while the menu is open.
bool axiom_render_frame() {
    float t = GetTime();

    // Animate theme every frame
    Axiom::ApplyTheme(t);

    if (!g_menuOpen) return false;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(420, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40, 40),   ImGuiCond_FirstUseEver);

    ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar;

    if (!ImGui::Begin("##AxiomRoot", &g_menuOpen, wflags)) {
        ImGui::End();
        return g_menuOpen;
    }

    // ---- Animated title bar ----
    Axiom::GradientTitleBar("  Axiom VR Mod  [made by TagtusVR]", 32.0f, t);
    ImGui::Separator();

    // ---- Modules list ----
    ImGui::BeginChild("##modules", ImVec2(0, 300), true);
    for (auto& mod : g_modules) {
        ImVec4 accent = Axiom::GradientColor(t);
        if (mod.enabled) ImGui::PushStyleColor(ImGuiCol_Text, accent);

        bool toggled = ImGui::CollapsingHeader(mod.name.c_str());

        if (mod.enabled) ImGui::PopStyleColor();

        // toggle on double-click header or checkbox
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        ImGui::PushID(mod.name.c_str());
        ImGui::Checkbox("##en", &mod.enabled);
        ImGui::PopID();

        if (toggled && mod.enabled && mod.onDraw) {
            ImGui::Indent();
            mod.onDraw(t);
            ImGui::Unindent();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // ---- Log window ----
    ImGui::BeginChild("##log", ImVec2(0, 120), true);
    ImGui::TextUnformatted(g_logBuf);
    ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Close Menu")) g_menuOpen = false;
    ImGui::SameLine();
    if (ImGui::Button("Reset All")) {
        for (auto& m : g_modules) m.enabled = false;
        AxiomLog("All modules disabled");
    }

    ImGui::End();
    return g_menuOpen;
}

void axiom_toggle_menu() { g_menuOpen = !g_menuOpen; }
void axiom_shutdown()    { AxiomLog("Axiom shutdown"); }

} // extern "C"

#ifdef __ANDROID__
#include <jni.h>
#include <android/log.h>

// Runs automatically when Java calls System.loadLibrary("axiom_vr_mod").
// Check with: adb logcat -s AxiomVR
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* /*vm*/, void* /*reserved*/) {
    __android_log_print(ANDROID_LOG_INFO, "AxiomVR", "libaxiom_vr_mod.so loaded");
    axiom_init();
    return JNI_VERSION_1_6;
}
#endif
