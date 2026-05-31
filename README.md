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

## 🛠️ How to Build & Run (Standalone)

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
*   `ViewportWindow.cpp` / `ViewportWindow.h` - The core reusable module for MFC/Win32 integration.

---

## 🔌 Integration Guide for Legacy MFC Codebases

To embed the `ViewportWindow` into an existing MFC application, follow these steps:

### 1. Add Files to Project
Add `ViewportWindow.cpp` and `ViewportWindow.h` to your Visual Studio project (`.vcxproj`). Since these files do not use MFC's precompiled headers, right-click `ViewportWindow.cpp` in Visual Studio -> **Properties** -> **C/C++** -> **Precompiled Headers** and set it to **Not Using Precompiled Headers**.

### 2. Link OpenGL Libraries
In your project properties (`.vcxproj`), go to **Linker** -> **Input** -> **Additional Dependencies** and add:
```text
opengl32.lib;glu32.lib;
```

### 3. Add to your Dialog Header (`.h`)
Include the header and add an instance of the `ViewportWindow` to your target dialog class (e.g., `CU5DPrintR1Dlg`).

```cpp
#include "ViewportWindow.h"

class CU5DPrintR1Dlg : public CDialog
{
// ...
public:
    ViewportWindow m_3DViewport;
// ...
};
```

### 4. Initialize in `OnInitDialog` (`.cpp`)
Spawn the window over a target control (e.g., an existing picture control container).

```cpp
BOOL CU5DPrintR1Dlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Find the target rect from an existing control
    CRect rectPict;
    m_ToolPathDlg.m_PictCNC.GetClientRect(&rectPict);
    
    // Initialize the OpenGL viewport over it
    m_3DViewport.Create(m_ToolPathDlg.m_PictCNC.GetSafeHwnd(), rectPict.Width(), rectPict.Height());

    return TRUE;
}
```

### 5. Feed Toolpath Data (`.cpp`)
Replace any legacy GDI drawing routines (e.g., `MoveTo`/`LineTo`) with the new API.

```cpp
void CU5DPrintR1Dlg::ShowGCDPath(int nFlag)
{
    int count = theApp.PreviewList.GetCount();
    if (count == 0) return;

    // 1. Clear previous toolpath
    m_3DViewport.ClearToolpath();

    POSITION pos = theApp.PreviewList.GetHeadPosition();
    for (int i = 0; i < count; i++)
    {
        PREVIEWPT ptTemp = theApp.PreviewList.GetNext(pos);
        
        // 2. Add each point (X, Y, Z, isExtruding)
        // Assume ptTemp.nType == 1 means G1 (extrusion)
        m_3DViewport.AddToolpathPoint(
            ptTemp.ptTarget.X[0], 
            ptTemp.ptTarget.X[1], 
            ptTemp.ptTarget.X[2], 
            ptTemp.nType == 1
        );
    }

    // 3. Push the batched points to the GPU
    m_3DViewport.CommitToolpathToGPU();
}
```
