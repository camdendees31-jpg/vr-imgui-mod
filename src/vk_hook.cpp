// ============================================================
//  Axiom VR Mod – Vulkan overlay hook
//
//  Modern Quest/Unity games render with Vulkan (OpenGL ES is
//  legacy), so an eglSwapBuffers hook alone never fires. This
//  file hooks the Vulkan present path and draws the ImGui menu
//  into the swapchain image right before it is presented.
//
//  Robust dispatch: we hook BOTH the direct imports (GOT) AND
//  vkGetInstanceProcAddr / vkGetDeviceProcAddr, because engines
//  like Unity resolve Vulkan entry points through their own
//  loader table instead of the ELF import table.
//  [made by TagtusVR]
// ============================================================
#ifdef __ANDROID__

#include "hook.h"
#include "axiom_theme.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#include <vulkan/vulkan.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <chrono>
#include <mutex>
#include <vector>

#define VTAG "AxiomVK"
#define VLOGI(...) __android_log_print(ANDROID_LOG_INFO,  VTAG, __VA_ARGS__)
#define VLOGE(...) __android_log_print(ANDROID_LOG_ERROR, VTAG, __VA_ARGS__)

extern "C" bool axiom_render_frame();   // builds the menu (defined in axiom_mod.cpp)

// ---- real function pointers -----------------------------------
static PFN_vkGetInstanceProcAddr g_real_gipa   = nullptr;
static PFN_vkGetDeviceProcAddr   g_real_gdpa   = nullptr;
static PFN_vkQueuePresentKHR     g_real_present = nullptr;
static PFN_vkCreateDevice        g_real_createDevice = nullptr;
static PFN_vkCreateSwapchainKHR  g_real_createSwapchain = nullptr;
static PFN_vkGetDeviceQueue      g_real_getQueue = nullptr;

// ---- captured Vulkan context ----------------------------------
static VkInstance       g_instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_phys     = VK_NULL_HANDLE;
static VkDevice         g_device   = VK_NULL_HANDLE;
static uint32_t         g_family   = 0;
static VkQueue          g_queue    = VK_NULL_HANDLE;

static VkRenderPass     g_renderPass = VK_NULL_HANDLE;
static VkDescriptorPool g_descPool   = VK_NULL_HANDLE;
static VkCommandPool    g_cmdPool    = VK_NULL_HANDLE;
static bool             g_imguiReady = false;
static std::mutex       g_vkMutex;

struct SwapData {
    VkSwapchainKHR             handle = VK_NULL_HANDLE;
    VkFormat                   format = VK_FORMAT_UNDEFINED;
    VkExtent2D                 extent = {0, 0};
    std::vector<VkImageView>   views;
    std::vector<VkFramebuffer> fbs;
    std::vector<VkCommandBuffer> cmds;
    std::vector<VkFence>       fences;
    bool                       ready = false;
};
static std::vector<SwapData*> g_swaps;

// Pending swapchain create-infos (format/extent) keyed by result handle.
static VkFormat   g_pendingFormat = VK_FORMAT_B8G8R8A8_UNORM;
static VkExtent2D g_pendingExtent = {0, 0};

static float vk_time() {
    using namespace std::chrono;
    static auto t0 = steady_clock::now();
    return duration<float>(steady_clock::now() - t0).count();
}

// ---- build per-swapchain Vulkan resources ---------------------
static bool build_swap_resources(SwapData* s) {
    uint32_t count = 0;
    if (vkGetSwapchainImagesKHR(g_device, s->handle, &count, nullptr) != VK_SUCCESS || !count)
        return false;
    std::vector<VkImage> images(count);
    vkGetSwapchainImagesKHR(g_device, s->handle, &count, images.data());

    // Render pass (load existing frame, draw over it, keep present layout).
    if (g_renderPass == VK_NULL_HANDLE) {
        VkAttachmentDescription att{};
        att.format         = s->format;
        att.samples        = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
        att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments    = &ref;
        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rp.attachmentCount = 1; rp.pAttachments = &att;
        rp.subpassCount    = 1; rp.pSubpasses   = &sub;
        rp.dependencyCount = 1; rp.pDependencies = &dep;
        if (vkCreateRenderPass(g_device, &rp, nullptr, &g_renderPass) != VK_SUCCESS) {
            VLOGE("vkCreateRenderPass failed"); return false;
        }
    }

    if (g_cmdPool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cp.queueFamilyIndex = g_family;
        if (vkCreateCommandPool(g_device, &cp, nullptr, &g_cmdPool) != VK_SUCCESS) {
            VLOGE("vkCreateCommandPool failed"); return false;
        }
    }

    s->views.resize(count);
    s->fbs.resize(count);
    s->cmds.resize(count);
    s->fences.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        iv.image    = images[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format   = s->format;
        iv.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                        VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        iv.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(g_device, &iv, nullptr, &s->views[i]) != VK_SUCCESS) return false;

        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass = g_renderPass;
        fb.attachmentCount = 1; fb.pAttachments = &s->views[i];
        fb.width = s->extent.width; fb.height = s->extent.height; fb.layers = 1;
        if (vkCreateFramebuffer(g_device, &fb, nullptr, &s->fbs[i]) != VK_SUCCESS) return false;

        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ca.commandPool = g_cmdPool;
        ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ca.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(g_device, &ca, &s->cmds[i]) != VK_SUCCESS) return false;

        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fc.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(g_device, &fc, nullptr, &s->fences[i]) != VK_SUCCESS) return false;
    }
    s->ready = true;
    VLOGI("swapchain resources ready: %u images  %ux%u fmt=%d",
          count, s->extent.width, s->extent.height, (int)s->format);
    return true;
}

