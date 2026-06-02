#include "ViewportWindow.h"
#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <vector>
#include <string>
#include <cmath>

// Hide implementation details and state from the header
struct ViewportState {
    bool isDragging;
    bool isPanning;
    bool isDraggingSlider;
    int lastMouseX;
    int lastMouseY;

    int maxIndex;
    int displayIndex;
    GLuint fontBase;

    float cameraYaw;
    float cameraPitch;
    float cameraZoom;

    float cameraTargetX;
    float cameraTargetY;
    float cameraTargetZ;

    float orbitSensitivity; 
    float zoomSensitivity;
    float panSensitivity;

    // We store the raw points for HUD
    struct GCodeSegment {
        float x1, y1, z1;
        float x2, y2, z2;
        bool isExtrude;
        int layer;
    };
    std::vector<GCodeSegment> mockGCode;
    std::vector<float> vertexArray;
    std::vector<float> colorArray;

    // Used for keeping track of the previous point when building segments
    float prevX;
    float prevY;
    float prevZ;
    bool hasPrevious;
    int currentLayer;

    ViewportState() : 
        isDragging(false), isPanning(false), isDraggingSlider(false),
        lastMouseX(0), lastMouseY(0), maxIndex(0), displayIndex(0), fontBase(0),
        cameraYaw(45.0f), cameraPitch(30.0f), cameraZoom(-200.0f),
        cameraTargetX(0.0f), cameraTargetY(0.0f), cameraTargetZ(0.0f),
        orbitSensitivity(0.35f), zoomSensitivity(2.5f), panSensitivity(0.25f),
        prevX(0.0f), prevY(0.0f), prevZ(0.0f), hasPrevious(false), currentLayer(1)
    {}
};

ViewportWindow::ViewportWindow() : m_hwnd(NULL), m_hdc(NULL), m_hrc(NULL) {
    m_state = new ViewportState();
}

ViewportWindow::~ViewportWindow() {
    if (m_hrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(m_hrc);
    }
    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
    }
    delete m_state;
}

bool ViewportWindow::Create(HWND hParent, int width, int height) {
    const char CLASS_NAME[] = "ViewportWindow_Class";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = ViewportWindow::WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.style = CS_OWNDC;
    RegisterClass(&wc);

    DWORD style = hParent ? (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN) : (WS_OVERLAPPEDWINDOW);

    m_hwnd = CreateWindowEx(
        0, CLASS_NAME, "3D Viewport",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        hParent, NULL, wc.hInstance, this
    );

    if (m_hwnd == NULL) return false;

    // Store pointer to ourselves in the HWND for the static WindowProc
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    m_hdc = GetDC(m_hwnd);
    PIXELFORMATDESCRIPTOR pfd = { 
        sizeof(PIXELFORMATDESCRIPTOR), 1, 
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, 
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 
    };
    SetPixelFormat(m_hdc, ChoosePixelFormat(m_hdc, &pfd), &pfd);
    m_hrc = wglCreateContext(m_hdc); 
    
    // Initialize OpenGL state
    wglMakeCurrent(m_hdc, m_hrc);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH); 

    m_state->fontBase = glGenLists(96);
    HFONT hFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Consolas");
    SelectObject(m_hdc, hFont);
    wglUseFontBitmaps(m_hdc, 32, 96, m_state->fontBase);
    DeleteObject(hFont);
    
    if (hParent == NULL) {
        ShowWindow(m_hwnd, SW_SHOW);
    }

    return true;
}

HWND ViewportWindow::GetHWND() const {
    return m_hwnd;
}

void ViewportWindow::ClearToolpath() {
    m_state->mockGCode.clear();
    m_state->vertexArray.clear();
    m_state->colorArray.clear();
    m_state->hasPrevious = false;
    m_state->currentLayer = 1;
    m_state->prevX = 0.0f;
    m_state->prevY = 0.0f;
    m_state->prevZ = 0.0f;
    m_state->maxIndex = 0;
    m_state->displayIndex = 0;
}

