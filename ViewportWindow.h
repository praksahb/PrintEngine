#pragma once

#include <windows.h>
#include <vector>

// Forward declaration of internal struct to hide implementation details from the legacy code
struct ViewportState;

class ViewportWindow {
public:
    ViewportWindow();
    ~ViewportWindow();

    // Creates the OpenGL window. 
    // If hParent is NULL, it creates a standalone window (useful for our testing).
    // If hParent is provided, it creates a child window inside the legacy UI panel.
    bool Create(HWND hParent, int width, int height);
    
    // API for the legacy codebase to feed us G-Code data
    void ClearToolpath();
    
    // Add a single pre-parsed point.
    // We expect the legacy code to call this in a loop for each point.
    void AddToolpathPoint(float x, float y, float z, bool isExtrude);
    
    // Call this after all points are added to finalize the buffers and draw
    void CommitToolpathToGPU();

    // The HWND of the viewport window
    HWND GetHWND() const;

private:
    // Window procedure for handling OpenGL context and input events independently
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Instance-specific message handler
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Rendering functions
    void Render();
    void DrawBuildVolume();
    void DrawGrid();
    void DrawGCode();

    HWND m_hwnd;
    HDC m_hdc;
    HGLRC m_hrc;

    // We use a PIMPL-like idiom or just hide state here so we don't pollute 
    // the legacy MFC codebase with OpenGL headers in our .h file.
    ViewportState* m_state;
};
