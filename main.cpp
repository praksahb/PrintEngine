#include <windows.h>
#include <fstream>
#include <string>
#include <sstream>
#include "ViewportWindow.h"

// Simulated File Parsing (just like the legacy app will do)
void LoadAndFeedGCode(const char* filepath, ViewportWindow& viewport) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        MessageBox(NULL, "Failed to open G-Code file for testing!", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    viewport.ClearToolpath();
    
    std::string line;
    float currentX = 0.0f, currentY = 0.0f, currentZ = 0.0f;
    
    while (std::getline(file, line)) {
        if (line.rfind("G0", 0) == 0 || line.rfind("G1", 0) == 0) {
            bool hasX = false, hasY = false, hasZ = false, hasE = false;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                if (token.length() > 1) {
                    if (token[0] == 'X') { currentX = std::stof(token.substr(1)); hasX = true; }
                    else if (token[0] == 'Y') { currentY = std::stof(token.substr(1)); hasY = true; }
                    else if (token[0] == 'Z') { currentZ = std::stof(token.substr(1)); hasZ = true; }
                    else if (token[0] == 'E') { hasE = true; }
                }
            }
            
            bool isExtrude = false;
            if (line.rfind("G1", 0) == 0) {
                if (hasE || hasX || hasY) {
                    isExtrude = true;
                }
            }
            
            // Feed the parsed point to our new Component!
            viewport.AddToolpathPoint(currentX, currentY, currentZ, isExtrude);
        }
    }
    
    // Tell the GPU we are done adding points
    viewport.CommitToolpathToGPU();
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    // Instantiate our new modular Viewport Component
    ViewportWindow viewport;
    
    // Create it as a standalone window (hParent = NULL) for testing
    if (!viewport.Create(NULL, 1280, 720)) {
        return 0;
    }

    // Load data from file and push it through our new API
    LoadAndFeedGCode("d:/Projects/PrintEngine/data/combined_print_2.gcode", viewport);

    // Standard Windows Message Loop for our standalone test harness
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
