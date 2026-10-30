# Axiom VR Mod – Animated ImGui Overlay
> [made by TagtusVR]

An ARM64 shared library (`libaxiom_vr_mod.so`) that drops an **animated orange-to-green gradient** Dear ImGui overlay into any VR game or application.

---

## Features
- 🎨 **Animated gradient theme** – orange ↔ green sinusoidal cycle (fully customisable period)
- 🪟 **Draggable floating menu** with module toggles
- 🔧 **Built-in VR mod modules** – Fly, NoClip, Infinite Jump, Speed Hack, ESP, Auto-Farm
- 📜 **Log console** inside the overlay
- 🏗️ **GitHub Actions CI** – auto-builds ARM64 `.so` on every push

---

## Quick Start

```bash
git clone https://github.com/camdendees31-jpg/vr-imgui-mod.git
cd vr-imgui-mod
# Fetch Dear ImGui (pinned, same as CI)
git clone --depth 1 --branch v1.90.4 https://github.com/ocornut/imgui.git imgui
```

### Android / Quest (ARM64)
Requires Android NDK r26+.
```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
# Output: build/libaxiom_vr_mod.so
```

### Desktop (Linux, for testing)
```bash
sudo apt install libglfw3-dev libgl1-mesa-dev
cmake -S . -B build_desktop -DCMAKE_BUILD_TYPE=Release
cmake --build build_desktop --parallel
./build_desktop/axiom_example   # press F1 to toggle menu
```

---

## Integrating into your VR app

```cpp
// In your render loop:
#include "axiom_theme.h"   // Axiom::ApplyTheme(), GradientTitleBar()

extern "C" {
  void axiom_init();
  bool axiom_render_frame();
  void axiom_toggle_menu();
  void axiom_shutdown();
}

// Startup:
axiom_init();

// Every frame (after ImGui::NewFrame()):
axiom_render_frame();

// Toggle overlay (e.g., controller button):
axiom_toggle_menu();
```

---

## GitHub Actions
Every push to `main` triggers:
1. **Android ARM64 build** → uploads `libaxiom_vr_mod.so` as an artifact
2. **Linux desktop smoke-test** build
3. On version tags (`v*`) → creates a GitHub Release with the `.so` attached

---

## Customising the gradient
Edit `Axiom::GradientColor()` in `include/axiom_theme.h`:
```cpp
// Change period (seconds per cycle):
Axiom::ApplyTheme(time_sec, /*period_sec=*/ 5.0f);

// Change colours – tweak the orange/green RGB values in GradientColor()
```
