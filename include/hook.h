#pragma once
#ifdef __ANDROID__
// ARM64 GOT patcher [made by TagtusVR]
bool axiom_patch_got(const char* lib_name, const char* sym_name,
                     void* new_func, void** old_func);
bool axiom_hook_unity(const char* sym_name, void* new_func, void** real_func);
#endif
