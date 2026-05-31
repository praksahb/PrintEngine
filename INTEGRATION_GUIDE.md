# 3D Viewport Integration Guide

This guide explains how to integrate the new 3D OpenGL viewport (`ViewportWindow.h` and `ViewportWindow.cpp`) into your main application (`U5DPrintR1`).

## 1. Add Files to Project
1. Copy `ViewportWindow.h` and `ViewportWindow.cpp` into your main program's source directory (e.g., `MainProgram\U5DPrint-NewDriver`).
2. Add these two files to your Visual Studio project (Right-click project -> Add -> Existing Item).

## 2. Link OpenGL Libraries
The 3D viewport requires OpenGL.
1. Right-click your project in Visual Studio -> **Properties**.
2. Go to **Linker** -> **Input**.
3. In **Additional Dependencies**, add `opengl32.lib;glu32.lib;` (if not already present).

## 3. Update the Main Dialog Header
In your main dialog header file (e.g., `U5DPrintR1Dlg.h`), include the new header and add a member variable for the viewport:

```cpp
// Add this near the top with other includes
#include "ViewportWindow.h"

// Inside your CU5DPrintR1Dlg class declaration, add:
public:
    ViewportWindow m_3DViewport;
```

## 4. Initialize the Viewport
In your main dialog's initialization function (e.g., `CU5DPrintR1Dlg::OnInitDialog()`), locate where the `m_ToolPathDlg` is created and positioned. Add the following code immediately after `m_ToolPathDlg` is set up:

```cpp
	// (Existing Code)
	m_ToolPathDlg.Create(IDD_DIALOG_TOOLPATH,&m_TabGraph);
	m_ToolPathDlg.SetWindowPos(NULL,5,5,rect.Width()-10,rect.Height()-30,SWP_SHOWWINDOW);
	pwndGraph = &m_ToolPathDlg;
	
	// --- NEW CODE: Initialize 3D Viewport ---
	// 1. Resize the picture control to perfectly fit the tab page area (prevents clipping the slider)
	CRect dlgRect;
	m_ToolPathDlg.GetClientRect(&dlgRect);
	m_ToolPathDlg.m_PictCNC.SetWindowPos(NULL, 0, 0, dlgRect.Width(), dlgRect.Height(), SWP_NOMOVE | SWP_NOZORDER);

	// 2. Create the 3D Viewport as a child of the Picture Control
	CRect rectPict;
	m_ToolPathDlg.m_PictCNC.GetClientRect(&rectPict);
	m_3DViewport.Create(m_ToolPathDlg.m_PictCNC.GetSafeHwnd(), rectPict.Width(), rectPict.Height());
	// ----------------------------------------
```

## 5. Feed Toolpath Data to the Viewport
When a G-code file is loaded and parsed, you need to send the generated points to the 3D viewport. 
In your `CU5DPrintR1Dlg::ShowGCDPath(int index)` (or wherever you iterate over `theApp.PreviewList`), clear the previous points, add the new ones, and commit them:

```cpp
	// 1. Clear old data before parsing the new list
	m_3DViewport.ClearToolpath();

	POSITION pos = theApp.PreviewList.GetHeadPosition();
	while(pos) {
		TPT ptTemp = theApp.PreviewList.GetNext(pos);

		// 2. Feed the point to the 3D viewport (scale down by 1000 to convert to mm if needed)
		m_3DViewport.AddToolpathPoint(
			(float)ptTemp.ptTarget.X / 1000.0f,
			(float)ptTemp.ptTarget.Y / 1000.0f,
			(float)ptTemp.ptTarget.Z / 1000.0f,
			ptTemp.Type
		);
	}

	// 3. Finalize and push data to the GPU for rendering
	m_3DViewport.CommitToolpathToGPU();
```

## 6. Update the Viewport During Playback
When you simulate the print or the machine moves, you can update the slider and the highlighted point by setting the display index. 
Whenever your current execution step changes, call:

```cpp
	// `currentIndex` is the step you are currently at in the PreviewList
	m_3DViewport.SetDisplayIndex(currentIndex);
```

## Mouse Controls
Once integrated, the 3D viewport will automatically handle its own mouse inputs:
* **Left Click + Drag**: Rotate the 3D view.
* **Right Click + Drag**: Pan the 3D view.
* **Mouse Wheel**: Zoom in and out.
* **Vertical Slider (Right Side)**: Click and drag to manually scrub back and forth through the toolpath sequence.
