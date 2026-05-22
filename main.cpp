#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>

// --- Phase 2 & 4: Camera State (Arcball & Pan) ---
bool isDragging = false;
bool isPanning = false;
bool isDraggingSlider = false;
int lastMouseX = 0;
int lastMouseY = 0;

int maxIndex = 1;
int displayIndex = 1;
GLuint fontBase = 0;

float cameraYaw = 45.0f;
float cameraPitch = 30.0f;
float cameraZoom = -200.0f; // Pulled back to see a standard 200mm build plate

// Target point the camera orbits around (Now centered at 0,0,0)
float cameraTargetX = 0.0f;
float cameraTargetY = 0.0f;
float cameraTargetZ = 0.0f;

// --- Camera Settings ---
float orbitSensitivity = 0.35f; 
float zoomSensitivity = 2.5f;
float panSensitivity = 0.25f;

// --- Phase 3 & 4: G-Code Data Structure & Parsing ---
struct GCodeSegment {
    float x1, y1, z1;
    float x2, y2, z2;
    bool isExtrude;
    int layer;
};
std::vector<GCodeSegment> mockGCode;
std::vector<float> vertexArray;
std::vector<float> colorArray;

// Phase 4: Parse the actual G-Code file!
void LoadGCode(const char* filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        MessageBox(NULL, "Failed to open G-Code file! Check if 'data/combined_print.gcode' exists.", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    mockGCode.clear(); // Clear old data
    vertexArray.clear();
    colorArray.clear();
    std::string line;
    float currentX = 0.0f, currentY = 0.0f, currentZ = 0.0f;
    float prevX = 0.0f, prevY = 0.0f, prevZ = 0.0f;
    bool hasPrevious = false;
    int currentLayer = 1;
    
    while (std::getline(file, line)) {
        // Look for Linear Move commands (G0 or G1)
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
            
            // Differentiate between Travel and Extrude
            // For FDM: relies on E. For DED: G1 with X/Y is extruding, G1 with only Z is a layer hop (travel).
            bool isExtrude = false;
            if (line.rfind("G1", 0) == 0) {
                if (hasE || hasX || hasY) {
                    isExtrude = true;
                } else if (hasZ && !hasX && !hasY && !hasE) {
                    currentLayer++; // Pure Z move = Next Layer
                }
            }
            
            if (hasPrevious) {
                // OpenGL maps: Y is Up. So we map Printer Z to GL Y!
                mockGCode.push_back({ prevX, prevZ, prevY, currentX, currentZ, currentY, isExtrude, currentLayer });
                
                // Add start and end vertices for GL_LINES
                vertexArray.push_back(prevX);
                vertexArray.push_back(prevZ);
                vertexArray.push_back(prevY);
                vertexArray.push_back(currentX);
                vertexArray.push_back(currentZ);
                vertexArray.push_back(currentY);
                
                // Assign color based on move type (Orange for Extrude, Blue for Travel)
                float r = 0.2f, g = 0.3f, b = 0.5f;
                if (isExtrude) {
                    r = 1.0f; g = 0.6f; b = 0.0f;
                }
                colorArray.push_back(r);
                colorArray.push_back(g);
                colorArray.push_back(b);
                colorArray.push_back(r);
                colorArray.push_back(g);
                colorArray.push_back(b);
            }
            
            prevX = currentX; prevY = currentY; prevZ = currentZ;
            hasPrevious = true;
        }
    }
    
    maxIndex = mockGCode.size();
    displayIndex = maxIndex;
}

void DrawBuildVolume() {
    // 200x200x200 wireframe build volume
    // Assuming X: [-100, 100], Y: [0, 200], Z: [-100, 100]
    glColor3f(0.3f, 0.3f, 0.3f);
    glLineWidth(1.0f);
    
    glBegin(GL_LINE_LOOP); // Bottom Face
    glVertex3f(-100.0f, 0.0f, -100.0f); glVertex3f(100.0f, 0.0f, -100.0f);
    glVertex3f(100.0f, 0.0f, 100.0f); glVertex3f(-100.0f, 0.0f, 100.0f);
    glEnd();
    
    glBegin(GL_LINE_LOOP); // Top Face
    glVertex3f(-100.0f, 200.0f, -100.0f); glVertex3f(100.0f, 200.0f, -100.0f);
    glVertex3f(100.0f, 200.0f, 100.0f); glVertex3f(-100.0f, 200.0f, 100.0f);
    glEnd();
    
    glBegin(GL_LINES); // Vertical Edges
    glVertex3f(-100.0f, 0.0f, -100.0f); glVertex3f(-100.0f, 200.0f, -100.0f);
    glVertex3f(100.0f, 0.0f, -100.0f); glVertex3f(100.0f, 200.0f, -100.0f);
    glVertex3f(100.0f, 0.0f, 100.0f); glVertex3f(100.0f, 200.0f, 100.0f);
    glVertex3f(-100.0f, 0.0f, 100.0f); glVertex3f(-100.0f, 200.0f, 100.0f);
    glEnd();
}