static bool ensure_imgui(uint32_t imageCount) {
    if (g_imguiReady) return true;
    if (!g_device || !g_queue || g_renderPass == VK_NULL_HANDLE) return false;

    // Descriptor pool for ImGui.
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
    };
    VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dp.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dp.maxSets = 64;
    dp.poolSizeCount = 1; dp.pPoolSizes = sizes;
    if (g_descPool == VK_NULL_HANDLE &&
        vkCreateDescriptorPool(g_device, &dp, nullptr, &g_descPool) != VK_SUCCESS) {
        VLOGE("vkCreateDescriptorPool failed"); return false;
    }

    if (!ImGui::GetCurrentContext()) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance       = g_instance;
    info.PhysicalDevice = g_phys;
    info.Device         = g_device;
    info.QueueFamily    = g_family;
    info.Queue          = g_queue;
    info.DescriptorPool = g_descPool;
    info.RenderPass     = g_renderPass;
    info.MinImageCount  = imageCount < 2 ? 2 : imageCount;
    info.ImageCount     = imageCount < 2 ? 2 : imageCount;
    info.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&info)) {
        VLOGE("ImGui_ImplVulkan_Init failed"); return false;
    }
    ImGui_ImplVulkan_CreateFontsTexture();
    g_imguiReady = true;
    VLOGI("ImGui Vulkan backend initialised (fam=%u imgs=%u)", g_family, imageCount);
    return true;
}

static SwapData* find_or_make(VkSwapchainKHR sc) {
    for (auto* s : g_swaps) if (s->handle == sc) return s;
    SwapData* s = new SwapData();
    s->handle = sc;
    s->format = g_pendingFormat;
    s->extent = g_pendingExtent;
    g_swaps.push_back(s);
    return s;
}

static void draw_overlay(VkQueue queue, const VkPresentInfoKHR* pi) {
    if (!pi || pi->swapchainCount == 0) return;
    std::lock_guard<std::mutex> lk(g_vkMutex);

    if (!g_queue) g_queue = queue;        // present queue works for our submit

    VkSwapchainKHR sc = pi->pSwapchains[0];
    uint32_t idx      = pi->pImageIndices[0];
    SwapData* s = find_or_make(sc);
    if (s->extent.width == 0) s->extent = g_pendingExtent;

    if (!s->ready && !build_swap_resources(s)) return;
    if (!ensure_imgui((uint32_t)s->fbs.size())) return;
    if (idx >= s->fbs.size()) return;

    vkWaitForFences(g_device, 1, &s->fences[idx], VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &s->fences[idx]);

    VkCommandBuffer cb = s->cmds[idx];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = g_renderPass;
    rp.framebuffer = s->fbs[idx];
    rp.renderArea.extent = s->extent;
    vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);

    // Build the ImGui frame.
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s->extent.width, (float)s->extent.height);
    io.DeltaTime   = 1.0f / 72.0f;
    Axiom::ApplyTheme(vk_time());
    axiom_render_frame();
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);

    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkQueueSubmit(queue, 1, &si, s->fences[idx]);
}

// ---- hooked entry points --------------------------------------
static VKAPI_ATTR VkResult VKAPI_CALL
hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pi) {
    draw_overlay(queue, pi);
    return g_real_present ? g_real_present(queue, pi)
                          : vkQueuePresentKHR(queue, pi);
}

static VKAPI_ATTR VkResult VKAPI_CALL
hooked_vkCreateDevice(VkPhysicalDevice phys, const VkDeviceCreateInfo* ci,
                      const VkAllocationCallbacks* alloc, VkDevice* out) {
    VkResult r = g_real_createDevice ? g_real_createDevice(phys, ci, alloc, out)
                                     : vkCreateDevice(phys, ci, alloc, out);
    if (r == VK_SUCCESS) {
        g_phys = phys;
        g_device = *out;
        if (ci->queueCreateInfoCount) g_family = ci->pQueueCreateInfos[0].queueFamilyIndex;
        VLOGI("vkCreateDevice captured: device=%p family=%u", (void*)g_device, g_family);
    }
    return r;
}

static VKAPI_ATTR void VKAPI_CALL
hooked_vkGetDeviceQueue(VkDevice dev, uint32_t fam, uint32_t qidx, VkQueue* out) {
    if (g_real_getQueue) g_real_getQueue(dev, fam, qidx, out);
    else                 vkGetDeviceQueue(dev, fam, qidx, out);
    if (out && *out) { g_queue = *out; g_family = fam; }
}

