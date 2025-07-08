#include "SimpleOverlay.h"
#include "..\XOrderUtils.h"
#include "..\XOrderIPC.h"
#include "HookGlobals.h" // Include the shared IPC header
#include <algorithm>
#include <stdio.h> // For swprintf_s

// External declarations / Déclarations externes
extern void WriteToLog(const std::wstring& message);

extern bool IsSimpleOverlayGloballyEnabled(); // Fonction pour vérifier l'état global / Function to check global state

SimpleOverlay* SimpleOverlay::instance = nullptr;

SimpleOverlay::SimpleOverlay() : overlayWindow(nullptr), hideTime(0), showTime(0), isVisible(false), updateThread(nullptr), shouldStop(false), 
                                 lastForceHideCheck(0), consecutiveFailedHides(0), emergencyTransparentMode(false), isInCleanup(false) {
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
    // The local overlay is now disabled. We just return true.
    // La superposition locale est maintenant désactivée. On retourne simplement true.
    return true;
}

void SimpleOverlay::ShowMessage(const std::wstring& message, DWORD duration) {
    // Create a data structure to pass to the new thread.
    // This needs to be dynamically allocated so it survives after this function returns.
    AsyncShowMessageData* data = new AsyncShowMessageData();
    data->message = message;
    data->duration = duration;

    // Create a new thread to send the message. This makes the call fully asynchronous.
    HANDLE hThread = CreateThread(nullptr, 0, AsyncShowMessageThreadProc, data, 0, nullptr);
    if (hThread) {
        // We don't need to wait for the thread, so we can close the handle immediately.
        // The thread will continue to run until it finishes.
        CloseHandle(hThread);
    } else {
        // If thread creation fails, we must delete the data to prevent a memory leak.
        delete data;
        WriteToLog(L"[SimpleOverlay] IPC Error: Failed to create async message thread.");
    }
}

// This function runs in a separate thread to send the IPC message.
// Cette fonction s'exécute dans un thread séparé pour envoyer le message IPC.
DWORD WINAPI SimpleOverlay::AsyncShowMessageThreadProc(LPVOID lpParam) {
    // Cast the parameter back to our data structure.
    AsyncShowMessageData* data = static_cast<AsyncShowMessageData*>(lpParam);
    if (!data) {
        return 1; // Should not happen.
    }

    // The local overlay logic is deprecated. The new design uses the injector's overlay.
    // This code is commented out to prevent build errors with the new XOrderIPC.h header.
    /*
    HWND injectorHwnd = nullptr;
    wchar_t debugMsg[512];

    // Retry finding the window a few times to handle timing issues.
    for (int i = 0; i < 5; ++i) {
        injectorHwnd = FindWindowW(L"XOrderInjectorOverlay", nullptr);
        if (injectorHwnd) {
            break;
        }
        Sleep(100);
    }

    if (injectorHwnd) {
        // This part is broken due to XOrderIPC.h changes
        // XOrderOverlayMsgData msgData;
        // wcsncpy_s(msgData.message, MAX_OVERLAY_MSG_LENGTH, data->message.c_str(), _TRUNCATE);
        // msgData.duration = data->duration;
        // msgData.isSuccess = true;

        // COPYDATASTRUCT cds;
        // cds.dwData = XORDER_IPC_MESSAGE_ID;
        // cds.cbData = sizeof(XOrderOverlayMsgData);
        // cds.lpData = &msgData;

        // LRESULT result = SendMessageTimeout(injectorHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds, SMTO_NORMAL, 1000, nullptr);
    }
    */

    // IMPORTANT: Clean up the dynamically allocated data.
    // IMPORTANT : Nettoyer les données allouées dynamiquement.
    delete data;
    return 0;
}

void SimpleOverlay::Update() {
    // The local overlay is disabled, so this update logic is no longer needed.
    // La superposition locale est désactivée, cette logique de mise à jour n'est plus nécessaire.
}

void SimpleOverlay::ForceHide() {
    if (overlayWindow) {
        isVisible = false;
        currentMessage.clear();
        consecutiveFailedHides = 0;
        emergencyTransparentMode = false;
        ShowWindow(overlayWindow, SW_HIDE);
        bool isFrench = IsSystemLanguageFrench();
        std::wstring forceHideMessage = isFrench ? L"[SimpleOverlay] Overlay forcé à se cacher" : L"[SimpleOverlay] Overlay forced to hide";
        WriteToLog(forceHideMessage);
    }
}