// --- Rendering Functions ---
void DrawGrid() {
    glBegin(GL_LINES);
    glColor3f(0.3f, 0.3f, 0.3f);
    
    // Draw a 200x200 mm build plate grid centered at origin (-100 to 100)
    for (float i = -100; i <= 100; i += 10.0f) {
        glVertex3f(i, 0.0f, -100.0f); glVertex3f(i, 0.0f, 100.0f);
        glVertex3f(-100.0f, 0.0f, i); glVertex3f(100.0f, 0.0f, i);
    }
    
    // Draw XYZ Axes at the 0,0,0 corner (Home position)
    glColor3f(1.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(20.0f, 0.0f, 0.0f); // X Red
    glColor3f(0.0f, 1.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 20.0f, 0.0f); // Y Green (Z in GCode)
    glColor3f(0.0f, 0.0f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 20.0f); // Z Blue  (Y in GCode)
    
    glEnd();
}

void DrawGCode() {
    if (displayIndex > 0 && !vertexArray.empty()) {
        glLineWidth(2.0f); 
        
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        
        glVertexPointer(3, GL_FLOAT, 0, vertexArray.data());
        glColorPointer(3, GL_FLOAT, 0, colorArray.data());
        
        glDrawArrays(GL_LINES, 0, displayIndex * 2);
        
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        
        glLineWidth(1.0f); 
    }
}

// --- Window Message Handler ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            PostQuitMessage(0); return 0;
            
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                int width = LOWORD(lParam); int height = HIWORD(lParam);
                if (height == 0) height = 1;
                glViewport(0, 0, width, height);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                gluPerspective(45.0, (double)width / (double)height, 0.1, 1000.0); // Increased far plane to 1000
                glMatrixMode(GL_MODELVIEW); 
            }
            return 0;

        case WM_LBUTTONDOWN: { // Orbit or Slider
            int x = GET_X_LPARAM(lParam); int y = GET_Y_LPARAM(lParam);
            RECT rect; GetClientRect(hwnd, &rect);
            int sliderX = rect.right - 40;
            
            if (x >= sliderX - 10 && x <= sliderX + 30) {
                isDraggingSlider = true;
                SetCapture(hwnd);
                
                int sliderY = 40; int sliderHeight = rect.bottom - 80;
                float ratio = 1.0f - (float)(y - sliderY) / (float)sliderHeight;
                if (ratio < 0.0f) ratio = 0.0f; if (ratio > 1.0f) ratio = 1.0f;
                displayIndex = (int)(ratio * maxIndex);
                if (displayIndex < 0) displayIndex = 0;
            } else {
                isDragging = true;
                lastMouseX = x; lastMouseY = y;
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
            isDragging = false;
            isDraggingSlider = false;
            if (!isPanning) ReleaseCapture();
            return 0;

        case WM_MBUTTONDOWN: // Pan
            isPanning = true;
            lastMouseX = GET_X_LPARAM(lParam); lastMouseY = GET_Y_LPARAM(lParam);
            SetCapture(hwnd);
            return 0;
        case WM_MBUTTONUP:
            isPanning = false;
            if (!isDragging) ReleaseCapture();
            return 0;
            
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam); int y = GET_Y_LPARAM(lParam);
            int dx = x - lastMouseX; int dy = y - lastMouseY;

            if (isDraggingSlider) {
                RECT rect; GetClientRect(hwnd, &rect);
                int sliderY = 40; int sliderHeight = rect.bottom - 80;
                float ratio = 1.0f - (float)(y - sliderY) / (float)sliderHeight;
                if (ratio < 0.0f) ratio = 0.0f; if (ratio > 1.0f) ratio = 1.0f;
                displayIndex = (int)(ratio * maxIndex);
                if (displayIndex < 0) displayIndex = 0;
            }
            else if (isDragging) {
                cameraYaw += dx * orbitSensitivity;
                cameraPitch += dy * orbitSensitivity;
                if (cameraPitch > 90.0f) cameraPitch = 90.0f;
                if (cameraPitch < -90.0f) cameraPitch = -90.0f;
            }
            else if (isPanning) {
                // Calculate pan directions based on current camera Yaw
                float yawRad = cameraYaw * (3.14159f / 180.0f);
                float rightX = cos(yawRad);
                float rightZ = sin(yawRad);
                
                cameraTargetX -= rightX * dx * panSensitivity;
                cameraTargetZ -= rightZ * dx * panSensitivity;
                cameraTargetY += dy * panSensitivity; // Up/down screen space
            }
            lastMouseX = x; lastMouseY = y;
            return 0;
        }
            
        case WM_MOUSEWHEEL: {
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            cameraZoom += (zDelta / 120.0f) * zoomSensitivity;
            if (cameraZoom > -1.0f) cameraZoom = -1.0f;
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// --- Main Application Entry ---
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    const char CLASS_NAME[] = "ForgeOS_Native_Class";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.style = CS_OWNDC;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "Unnati 5D - ForgeOS [Phase 4: Real G-Code & Pan]", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) return 0;

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC hrc = wglCreateContext(hdc); wglMakeCurrent(hdc, hrc);

    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH); 

    // --- PHASE 4: HUD Font Setup ---
    fontBase = glGenLists(96);
    HFONT hFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Consolas");
    SelectObject(hdc, hFont);
    wglUseFontBitmaps(hdc, 32, 96, fontBase);

    // --- PHASE 4: Load the File! ---
    // Using an absolute path because Visual Studio's default working directory is deep inside the 'out/build/' folder!
    LoadGCode("d:/Projects/PrintEngine/data/combined_print_2.gcode");

    ShowWindow(hwnd, nCmdShow);

    MSG msg; bool running = true;
    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity(); 
        
        // Arcball Camera Logic with Panning Target
        glTranslatef(0.0f, 0.0f, cameraZoom); 
        glRotatef(cameraPitch, 1.0f, 0.0f, 0.0f);
        glRotatef(cameraYaw, 0.0f, 1.0f, 0.0f);
        
        // Shift world based on pan target
        glTranslatef(-cameraTargetX, -cameraTargetY, -cameraTargetZ);
        
        DrawBuildVolume();
        DrawGrid();
        DrawGCode();

        // --- 2D UI Pass (Slider & HUD) ---
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        
        RECT rect; GetClientRect(hwnd, &rect);
        gluOrtho2D(0, rect.right, rect.bottom, 0); // Origin at top-left
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        glDisable(GL_DEPTH_TEST);
        
        // --- Draw HUD Text ---
        glColor3f(1.0f, 1.0f, 1.0f);
        char buffer[256];
        
        glRasterPos2i(10, 25);
        sprintf_s(buffer, "Unnati 5D Viewport - DED Simulation");
        glPushAttrib(GL_LIST_BIT);
        glListBase(fontBase - 32);
        glCallLists((GLsizei)strlen(buffer), GL_UNSIGNED_BYTE, buffer);
        
        glRasterPos2i(10, 50);
        sprintf_s(buffer, "Points: %d / %d", displayIndex, (int)mockGCode.size());
        glCallLists((GLsizei)strlen(buffer), GL_UNSIGNED_BYTE, buffer);
        
        if (displayIndex > 0 && displayIndex <= mockGCode.size()) {
            auto& seg = mockGCode[displayIndex - 1];
            // Remember: seg.y2 is GL_Y (Printer Z), seg.z2 is GL_Z (Printer Y)
            glRasterPos2i(10, 75);
            sprintf_s(buffer, "Machine Pos: X=%.3f Y=%.3f Z=%.3f", seg.x2, seg.z2, seg.y2);
            glCallLists((GLsizei)strlen(buffer), GL_UNSIGNED_BYTE, buffer);
        }
        glPopAttrib();

        // --- Draw Slider ---
        int sliderX = rect.right - 40;
        int sliderY = 40;
        int sliderHeight = rect.bottom - 80;
        int sliderWidth = 20;
        
        // Draw Slider Track
        glColor3f(0.2f, 0.2f, 0.2f);
        glBegin(GL_QUADS);
        glVertex2i(sliderX, sliderY);
        glVertex2i(sliderX + sliderWidth, sliderY);
        glVertex2i(sliderX + sliderWidth, sliderY + sliderHeight);
        glVertex2i(sliderX, sliderY + sliderHeight);
        glEnd();
        
        // Draw Slider Thumb (Inverted so bottom is index 0)
        float fillRatio = (float)displayIndex / (float)(maxIndex > 0 ? maxIndex : 1);
        int thumbY = sliderY + sliderHeight - (int)(fillRatio * sliderHeight);
        
        glColor3f(0.8f, 0.8f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2i(sliderX - 5, thumbY - 10);
        glVertex2i(sliderX + sliderWidth + 5, thumbY - 10);
        glVertex2i(sliderX + sliderWidth + 5, thumbY + 10);
        glVertex2i(sliderX - 5, thumbY + 10);
        glEnd();
        
        glEnable(GL_DEPTH_TEST);
        
        glPopMatrix(); // Restore ModelView
        glMatrixMode(GL_PROJECTION);
        glPopMatrix(); // Restore Projection
        glMatrixMode(GL_MODELVIEW);

        SwapBuffers(hdc);
        Sleep(16); 
    }

    wglMakeCurrent(NULL, NULL); wglDeleteContext(hrc); ReleaseDC(hwnd, hdc);
    return 0;
}
