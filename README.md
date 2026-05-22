# 3D Toolpath Viewport (Prototype #1)

A lightweight, zero-dependency 3D G-code toolpath visualizer built natively using C++17, raw Win32 APIs, and OpenGL.

This is a standalone, modular component designed to be 100% compatible with legacy Windows architectures (such as MFC or raw Win32 systems).

## 🚀 Key Features

*   **Native Win32 & OpenGL:** No external UI framework dependencies. Initializes a hardware-accelerated 3D context using low-level Windows APIs.
*   **Camera Controls:** 
    *   *Left-Click + Drag:* Arcball rotation around the build plate center.
    *   *Middle-Click + Drag:* Pan (translate) view.
    *   *Scroll Wheel:* Zoom in and out.
*   **G-Code Parser & Renderer:** Processes linear movement commands (`G0`/`G1`), separating extrusion toolpaths (Orange) from travel paths (Blue/Grey). Uses efficient OpenGL Client-Side Vertex Arrays.
*   **Interactive Simulation Slider:** A custom 2D orthographic overlay slider on the right side of the screen allows scrubbing through the toolpath point-by-point.
*   **Heads-Up Display (HUD):** Shows active point index, total parsed points, and active machine XYZ coordinates.

## 🛠️ How to Build & Run

### Prerequisites
*   Windows 10/11
*   C++17 compliant compiler (such as MSVC)
*   CMake (3.16+)

### Build via Command Line
```bash
cmake -S . -B build
cmake --build build --config Release
./build/Release/PrintEngine.exe
```

### Build in Visual Studio (Recommended)
1. Open Visual Studio.
2. Select **Open a local folder** and choose the project directory.
3. Select `PrintEngine.exe` as the startup item and press **F5** to build and run.

## 📁 Project Structure

*   `main.cpp` - Entry point containing windowing loop, G-code parsing, camera controls, and rendering.
*   `CMakeLists.txt` - Project build configuration.
*   `data/` - Directory containing sample G-code files.
