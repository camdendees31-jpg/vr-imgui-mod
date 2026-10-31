// ============================================================
//  Axiom VR Mod – ARM64 GOT patcher
//  Patches the PLT/GOT of a target .so so its import of a
//  named symbol redirects to our replacement function.
//  [made by TagtusVR]
// ============================================================
#ifdef __ANDROID__

#include "hook.h"
#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define HTAG "AxiomHook"
#define HLOGI(...) __android_log_print(ANDROID_LOG_INFO,  HTAG, __VA_ARGS__)
#define HLOGE(...) __android_log_print(ANDROID_LOG_ERROR, HTAG, __VA_ARGS__)

// Patch the PLT/GOT entry for `sym_name` inside already-loaded `lib_name`.
// *old_func receives the original pointer before patching.
bool axiom_patch_got(const char* lib_name, const char* sym_name,
                     void* new_func, void** old_func) {
    void* handle = dlopen(lib_name, RTLD_NOLOAD | RTLD_NOW);
    if (!handle) {
        HLOGE("dlopen(%s, NOLOAD) failed: %s", lib_name, dlerror());
        return false;
    }

    struct link_map* lm = nullptr;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &lm) != 0 || !lm) {
        HLOGE("dlinfo failed for %s", lib_name);
        dlclose(handle);
        return false;
    }

    uintptr_t    base        = (uintptr_t)lm->l_addr;
    ElfW(Dyn)*   dyn         = lm->l_ld;
    ElfW(Rela)*  rela_plt    = nullptr;
    size_t       rela_plt_sz = 0;
    char*        strtab      = nullptr;
    ElfW(Sym)*   symtab      = nullptr;

    for (; dyn->d_tag != DT_NULL; ++dyn) {
        switch (dyn->d_tag) {
            case DT_JMPREL:   rela_plt    = (ElfW(Rela)*)dyn->d_un.d_ptr; break;
            case DT_PLTRELSZ: rela_plt_sz = dyn->d_un.d_val;              break;
            case DT_STRTAB:   strtab      = (char*)dyn->d_un.d_ptr;       break;
            case DT_SYMTAB:   symtab      = (ElfW(Sym)*)dyn->d_un.d_ptr;  break;
            default: break;
        }
    }

    if (!rela_plt || !rela_plt_sz || !strtab || !symtab) {
        HLOGE("Missing dynamic sections in %s (jmprel=%p sz=%zu str=%p sym=%p)",
              lib_name, rela_plt, rela_plt_sz, strtab, symtab);
        dlclose(handle);
        return false;
    }

    const int   page_sz = getpagesize();
    const size_t count  = rela_plt_sz / sizeof(ElfW(Rela));
    bool found = false;

    for (size_t i = 0; i < count; ++i) {
        size_t sym_idx = ELF64_R_SYM(rela_plt[i].r_info);
        const char* name = strtab + symtab[sym_idx].st_name;
        if (strcmp(name, sym_name) != 0) continue;

        void** got_entry = (void**)(base + rela_plt[i].r_offset);
        uintptr_t page   = (uintptr_t)got_entry & ~(uintptr_t)(page_sz - 1);

        if (mprotect((void*)page, (size_t)page_sz, PROT_READ | PROT_WRITE) == 0) {
            if (old_func) *old_func = *got_entry;
            *got_entry = new_func;
            mprotect((void*)page, (size_t)page_sz, PROT_READ);
            HLOGI("GOT patched: %s!%s  old=%p -> new=%p",
                  lib_name, sym_name, old_func ? *old_func : nullptr, new_func);
            found = true;
        } else {
            HLOGE("mprotect failed for %s!%s", lib_name, sym_name);
        }
        break;
    }

    if (!found) HLOGE("Symbol '%s' not found in GOT of %s", sym_name, lib_name);
    dlclose(handle);
    return found;
}

// Try to hook `sym_name` across the common Unity library names.
// Fills *real_func with the genuine implementation obtained via dlsym on libEGL.
bool axiom_hook_unity(const char* sym_name, void* new_func, void** real_func) {
    // Grab the real symbol from libEGL before we patch anything.
    if (real_func) {
        void* egl = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
        if (egl) {
            void* sym = dlsym(egl, sym_name);
            if (sym) {
                *real_func = sym;
                HLOGI("Real %s = %p (from libEGL.so)", sym_name, sym);
            }
            dlclose(egl);
        }
    }

    static const char* unity_libs[] = {
        "libunity.so",    // Unity engine
        "libil2cpp.so",   // IL2CPP runtime (also calls EGL in some builds)
        "libmain.so",     // Some Unity builds
        nullptr
    };

    bool ok = false;
    for (int i = 0; unity_libs[i]; ++i)
        ok |= axiom_patch_got(unity_libs[i], sym_name, new_func, nullptr);

    return ok;
}

#endif // __ANDROID__
