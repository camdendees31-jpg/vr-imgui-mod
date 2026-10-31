// ============================================================
//  Axiom VR Mod – OpenXR detection + instrumentation
//
//  On Quest, the headset image is composited by the OpenXR
//  runtime via xrEndFrame – NOT by eglSwapBuffers/vkQueuePresent
//  (those only feed the 2D mirror). To draw INSIDE the headset
//  you must inject a composition layer at xrEndFrame.
//
//  This file safely instruments that path: it reports which
//  graphics API the VR session uses (GLES vs Vulkan) and
//  confirms xrEndFrame fires, WITHOUT modifying the frame yet
//  (blind layer injection can crash the game). The logcat output
//  tells us exactly what to target for in-headset rendering.
//  Filter with:  adb logcat -s AxiomXR
//  [made by TagtusVR]
// ============================================================
#ifdef __ANDROID__

#include "hook.h"
#include <android/log.h>
#include <dlfcn.h>
#include <stdint.h>

#define XTAG "AxiomXR"
#define XLOGI(...) __android_log_print(ANDROID_LOG_INFO,  XTAG, __VA_ARGS__)
#define XLOGE(...) __android_log_print(ANDROID_LOG_ERROR, XTAG, __VA_ARGS__)

// ---- minimal OpenXR ABI (avoid an SDK dependency) -------------
typedef void*   XrHandle;
typedef int32_t XrResult;

// XR_TYPE_GRAPHICS_BINDING_* enum values (stable, from openxr.h).
static const int XR_TYPE_GB_OPENGL_ES_ANDROID = 1000023000;
static const int XR_TYPE_GB_VULKAN            = 1000025000;
static const int XR_TYPE_GB_D3D11             = 1000027000; // desktop link

struct XrBaseStub { int type; int _pad; const void* next; };
struct XrSessionCreateInfoStub { int type; int _pad; const void* next; };

typedef XrResult (*PFN_xrEndFrame)(XrHandle session, const void* frameEndInfo);
typedef XrResult (*PFN_xrCreateSession)(XrHandle instance, const void* createInfo, XrHandle* session);
typedef XrResult (*PFN_xrCreateSwapchain)(XrHandle session, const void* createInfo, XrHandle* swapchain);

static PFN_xrEndFrame        g_real_endFrame       = nullptr;
static PFN_xrCreateSession   g_real_createSession  = nullptr;
static PFN_xrCreateSwapchain g_real_createSwapchain = nullptr;

static const char* binding_name(int type) {
    switch (type) {
        case XR_TYPE_GB_OPENGL_ES_ANDROID: return "OpenGL ES (GLES)";
        case XR_TYPE_GB_VULKAN:            return "Vulkan";
        case XR_TYPE_GB_D3D11:             return "D3D11 (PCVR link)";
        default:                           return "unknown";
    }
}

static XrResult hooked_xrCreateSession(XrHandle inst, const void* ci, XrHandle* out) {
    // Report the graphics API bound to this VR session.
    if (ci) {
        const XrSessionCreateInfoStub* s = (const XrSessionCreateInfoStub*)ci;
        if (s->next) {
            int t = ((const XrBaseStub*)s->next)->type;
            XLOGI("xrCreateSession: VR graphics API = %s (type=%d)", binding_name(t), t);
        } else {
            XLOGI("xrCreateSession: no graphics binding in next-chain");
        }
    }
    return g_real_createSession ? g_real_createSession(inst, ci, out) : -1;
}

static XrResult hooked_xrCreateSwapchain(XrHandle session, const void* ci, XrHandle* out) {
    XLOGI("xrCreateSwapchain called (VR eye/overlay swapchain created)");
    return g_real_createSwapchain ? g_real_createSwapchain(session, ci, out) : -1;
}

static XrResult hooked_xrEndFrame(XrHandle session, const void* frameEndInfo) {
    static int once = 0;
    if (once++ == 0)
        XLOGI("xrEndFrame firing – this is the in-headset injection point. "
              "Overlay layer injection is the next step once the API above is known.");
    // SAFE passthrough: do not alter the submitted layers yet.
    return g_real_endFrame ? g_real_endFrame(session, frameEndInfo) : 0;
}

extern "C" bool install_xr_hook() {
    // The OpenXR loader on Quest.
    const char* names[] = { "libopenxr_loader.so", "libopenxr_forwardloader.so", nullptr };
    void* xr = nullptr;
    for (int i = 0; names[i] && !xr; ++i) xr = dlopen(names[i], RTLD_NOW | RTLD_GLOBAL);
    if (!xr) { XLOGI("OpenXR loader not present – game may use VrApi or be flatscreen"); return false; }

    g_real_endFrame        = (PFN_xrEndFrame)dlsym(xr, "xrEndFrame");
    g_real_createSession   = (PFN_xrCreateSession)dlsym(xr, "xrCreateSession");
    g_real_createSwapchain = (PFN_xrCreateSwapchain)dlsym(xr, "xrCreateSwapchain");

    int n = 0;
    n += axiom_patch_got(nullptr, "xrEndFrame",       (void*)hooked_xrEndFrame,       nullptr);
    n += axiom_patch_got(nullptr, "xrCreateSession",  (void*)hooked_xrCreateSession,  nullptr);
    n += axiom_patch_got(nullptr, "xrCreateSwapchain",(void*)hooked_xrCreateSwapchain,nullptr);
    XLOGI("OpenXR hooks installed (%d GOT slots) – watch 'adb logcat -s AxiomXR'", n);
    return n > 0;
}

#endif // __ANDROID__
