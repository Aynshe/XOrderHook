#include "SimpleOverlay.h"
#include <algorithm>

// External declarations / Déclarations externes
extern void WriteToLog(const std::wstring& message);
extern bool IsSystemLanguageFrench();

SimpleOverlay* SimpleOverlay::instance = nullptr;

SimpleOverlay::SimpleOverlay() : overlayWindow(nullptr), hideTime(0), showTime(0), isVisible(false), updateThread(nullptr), shouldStop(false) {
}

SimpleOverlay::~SimpleOverlay() {
    Shutdown();
}

SimpleOverlay* SimpleOverlay::GetInstance() {
    if (!instance) {
        instance = new SimpleOverlay();
    }
    return instance;
}

void SimpleOverlay::ResetInstance() {
    instance = nullptr;
}

bool SimpleOverlay::Initialize() {
    if (overlayWindow) return true; // Déjà initialisé
    
    bool isFrench = IsSystemLanguageFrench();
    std::wstring initMessage = isFrench ? L"[SimpleOverlay] Initialisation..." : L"[SimpleOverlay] Initializing...";
    WriteToLog(initMessage);
    
    // Register window class / Enregistrer la classe de fenêtre
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // No background / Pas de background
    wc.lpszClassName = L"XOrderSimpleOverlay";
    
    if (!RegisterClassExW(&wc)) {
        std::wstring errorMessage = isFrench ? L"[SimpleOverlay] échec de l'enregistrement de la classe" : L"[SimpleOverlay] failed to register window class";
        WriteToLog(errorMessage);
        return false;
    }
    
    // Create overlay window / Créer la fenêtre overlay
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    overlayWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        L"XOrderSimpleOverlay",
        L"XOrder Overlay",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), this
    );
    
    if (!overlayWindow) {
        std::wstring errorMessage = isFrench ? L"[SimpleOverlay] échec de la création de la fenêtre" : L"[SimpleOverlay] failed to create window";
        WriteToLog(errorMessage);
        return false;
    }
    
    // Make window transparent (black background = transparent) / Rendre la fenêtre transparente (fond noir = transparent)
    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);
    
    // Create update thread / Créer le thread de mise à jour
    shouldStop = false;
    updateThread = CreateThread(nullptr, 0, UpdateThreadProc, this, 0, nullptr);
    if (!updateThread) {
        std::wstring errorMessage = isFrench ? L"[SimpleOverlay] échec de la création du thread de mise à jour" : L"[SimpleOverlay] failed to create update thread";
        WriteToLog(errorMessage);
        DestroyWindow(overlayWindow);
        overlayWindow = nullptr;
        return false;
    }
    
    std::wstring successMessage = isFrench ? L"[SimpleOverlay] Initialisé avec succès" : L"[SimpleOverlay] Initialized successfully";
    WriteToLog(successMessage);
    return true;
}