void ViewportWindow::AddToolpathPoint(float x, float y, float z, bool isExtrude) {
    if (m_state->hasPrevious) {
        // OpenGL maps: Y is Up. So we map Printer Z to GL Y!
        // Printer Coordinates: x=X, y=Y, z=Z
        // GL Coordinates: X=x, Y=z, Z=y
        ViewportState::GCodeSegment seg;
        seg.x1 = m_state->prevX; seg.y1 = m_state->prevZ; seg.z1 = m_state->prevY;
        seg.x2 = x; seg.y2 = z; seg.z2 = y;
        seg.isExtrude = isExtrude;
        seg.layer = m_state->currentLayer;
        m_state->mockGCode.push_back(seg);
        
        m_state->vertexArray.push_back(m_state->prevX);
        m_state->vertexArray.push_back(m_state->prevZ);
        m_state->vertexArray.push_back(m_state->prevY);
        m_state->vertexArray.push_back(x);
        m_state->vertexArray.push_back(z);
        m_state->vertexArray.push_back(y);
        
        float r = 0.2f, g = 0.3f, b = 0.5f; // Travel (Blue)
        if (isExtrude) {
            r = 1.0f; g = 0.6f; b = 0.0f; // Extrude (Orange)
        }
        
        m_state->colorArray.push_back(r); m_state->colorArray.push_back(g); m_state->colorArray.push_back(b);
        m_state->colorArray.push_back(r); m_state->colorArray.push_back(g); m_state->colorArray.push_back(b);
    }
    
    m_state->prevX = x;
    m_state->prevY = y;
    m_state->prevZ = z;
    m_state->hasPrevious = true;
    
    // Note: Z logic for layers was simplified here since the legacy app might pass us the raw points.
}

