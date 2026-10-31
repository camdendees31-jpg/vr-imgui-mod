// ============================================================
//  Axiom VR Mod – ARM64 GOT patcher
//  Walks every loaded ELF (dl_iterate_phdr – bionic has no
//  dlinfo) and redirects its import of a symbol to our hook.
//  [made by TagtusVR]
// ============================================================
#ifdef __ANDROID__

#include "hook.h"
#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define HTAG "AxiomHook"
#define HLOGI(...) __android_log_print(ANDROID_LOG_INFO,  HTAG, __VA_ARGS__)
#define HLOGE(...) __android_log_print(ANDROID_LOG_ERROR, HTAG, __VA_ARGS__)

namespace {

struct PatchCtx {
    const char* lib_filter;   // nullptr = patch every library
    const char* sym_name;
    void*       new_func;
    int         patched;
};

const char* base_name(const char* path) {
    if (!path || !*path) return "";
    const char* s = strrchr(path, '/');
    return s ? s + 1 : path;
}

bool patch_slot(void** slot, void* new_func) {
    if (*slot == new_func) return true;                 // already ours
    const uintptr_t pg = (uintptr_t)getpagesize();
    uintptr_t page = (uintptr_t)slot & ~(pg - 1);
    if (mprotect((void*)page, pg, PROT_READ | PROT_WRITE) != 0) return false;
    *slot = new_func;
    __builtin___clear_cache((char*)slot, (char*)(slot + 1));
    mprotect((void*)page, pg, PROT_READ);
    return true;
}

int scan_relocs(uintptr_t bias, const ElfW(Rela)* rel, size_t sz,
                const ElfW(Sym)* symtab, const char* strtab,
                const char* lib, PatchCtx* c) {
    int n = 0;
    if (!rel || !sz || !symtab || !strtab) return 0;
    for (size_t i = 0; i < sz / sizeof(ElfW(Rela)); ++i) {
        uint32_t type = ELF64_R_TYPE(rel[i].r_info);
        if (type != R_AARCH64_JUMP_SLOT && type != R_AARCH64_GLOB_DAT &&
            type != R_AARCH64_ABS64) continue;
        size_t si = ELF64_R_SYM(rel[i].r_info);
        if (!si) continue;
        if (strcmp(strtab + symtab[si].st_name, c->sym_name) != 0) continue;
        void** slot = (void**)(bias + rel[i].r_offset);
        if (patch_slot(slot, c->new_func)) {
            HLOGI("patched %s!%s slot=%p", lib, c->sym_name, (void*)slot);
            ++n;
        } else {
            HLOGE("mprotect failed for %s!%s", lib, c->sym_name);
        }
    }
    return n;
}

int phdr_cb(struct dl_phdr_info* info, size_t, void* data) {
    PatchCtx* c = (PatchCtx*)data;
    const char* name = base_name(info->dlpi_name);

    if (c->lib_filter && strcmp(name, c->lib_filter) != 0) return 0;
    // Never patch ourselves, the loader or libEGL itself.
    if (strstr(name, "libaxiom_vr_mod") || strstr(name, "linker") ||
        strcmp(name, "libEGL.so") == 0)
        return 0;

    uintptr_t bias = (uintptr_t)info->dlpi_addr;
    const ElfW(Dyn)* dyn = nullptr;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) {
            dyn = (const ElfW(Dyn)*)(bias + info->dlpi_phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return 0;

    const ElfW(Rela)* jmprel = nullptr; size_t pltsz = 0;
    const ElfW(Rela)* rela   = nullptr; size_t relasz = 0;
    const ElfW(Sym)*  symtab = nullptr;
    const char*       strtab = nullptr;

    for (; dyn->d_tag != DT_NULL; ++dyn) {
        switch (dyn->d_tag) {
            case DT_JMPREL:   jmprel = (const ElfW(Rela)*)dyn->d_un.d_ptr; break;
            case DT_PLTRELSZ: pltsz  = dyn->d_un.d_val;                    break;
            case DT_RELA:     rela   = (const ElfW(Rela)*)dyn->d_un.d_ptr; break;
            case DT_RELASZ:   relasz = dyn->d_un.d_val;                    break;
            case DT_STRTAB:   strtab = (const char*)dyn->d_un.d_ptr;       break;
            case DT_SYMTAB:   symtab = (const ElfW(Sym)*)dyn->d_un.d_ptr;  break;
            default: break;
        }
    }

    int n = 0;
    n += scan_relocs(bias, jmprel, pltsz,  symtab, strtab, name, c);
    n += scan_relocs(bias, rela,   relasz, symtab, strtab, name, c);
    c->patched += n;
    return 0;   // keep walking – patch it everywhere
}

} // namespace

// Patch the import of `sym_name` inside `lib_name` (nullptr = all libs).
bool axiom_patch_got(const char* lib_name, const char* sym_name,
                     void* new_func, void** /*old_func*/) {
    PatchCtx c{ lib_name, sym_name, new_func, 0 };
    dl_iterate_phdr(phdr_cb, &c);
    if (c.patched == 0) {
        HLOGE("symbol '%s' not found in GOT of %s",
              sym_name, lib_name ? lib_name : "any library");
    }
    return c.patched > 0;
}

// Hook `sym_name` everywhere; *real_func gets the genuine libEGL impl.
bool axiom_hook_unity(const char* sym_name, void* new_func, void** real_func) {
    if (real_func && !*real_func) {
        void* egl = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
        if (egl) {
            void* sym = dlsym(egl, sym_name);
            if (sym) {
                *real_func = sym;
                HLOGI("real %s = %p (libEGL.so)", sym_name, sym);
            }
            // keep libEGL mapped – do NOT dlclose
        }
    }
    // Patch every loaded library (Unity, il2cpp, main, GameActivity, …).
    return axiom_patch_got(nullptr, sym_name, new_func, nullptr);
}

#endif // __ANDROID__