void SimpleOverlay::EmergencyHide() {
    if (!overlayWindow) return;
    
    bool isFrench = IsSystemLanguageFrench();
    std::wstring emergencyMessage = isFrench ? 
        L"[SimpleOverlay] MODE URGENCE: Fermeture forcée" :
        L"[SimpleOverlay] EMERGENCY MODE: Forced closure";
    WriteToLog(emergencyMessage);
    
    isVisible = false;
    currentMessage.clear();
    emergencyTransparentMode = true;
    
    // Méthode 1: Cacher normalement / Method 1: Hide normally
    ShowWindow(overlayWindow, SW_HIDE);
    
    // Méthode 2: Déplacer hors écran / Method 2: Move off-screen
    SetWindowPos(overlayWindow, HWND_BOTTOM, -10000, -10000, 1, 1, SWP_NOACTIVATE);
    
    // Méthode 3: Rendre complètement transparent / Method 3: Make completely transparent
    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 0, LWA_ALPHA);
    
    // Méthode 4: Minimiser puis cacher / Method 4: Minimize then hide
    ShowWindow(overlayWindow, SW_MINIMIZE);
    ShowWindow(overlayWindow, SW_HIDE);
    
    // Méthode 5: Redimensionner à 0x0 / Method 5: Resize to 0x0
    SetWindowPos(overlayWindow, HWND_BOTTOM, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOACTIVATE);
    
    // Programmer un overlay transparent dans 2 secondes comme solution de contournement / Schedule transparent overlay in 2 seconds as workaround
    CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
        Sleep(2000);
        SimpleOverlay* overlay = (SimpleOverlay*)param;
        if (overlay && overlay->emergencyTransparentMode) {
            bool isFrench = IsSystemLanguageFrench();
            std::wstring transparentMessage = isFrench ? 
                L"[SimpleOverlay] Application d'overlay transparent de sécurité" :
                L"[SimpleOverlay] Applying safety transparent overlay";
            WriteToLog(transparentMessage);
            
            // Afficher un overlay complètement transparent pour "nettoyer" l'affichage / Show completely transparent overlay to "clean" display
            overlay->ShowMessage(L"", 100); // Message vide, durée très courte / Empty message, very short duration
            Sleep(200);
            overlay->ForceHide();
        }
        return 0;
    }, this, 0, nullptr);
    
    consecutiveFailedHides = 0;
    
    std::wstring completedMessage = isFrench ? 
        L"[SimpleOverlay] Mode urgence terminé" :
        L"[SimpleOverlay] Emergency mode completed";
    WriteToLog(completedMessage);
}

void SimpleOverlay::CleanupOverlay() {
    if (isInCleanup) return;  // Éviter les appels récursifs / Prevent recursive calls
    isInCleanup = true;
    
    if (!overlayWindow) {
        isInCleanup = false;
        return;
    }
    
    bool isFrench = IsSystemLanguageFrench();
    
    // 1. Désactiver temporairement le rendu / Temporarily disable rendering
    SetWindowLongPtrW(overlayWindow, GWL_EXSTYLE, 
                     GetWindowLongPtrW(overlayWindow, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
    
    // 2. Masquer la fenêtre avec plusieurs méthodes / Hide window using multiple methods
    ShowWindow(overlayWindow, SW_HIDE);
    SetWindowPos(overlayWindow, HWND_BOTTOM, 0, 0, 0, 0, 
                SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    
    // 3. Vider le message et forcer un rafraîchissement / Clear message and force refresh
    currentMessage.clear();
    InvalidateRect(overlayWindow, nullptr, TRUE);
    UpdateWindow(overlayWindow);
    
    // 4. Forcer un rafraîchissement du bureau / Force desktop refresh
    RECT rect = {0, 0, 1, 1};
    InvalidateRect(nullptr, &rect, TRUE);
    UpdateWindow(GetDesktopWindow());
    
    // 5. Réinitialiser les états / Reset states
    isVisible = false;
    emergencyTransparentMode = false;
    consecutiveFailedHides = 0;
    
    isInCleanup = false;
    
    std::wstring logMsg = isFrench ? 
        L"[SimpleOverlay] Nettoyage de l'overlay effectué" :
        L"[SimpleOverlay] Overlay cleanup completed";
    WriteToLog(logMsg);
}

void SimpleOverlay::Shutdown() {
    // The local overlay is disabled, so there's nothing to shut down.
    // La superposition locale est désactivée, il n'y a rien à fermer.
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
            // Afficher seulement si g_SimpleOverlayEnabled est true OU si c'est un message forcé / Display only if g_SimpleOverlayEnabled is true OR if it's a forced message
            bool shouldDisplay = overlay && overlay->isVisible && !overlay->currentMessage.empty() && !overlay->emergencyTransparentMode;
            
            // Permettre l'affichage des messages de confirmation START+GAUCHE même si désactivé / Allow START+LEFT confirmation messages even if disabled
            bool isConfirmationMessage = (overlay->currentMessage == L"OVERLAY ACTIVÉ" || 
                                        overlay->currentMessage == L"OVERLAY ENABLED" ||
                                        overlay->currentMessage == L"OVERLAY DÉSACTIVÉ" || 
                                        overlay->currentMessage == L"OVERLAY DISABLED");
            
            if (shouldDisplay && (IsSimpleOverlayGloballyEnabled() || isConfirmationMessage)) {
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
            // Nettoyer les ressources du timer
            KillTimer(hwnd, 1);
            return 0;
            
        case WM_TIMER:
            if (wParam == 1) {  // Notre timer de fermeture
                KillTimer(hwnd, 1);  // Arrêter le timer
                if (overlay) {
                    overlay->CleanupOverlay();
                }
                return 0;
            }
            break;
            
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

void ShowSimpleOverlayMessageForced(const std::wstring& message, DWORD duration) {
    // Cette fonction force l'affichage même si g_SimpleOverlayEnabled est false / This function forces display even if g_SimpleOverlayEnabled is false
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

void EmergencyHideSimpleOverlay() {
    SimpleOverlay* overlay = SimpleOverlay::GetInstance();
    if (overlay) {
        overlay->EmergencyHide();
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