void ViewportWindow::CommitToolpathToGPU() {
    m_state->maxIndex = (int)m_state->mockGCode.size();
    m_state->displayIndex = m_state->maxIndex;
    
    // Auto-center and auto-zoom the camera based on the bounding box of the points
    if (!m_state->vertexArray.empty()) {
        float minX = m_state->vertexArray[0], maxX = minX;
        float minY = m_state->vertexArray[1], maxY = minY;
        float minZ = m_state->vertexArray[2], maxZ = minZ;
        
        for (size_t i = 0; i < m_state->vertexArray.size(); i += 3) {
            float vx = m_state->vertexArray[i];
            float vy = m_state->vertexArray[i+1];
            float vz = m_state->vertexArray[i+2];
            
            if (vx < minX) minX = vx;
            if (vx > maxX) maxX = vx;
            if (vy < minY) minY = vy;
            if (vy > maxY) maxY = vy;
            if (vz < minZ) minZ = vz;
            if (vz > maxZ) maxZ = vz;
        }
        
        m_state->cameraTargetX = (minX + maxX) / 2.0f;
        m_state->cameraTargetY = (minY + maxY) / 2.0f;
        m_state->cameraTargetZ = (minZ + maxZ) / 2.0f;
        
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        
        float maxDim = dx;
        if (dy > maxDim) maxDim = dy;
        if (dz > maxDim) maxDim = dz;
        
        if (maxDim < 1.0f) maxDim = 1.0f;
        
        m_state->cameraZoom = -maxDim * 1.5f;
        m_state->zoomSensitivity = maxDim / 50.0f;
        m_state->panSensitivity = maxDim / 500.0f;
    }
    
    // We can force a redraw immediately
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void ViewportWindow::DrawBuildVolume() {
    glColor3f(0.3f, 0.3f, 0.3f);
    glLineWidth(1.0f);
    
    glBegin(GL_LINE_LOOP); 
    glVertex3f(-100.0f, 0.0f, -100.0f); glVertex3f(100.0f, 0.0f, -100.0f);
    glVertex3f(100.0f, 0.0f, 100.0f); glVertex3f(-100.0f, 0.0f, 100.0f);
    glEnd();
    
    glBegin(GL_LINE_LOOP); 
    glVertex3f(-100.0f, 200.0f, -100.0f); glVertex3f(100.0f, 200.0f, -100.0f);
    glVertex3f(100.0f, 200.0f, 100.0f); glVertex3f(-100.0f, 200.0f, 100.0f);
    glEnd();
    
    glBegin(GL_LINES); 
    glVertex3f(-100.0f, 0.0f, -100.0f); glVertex3f(-100.0f, 200.0f, -100.0f);
    glVertex3f(100.0f, 0.0f, -100.0f); glVertex3f(100.0f, 200.0f, -100.0f);
    glVertex3f(100.0f, 0.0f, 100.0f); glVertex3f(100.0f, 200.0f, 100.0f);
    glVertex3f(-100.0f, 0.0f, 100.0f); glVertex3f(-100.0f, 200.0f, 100.0f);
    glEnd();
}

void ViewportWindow::DrawGrid() {
    glBegin(GL_LINES);
    glColor3f(0.3f, 0.3f, 0.3f);
    
    for (float i = -100; i <= 100; i += 10.0f) {
        glVertex3f(i, 0.0f, -100.0f); glVertex3f(i, 0.0f, 100.0f);
        glVertex3f(-100.0f, 0.0f, i); glVertex3f(100.0f, 0.0f, i);
    }
    
    glColor3f(1.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(20.0f, 0.0f, 0.0f); 
    glColor3f(0.0f, 1.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 20.0f, 0.0f); 
    glColor3f(0.0f, 0.0f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 20.0f); 
    glEnd();
}

void ViewportWindow::DrawGCode() {
    if (m_state->displayIndex > 0 && !m_state->vertexArray.empty()) {
        glLineWidth(2.0f); 
        
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        
        glVertexPointer(3, GL_FLOAT, 0, m_state->vertexArray.data());
        glColorPointer(3, GL_FLOAT, 0, m_state->colorArray.data());
        
        glDrawArrays(GL_LINES, 0, m_state->displayIndex * 2);
        
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        
        glLineWidth(1.0f); 
    }
}

void ViewportWindow::Render() {
    wglMakeCurrent(m_hdc, m_hrc);
    
    // Dynamically set up 3D projection (handles cases where WM_SIZE was missed during initialization)
    RECT rect; GetClientRect(m_hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (height == 0) height = 1;
    
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Calculate far plane dynamically so we never clip the object
    double farPlane = 1000.0;
    if (m_state->cameraZoom < -500.0f) {
        farPlane = (double)(-m_state->cameraZoom * 10.0f);
    }
    
    gluPerspective(45.0, (double)width / (double)height, 0.1, farPlane);
    glMatrixMode(GL_MODELVIEW);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity(); 
    
    glTranslatef(0.0f, 0.0f, m_state->cameraZoom); 
    glRotatef(m_state->cameraPitch, 1.0f, 0.0f, 0.0f);
    glRotatef(m_state->cameraYaw, 0.0f, 1.0f, 0.0f);
    
    glTranslatef(-m_state->cameraTargetX, -m_state->cameraTargetY, -m_state->cameraTargetZ);
    
    DrawBuildVolume();
    DrawGrid();
    DrawGCode();

    // 2D UI Pass
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    
    gluOrtho2D(0, rect.right, rect.bottom, 0); 
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    
    // Draw HUD
    glColor3f(1.0f, 1.0f, 1.0f);
    char buffer[256];
    
    glRasterPos2i(10, 25);
    sprintf_s(buffer, "3D Toolpath Viewport - Module");
    glPushAttrib(GL_LIST_BIT);
    glListBase(m_state->fontBase - 32);
    glCallLists((GLsizei)strlen(buffer), GL_UNSIGNED_BYTE, buffer);
    
    glRasterPos2i(10, 50);
    sprintf_s(buffer, "Points: %d / %d", m_state->displayIndex, (int)m_state->mockGCode.size());
    glCallLists((GLsizei)strlen(buffer), GL_UNSIGNED_BYTE, buffer);
    
    if (m_state->displayIndex > 0 && m_state->displayIndex <= (int)m_state->mockGCode.size()) {
        ViewportState::GCodeSegment& seg = m_state->mockGCode[m_state->displayIndex - 1];
        glRasterPos2i(10, 75);
        sprintf_s(buffer, "Machine Pos: X=%.3f Y=%.3f Z=%.3f", seg.x2, seg.z2, seg.y2);
        glCallLists((GLsizei)strlen(buffer), GL_UNSIGNED_BYTE, buffer);
    }
    glPopAttrib();

    // Draw Slider
    int sliderX = rect.right - 40;
    int sliderY = 40;
    int sliderHeight = rect.bottom - 80;
    int sliderWidth = 20;
    
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2i(sliderX, sliderY);
    glVertex2i(sliderX + sliderWidth, sliderY);
    glVertex2i(sliderX + sliderWidth, sliderY + sliderHeight);
    glVertex2i(sliderX, sliderY + sliderHeight);
    glEnd();
    
    float fillRatio = (float)m_state->displayIndex / (float)(m_state->maxIndex > 0 ? m_state->maxIndex : 1);
    int thumbY = sliderY + sliderHeight - (int)(fillRatio * sliderHeight);
    
    glColor3f(0.8f, 0.8f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2i(sliderX - 5, thumbY - 10);
    glVertex2i(sliderX + sliderWidth + 5, thumbY - 10);
    glVertex2i(sliderX + sliderWidth + 5, thumbY + 10);
    glVertex2i(sliderX - 5, thumbY + 10);
    glEnd();
    
    glEnable(GL_DEPTH_TEST);
    
    glPopMatrix(); 
    glMatrixMode(GL_PROJECTION);
    glPopMatrix(); 
    glMatrixMode(GL_MODELVIEW);

    SwapBuffers(m_hdc);
}

LRESULT CALLBACK ViewportWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ViewportWindow* pThis = NULL;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ViewportWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ViewportWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT ViewportWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    m_hwnd = hwnd;
    switch (uMsg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && m_hrc) {
                wglMakeCurrent(m_hdc, m_hrc);
                int width = LOWORD(lParam); int height = HIWORD(lParam);
                if (height == 0) height = 1;
                glViewport(0, 0, width, height);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                gluPerspective(45.0, (double)width / (double)height, 0.1, 1000.0); 
                glMatrixMode(GL_MODELVIEW); 
                Render();
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(m_hwnd, &ps);
            Render();
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: { 
            int x = GET_X_LPARAM(lParam); int y = GET_Y_LPARAM(lParam);
            RECT rect; GetClientRect(m_hwnd, &rect);
            int sliderX = rect.right - 40;
            
            if (x >= sliderX - 10 && x <= sliderX + 30) {
                m_state->isDraggingSlider = true;
                SetCapture(m_hwnd);
                
                int sliderY = 40; int sliderHeight = rect.bottom - 80;
                float ratio = 1.0f - (float)(y - sliderY) / (float)sliderHeight;
                if (ratio < 0.0f) ratio = 0.0f; if (ratio > 1.0f) ratio = 1.0f;
                m_state->displayIndex = (int)(ratio * m_state->maxIndex);
                if (m_state->displayIndex < 0) m_state->displayIndex = 0;
                InvalidateRect(m_hwnd, NULL, FALSE);
            } else {
                m_state->isDragging = true;
                m_state->lastMouseX = x; m_state->lastMouseY = y;
                SetCapture(m_hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
            m_state->isDragging = false;
            m_state->isDraggingSlider = false;
            if (!m_state->isPanning) ReleaseCapture();
            return 0;

        case WM_MBUTTONDOWN: 
            m_state->isPanning = true;
            m_state->lastMouseX = GET_X_LPARAM(lParam); m_state->lastMouseY = GET_Y_LPARAM(lParam);
            SetCapture(m_hwnd);
            return 0;
        case WM_MBUTTONUP:
            m_state->isPanning = false;
            if (!m_state->isDragging) ReleaseCapture();
            return 0;
            
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam); int y = GET_Y_LPARAM(lParam);
            int dx = x - m_state->lastMouseX; int dy = y - m_state->lastMouseY;

            if (m_state->isDraggingSlider) {
                RECT rect; GetClientRect(m_hwnd, &rect);
                int sliderY = 40; int sliderHeight = rect.bottom - 80;
                float ratio = 1.0f - (float)(y - sliderY) / (float)sliderHeight;
                if (ratio < 0.0f) ratio = 0.0f; if (ratio > 1.0f) ratio = 1.0f;
                m_state->displayIndex = (int)(ratio * m_state->maxIndex);
                if (m_state->displayIndex < 0) m_state->displayIndex = 0;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
            else if (m_state->isDragging) {
                m_state->cameraYaw += dx * m_state->orbitSensitivity;
                m_state->cameraPitch += dy * m_state->orbitSensitivity;
                if (m_state->cameraPitch > 90.0f) m_state->cameraPitch = 90.0f;
                if (m_state->cameraPitch < -90.0f) m_state->cameraPitch = -90.0f;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
            else if (m_state->isPanning) {
                float yawRad = m_state->cameraYaw * (3.14159f / 180.0f);
                float rightX = cos(yawRad);
                float rightZ = sin(yawRad);
                
                m_state->cameraTargetX -= rightX * dx * m_state->panSensitivity;
                m_state->cameraTargetZ -= rightZ * dx * m_state->panSensitivity;
                m_state->cameraTargetY += dy * m_state->panSensitivity;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
            m_state->lastMouseX = x; m_state->lastMouseY = y;
            return 0;
        }
            
        case WM_MOUSEWHEEL: {
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            m_state->cameraZoom += (zDelta / 120.0f) * m_state->zoomSensitivity;
            if (m_state->cameraZoom > -1.0f) m_state->cameraZoom = -1.0f;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