void SimpleOverlay::ShowMessage(const std::wstring& message, DWORD duration) {
    bool isFrench = IsSystemLanguageFrench();
    std::wstring displayMessage = isFrench ? L"[SimpleOverlay] Affichage: " + message : L"[SimpleOverlay] Displaying: " + message;
    WriteToLog(displayMessage);
    
    if (!overlayWindow && !Initialize()) {
        std::wstring errorMessage = isFrench ? L"[SimpleOverlay] Impossible d'initialiser" : L"[SimpleOverlay] Unable to initialize";
        WriteToLog(errorMessage);
        return;
    }
    
    currentMessage = message;
    hideTime = GetTickCount() + duration;
    showTime = GetTickCount();
    isVisible = true;
    
    // Ensure window is on top / S'assurer que la fenêtre est au premier plan
    SetWindowPos(overlayWindow, HWND_TOPMOST, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    // Show and redraw / Afficher et redessiner
    ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);
    UpdateWindow(overlayWindow);
    InvalidateRect(overlayWindow, nullptr, TRUE);
    
    // Force immediate redraw / Forcer un redraw immédiat
    RedrawWindow(overlayWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void SimpleOverlay::Update() {
    if (!isVisible || !overlayWindow) return;
    
    DWORD currentTime = GetTickCount();
    
    // Mécanisme de sécurité: forcer la fermeture après 5 secondes maximum / Safety mechanism: force close after 5 seconds maximum
    if (currentTime - showTime > 5000) {
        isVisible = false;
        ShowWindow(overlayWindow, SW_HIDE);
        bool isFrench = IsSystemLanguageFrench();
        std::wstring forceHideMessage = isFrench ? L"[SimpleOverlay] SECURITE: Message forcé à se cacher après 5 secondes" : L"[SimpleOverlay] SAFETY: Message forced to hide after 5 seconds";
        WriteToLog(forceHideMessage);
        return;
    }
    
    // Check if message should be hidden / Vérifier si il faut cacher le message
    if (currentTime >= hideTime) {
        isVisible = false;
        ShowWindow(overlayWindow, SW_HIDE);
        bool isFrench = IsSystemLanguageFrench();
        std::wstring hideMessage = isFrench ? L"[SimpleOverlay] Message caché automatiquement" : L"[SimpleOverlay] Message hidden automatically";
        WriteToLog(hideMessage);
    }
}

void SimpleOverlay::ForceHide() {
    if (overlayWindow) {
        isVisible = false;
        currentMessage.clear();
        ShowWindow(overlayWindow, SW_HIDE);
        bool isFrench = IsSystemLanguageFrench();
        std::wstring forceHideMessage = isFrench ? L"[SimpleOverlay] Overlay forcé à se cacher" : L"[SimpleOverlay] Overlay forced to hide";
        WriteToLog(forceHideMessage);
    }
}

void SimpleOverlay::Shutdown() {
    // Stop update thread / Arrêter le thread de mise à jour
    shouldStop = true;
    if (updateThread) {
        WaitForSingleObject(updateThread, 5000); // Wait max 5 seconds / Attendre max 5 secondes
        CloseHandle(updateThread);
        updateThread = nullptr;
    }
    
    if (overlayWindow) {
        DestroyWindow(overlayWindow);
        overlayWindow = nullptr;
    }
    UnregisterClassW(L"XOrderSimpleOverlay", GetModuleHandle(nullptr));
    
    bool isFrench = IsSystemLanguageFrench();
    std::wstring shutdownMessage = isFrench ? L"[SimpleOverlay] Fermé" : L"[SimpleOverlay] Closed";
    WriteToLog(shutdownMessage);
}

DWORD WINAPI SimpleOverlay::UpdateThreadProc(LPVOID lpParam) {
    SimpleOverlay* overlay = (SimpleOverlay*)lpParam;
    
    while (!overlay->shouldStop) {
        // Process Windows messages to avoid blocking / Traiter les messages Windows pour éviter les blocages
        MSG msg;
        while (PeekMessage(&msg, overlay->overlayWindow, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Update overlay / Mettre à jour l'overlay
        overlay->Update();
        
        // Wait a bit before next update / Attendre un peu avant la prochaine mise à jour
        Sleep(50); // Update every 50ms / Mise à jour toutes les 50ms
    }
    
    return 0;
}

LRESULT CALLBACK SimpleOverlay::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SimpleOverlay* overlay = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        overlay = (SimpleOverlay*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)overlay);
    } else {
        overlay = (SimpleOverlay*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    switch (uMsg) {
        case WM_PAINT: {
            if (overlay && overlay->isVisible && !overlay->currentMessage.empty()) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                // Get screen dimensions / Obtenir les dimensions de l'écran
                RECT screenRect;
                GetClientRect(hwnd, &screenRect);
                
                // Fill background with transparent black / Remplir le fond avec du noir transparent
                HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &screenRect, blackBrush);
                DeleteObject(blackBrush);
                
                // Configure text rendering / Configurer le rendu du texte
                SetBkMode(hdc, TRANSPARENT);
                
                // Create modern and readable font (like InjectorOverlay) / Créer une police moderne et lisible (comme InjectorOverlay)
                HFONT hFont = CreateFontW(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
                
                HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
                
                // Calculate text size / Calculer la taille du texte
                SIZE textSize;
                GetTextExtentPoint32W(hdc, overlay->currentMessage.c_str(), 
                                    (int)overlay->currentMessage.length(), &textSize);
                
                // Add padding / Ajouter du padding
                int padding = 20;
                int rectWidth = textSize.cx + (padding * 2);
                int rectHeight = textSize.cy + (padding * 2);
                
                // Center rectangle / Centrer le rectangle
                int rectX = (screenRect.right - rectWidth) / 2;
                int rectY = (screenRect.bottom - rectHeight) / 2;
                
                // Dessiner le rectangle noir semi-transparent
                // Draw semi-transparent black rectangle
                RECT bgRect = {rectX, rectY, rectX + rectWidth, rectY + rectHeight};
                
                // Créer un pinceau noir semi-transparent (simulation)
                // Create semi-transparent black brush (simulation)
                HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 20)); // Gris très foncé au lieu de noir pur / Very dark gray instead of pure black
                FillRect(hdc, &bgRect, bgBrush);
                DeleteObject(bgBrush);
                
                // Dessiner une bordure subtile
                // Draw subtle border
                HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
                HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, rectX, rectY, rectX + rectWidth, rectY + rectHeight);
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
                DeleteObject(borderPen);
                
                // Centrer le texte dans le rectangle
                // Center text in rectangle
                int x = rectX + (rectWidth - textSize.cx) / 2;
                int y = rectY + (rectHeight - textSize.cy) / 2;
                
                // Dessiner le texte en gris clair (comme InjectorOverlay)
                // Draw text in light gray (like InjectorOverlay)
                SetTextColor(hdc, RGB(200, 200, 200));
                TextOutW(hdc, x, y, overlay->currentMessage.c_str(), (int)overlay->currentMessage.length());
                
                SelectObject(hdc, oldFont);
                DeleteObject(hFont);
                
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        
        case WM_ERASEBKGND:
            // Ne pas effacer le fond, on le garde dans WM_PAINT
            // Don't erase background, we handle it in WM_PAINT
            return 1;
            
        case WM_SHOWWINDOW:
            if (wParam && overlay) {
                // Forcer un redraw quand la fenêtre devient visible
                // Force redraw when window becomes visible
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
            
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Ignorer les clics pour éviter les blocages
            // Ignore clicks to avoid blocking
            return 0;
            
        case WM_NCHITTEST:
            // Rendre la fenêtre "transparente" aux clics
            // Make window "transparent" to clicks
            return HTTRANSPARENT;
            
        case WM_CLOSE:
            // Empêcher la fermeture manuelle
            // Prevent manual closing
            return 0;
            
        case WM_DESTROY:
            return 0;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Fonctions utilitaires globales
// Global utility functions
void InitializeSimpleOverlay() {
    SimpleOverlay::GetInstance()->Initialize();
}

void ShowSimpleOverlayMessage(const std::wstring& message, DWORD duration) {
    SimpleOverlay* overlay = SimpleOverlay::GetInstance();
    if (overlay) {
        overlay->ShowMessage(message, duration);
    }
}

void ForceHideSimpleOverlay() {
    SimpleOverlay* overlay = SimpleOverlay::GetInstance();
    if (overlay) {
        overlay->ForceHide();
    }
}

void ShutdownSimpleOverlay() {
    SimpleOverlay* overlay = SimpleOverlay::GetInstance();
    if (overlay) {
        overlay->Shutdown();
        delete overlay;
        SimpleOverlay::ResetInstance();
    }
}