static VKAPI_ATTR VkResult VKAPI_CALL
hooked_vkCreateSwapchainKHR(VkDevice dev, const VkSwapchainCreateInfoKHR* ci,
                            const VkAllocationCallbacks* alloc, VkSwapchainKHR* out) {
    if (ci) { g_pendingFormat = ci->imageFormat; g_pendingExtent = ci->imageExtent; }
    VkResult r = g_real_createSwapchain ? g_real_createSwapchain(dev, ci, alloc, out)
                                        : vkCreateSwapchainKHR(dev, ci, alloc, out);
    if (r == VK_SUCCESS) {
        g_device = dev;
        VLOGI("vkCreateSwapchainKHR %ux%u fmt=%d", ci->imageExtent.width,
              ci->imageExtent.height, (int)ci->imageFormat);
    }
    return r;
}

// Loader-based dispatch (Unity): return our hook for targeted names.
static PFN_vkVoidFunction dispatch(const char* name, PFN_vkVoidFunction real) {
    if (!name) return real;
    if (!strcmp(name, "vkQueuePresentKHR")) {
        if (!g_real_present) g_real_present = (PFN_vkQueuePresentKHR)real;
        return (PFN_vkVoidFunction)hooked_vkQueuePresentKHR;
    }
    if (!strcmp(name, "vkCreateDevice")) {
        if (!g_real_createDevice) g_real_createDevice = (PFN_vkCreateDevice)real;
        return (PFN_vkVoidFunction)hooked_vkCreateDevice;
    }
    if (!strcmp(name, "vkGetDeviceQueue")) {
        if (!g_real_getQueue) g_real_getQueue = (PFN_vkGetDeviceQueue)real;
        return (PFN_vkVoidFunction)hooked_vkGetDeviceQueue;
    }
    if (!strcmp(name, "vkCreateSwapchainKHR")) {
        if (!g_real_createSwapchain) g_real_createSwapchain = (PFN_vkCreateSwapchainKHR)real;
        return (PFN_vkVoidFunction)hooked_vkCreateSwapchainKHR;
    }
    return real;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
hooked_vkGetInstanceProcAddr(VkInstance inst, const char* name) {
    if (inst && inst != VK_NULL_HANDLE) g_instance = inst;
    PFN_vkVoidFunction real = g_real_gipa ? g_real_gipa(inst, name)
                                          : vkGetInstanceProcAddr(inst, name);
    return dispatch(name, real);
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
hooked_vkGetDeviceProcAddr(VkDevice dev, const char* name) {
    PFN_vkVoidFunction real = g_real_gdpa ? g_real_gdpa(dev, name)
                                          : vkGetDeviceProcAddr(dev, name);
    return dispatch(name, real);
}

// ---- public: install the Vulkan hooks -------------------------
extern "C" bool install_vk_hook() {
    void* vk = dlopen("libvulkan.so", RTLD_NOW | RTLD_GLOBAL);
    if (!vk) { VLOGE("libvulkan.so not present – game is not Vulkan?"); return false; }

    g_real_gipa = (PFN_vkGetInstanceProcAddr)dlsym(vk, "vkGetInstanceProcAddr");
    g_real_gdpa = (PFN_vkGetDeviceProcAddr)dlsym(vk, "vkGetDeviceProcAddr");
    g_real_present         = (PFN_vkQueuePresentKHR)dlsym(vk, "vkQueuePresentKHR");
    g_real_createDevice    = (PFN_vkCreateDevice)dlsym(vk, "vkCreateDevice");
    g_real_createSwapchain = (PFN_vkCreateSwapchainKHR)dlsym(vk, "vkCreateSwapchainKHR");
    g_real_getQueue        = (PFN_vkGetDeviceQueue)dlsym(vk, "vkGetDeviceQueue");

    int n = 0;
    // Robust: patch loader entry points AND direct imports across all libs.
    n += axiom_patch_got(nullptr, "vkGetInstanceProcAddr", (void*)hooked_vkGetInstanceProcAddr, nullptr);
    n += axiom_patch_got(nullptr, "vkGetDeviceProcAddr",   (void*)hooked_vkGetDeviceProcAddr,   nullptr);
    n += axiom_patch_got(nullptr, "vkQueuePresentKHR",     (void*)hooked_vkQueuePresentKHR,     nullptr);
    n += axiom_patch_got(nullptr, "vkCreateDevice",        (void*)hooked_vkCreateDevice,        nullptr);
    n += axiom_patch_got(nullptr, "vkGetDeviceQueue",      (void*)hooked_vkGetDeviceQueue,      nullptr);
    n += axiom_patch_got(nullptr, "vkCreateSwapchainKHR",  (void*)hooked_vkCreateSwapchainKHR,  nullptr);

    VLOGI("Vulkan hooks installed (%d GOT slots patched; loader dispatch active)", n);
    return true;
}

#endif // __ANDROID__
