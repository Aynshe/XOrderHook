#include "InjectorOverlay.h"
#include "..\XOrderIPC.h" // Include the shared IPC header
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <chrono>

InjectorOverlay* InjectorOverlay::instance = nullptr;

InjectorOverlay::InjectorOverlay() : overlayWindow(nullptr), initialized(false), updateThread(nullptr), shouldStop(false) {
    InitializeCriticalSection(&messageCS);
}

InjectorOverlay::~InjectorOverlay() {
    Shutdown();
    DeleteCriticalSection(&messageCS);
}

InjectorOverlay* InjectorOverlay::GetInstance() {
    if (!instance) {
        instance = new InjectorOverlay();
    }
    return instance;
}

bool InjectorOverlay::Initialize() {
    if (initialized) return true;
    
    std::wcout << L"[InjectorOverlay] Initialisation de l'overlay de l'injecteur..." << std::endl;
    
    // Enregistrer la classe de fenêtre / Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // Pas de background / No background
    wc.lpszClassName = L"XOrderInjectorOverlay";
    
    if (!RegisterClassExW(&wc)) {
        std::wcout << L"[InjectorOverlay] Échec de l'enregistrement de la classe de fenêtre" << std::endl;
        return false;
    }
    
    // Obtenir les dimensions de l'écran / Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Créer la fenêtre overlay (transparente, toujours au-dessus, transparente aux clics) / Create overlay window (transparent, always on top, click-through)
    overlayWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        L"XOrderInjectorOverlay",
        L"XOrder Injector Overlay",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), this
    );
    
    if (!overlayWindow) {
        std::wcout << L"[InjectorOverlay] Échec de la création de la fenêtre overlay" << std::endl;
        return false;
    }
    
    std::wcout << L"[InjectorOverlay] Fenêtre overlay créée (" << screenWidth << L"x" << screenHeight << L")" << std::endl;
    
    // Rendre la fenêtre transparente (fond noir transparent) / Make window transparent (transparent black background)
    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);
    
    std::wcout << L"[InjectorOverlay] Fenêtre overlay créée avec succès" << std::endl;
    
    // Créer le thread de mise à jour / Create update thread
    shouldStop = false;
    updateThread = CreateThread(nullptr, 0, UpdateThreadProc, this, 0, nullptr);
    if (!updateThread) {
        std::wcout << L"[InjectorOverlay] Échec de la création du thread de mise à jour" << std::endl;
        DestroyWindow(overlayWindow);
        overlayWindow = nullptr;
        return false;
    }
    
    initialized = true;
    std::wcout << L"[InjectorOverlay] Overlay de l'injecteur initialisé avec succès" << std::endl;
    return true;
}

void InjectorOverlay::Shutdown() {
    if (!initialized) return;
    
    std::wcout << L"[InjectorOverlay] Fermeture de l'overlay de l'injecteur..." << std::endl;
    
    // Arrêter le thread de mise à jour / Stop update thread
    shouldStop = true;
    if (updateThread) {
        WaitForSingleObject(updateThread, 5000); // Attendre max 5 secondes / Wait max 5 seconds
        CloseHandle(updateThread);
        updateThread = nullptr;
    }
    
    // Détruire la fenêtre / Destroy window
    if (overlayWindow) {
        DestroyWindow(overlayWindow);
        overlayWindow = nullptr;
    }
    
    // Désenregistrer la classe / Unregister class
    UnregisterClassW(L"XOrderInjectorOverlay", GetModuleHandle(nullptr));
    
    initialized = false;
    std::wcout << L"[InjectorOverlay] Overlay de l'injecteur fermé" << std::endl;
}

void InjectorOverlay::HideOverlay() {
    if (overlayWindow) {
        ShowWindow(overlayWindow, SW_HIDE);
        std::wcout << L"[InjectorOverlay] Overlay cache manuellement" << std::endl;
    }
}

void InjectorOverlay::ForceHideOverlay() {
    if (overlayWindow) {
        // Vider tous les messages / Clear all messages
        EnterCriticalSection(&messageCS);
        messages.clear();
        LeaveCriticalSection(&messageCS);
        
        // Cacher immédiatement la fenêtre / Hide window immediately
        ShowWindow(overlayWindow, SW_HIDE);
        std::wcout << L"[InjectorOverlay] Overlay ferme immediatement (rechargement config)" << std::endl;
    }
}

void InjectorOverlay::ShowMessage(const std::wstring& message, DWORD duration, bool isSuccess) {
    EnterCriticalSection(&messageCS);
    
    // Supprimer les anciens messages expirés / Remove old expired messages
    auto it = messages.begin();
    while (it != messages.end()) {
        if (it->IsExpired()) {
            std::wcout << L"[InjectorOverlay] Suppression message expire: " << it->text << std::endl;
            it = messages.erase(it);
        } else {
            ++it;
        }
    }
    
    // Ajouter le nouveau message / Add new message
    messages.emplace_back(message, duration, isSuccess);
    
    LeaveCriticalSection(&messageCS);
    
    // Afficher la fenêtre et forcer un redraw / Show window and force redraw
    if (overlayWindow) {
        std::wcout << L"[InjectorOverlay] Affichage de la fenetre overlay..." << std::endl;
        std::wcout << L"[InjectorOverlay] Message: " << message << L" (duree: " << duration << L"ms, succes: " << (isSuccess ? L"oui" : L"non") << L")" << std::endl;
        
        // S'assurer que la fenêtre est au premier plan / Ensure window is on top
        SetWindowPos(overlayWindow, HWND_TOPMOST, 0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        
        ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);
        UpdateWindow(overlayWindow);
        InvalidateRect(overlayWindow, nullptr, TRUE);
        
        // Forcer un redraw immédiat / Force immediate redraw
        RedrawWindow(overlayWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        
        std::wcout << L"[InjectorOverlay] Fenetre overlay affichee" << std::endl;
    } else {
        std::wcout << L"[InjectorOverlay] ERREUR: overlayWindow est null!" << std::endl;
    }
    
    std::wcout << L"[InjectorOverlay] Message ajouté: " << message << std::endl;
}

void InjectorOverlay::Update() {
    if (!overlayWindow || shouldStop) return;
    
    bool hadMessages = false;
    bool hasMessages = false;
    
    // Utiliser un timeout pour éviter les deadlocks / Use timeout to avoid deadlocks
    if (TryEnterCriticalSection(&messageCS)) {
        // Supprimer les messages expirés / Remove expired messages
        hadMessages = !messages.empty();
        
        // Forcer la vérification d'expiration pour tous les messages / Force expiration check for all messages
        auto it = messages.begin();
        while (it != messages.end()) {
            if (it->IsExpired()) {
                std::wcout << L"[InjectorOverlay] Message expire supprime: " << it->text << std::endl;
                it = messages.erase(it);
            } else {
                ++it;
            }
        }
        
        hasMessages = !messages.empty();
        
        LeaveCriticalSection(&messageCS);
        
        // Si plus de messages, cacher la fenêtre / If no more messages, hide window
        if (hadMessages && !hasMessages) {
            ShowWindow(overlayWindow, SW_HIDE);
            std::wcout << L"[InjectorOverlay] Fenetre cachee - tous messages expires" << std::endl;
        }
        
        // Redessiner si nécessaire / Redraw if necessary
        if (hasMessages) {
            InvalidateRect(overlayWindow, nullptr, FALSE);
        }
    }
}

void InjectorOverlay::Render(HDC hdc) {
    if (!TryEnterCriticalSection(&messageCS)) {
        // Si on ne peut pas obtenir le verrou, ne pas bloquer / If can't get lock, don't block
        return;
    }
    
    if (messages.empty()) {
        LeaveCriticalSection(&messageCS);
        return;
    }
    
    // Obtenir les dimensions de l'écran / Get screen dimensions
    RECT screenRect;
    GetClientRect(overlayWindow, &screenRect);
    
    // Remplir le fond avec du noir transparent / Fill background with transparent black
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &screenRect, blackBrush);
    DeleteObject(blackBrush);
    
    // Configurer le rendu du texte / Configure text rendering
    SetBkMode(hdc, TRANSPARENT);
    
    // Créer une police moderne et lisible / Create modern and readable font
    HFONT hFont = CreateFontW(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    
    // Calculer la taille totale nécessaire pour tous les messages / Calculate total size needed for all messages
    int totalHeight = 0;
    int maxWidth = 0;
    std::vector<SIZE> textSizes;
    
    for (const InjectorOverlayMessage& msg : messages) {
        if (msg.GetAlpha() <= 0.0f) continue;
        
        SIZE textSize = {0};
        const wchar_t* text = msg.text.c_str();
        int textLen = (int)msg.text.length();
        GetTextExtentPoint32W(hdc, text, textLen, &textSize);
        
        textSizes.push_back(textSize);
        totalHeight += textSize.cy + 20; // 20px d'espacement / 20px spacing
        if (textSize.cx > maxWidth) {
            maxWidth = textSize.cx;
        }
    }
    
    if (totalHeight > 0) {
        totalHeight -= 20; // Enlever le dernier espacement / Remove last spacing
        
        // Ajouter du padding / Add padding
        int padding = 20;
        int rectWidth = maxWidth + (padding * 2);
        int rectHeight = totalHeight + (padding * 2);
        
        // Centrer le rectangle / Center rectangle
        int rectX = (screenRect.right - rectWidth) / 2;
        int rectY = (screenRect.bottom - rectHeight) / 2;
        
        // Dessiner le rectangle noir semi-transparent / Draw semi-transparent black rectangle
        RECT bgRect = {rectX, rectY, rectX + rectWidth, rectY + rectHeight};
        
        // Créer un pinceau noir semi-transparent (simulation) / Create semi-transparent black brush (simulation)
        HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 20)); // Gris très foncé au lieu de noir pur / Very dark gray instead of pure black
        FillRect(hdc, &bgRect, bgBrush);
        DeleteObject(bgBrush);
        
        // Dessiner une bordure subtile / Draw subtle border
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rectX, rectY, rectX + rectWidth, rectY + rectHeight);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(borderPen);
        
        // Rendre chaque message / Render each message
        int yOffset = rectY + padding;
        int messageIndex = 0;
        
        for (const InjectorOverlayMessage& msg : messages) {
            float alpha = msg.GetAlpha();
            if (alpha <= 0.0f) continue;
            
            if (messageIndex >= textSizes.size()) break;
            
            SIZE textSize = textSizes[messageIndex];
            
            // Couleur grise pour le texte / Gray color for text
            COLORREF textColor = RGB(200, 200, 200); // Gris clair / Light gray
            
            // Appliquer l'alpha / Apply alpha
            if (alpha < 1.0f) {
                BYTE gray = (BYTE)(200 * alpha);
                textColor = RGB(gray, gray, gray);
            }
            
            // Centrer le texte dans le rectangle / Center text in rectangle
            int x = rectX + (rectWidth - textSize.cx) / 2;
            int y = yOffset;
            
            // Dessiner le texte / Draw text
            SetTextColor(hdc, textColor);
            const wchar_t* text = msg.text.c_str();
            int textLen = (int)msg.text.length();
            TextOutW(hdc, x, y, text, textLen);
            
            yOffset += textSize.cy + 20;
            messageIndex++;
        }
    }
    
    // Nettoyer / Clean up
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    
    LeaveCriticalSection(&messageCS);
}

LRESULT CALLBACK InjectorOverlay::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    InjectorOverlay* overlay = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        overlay = (InjectorOverlay*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)overlay);
    } else {
        overlay = (InjectorOverlay*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    switch (uMsg) {
        case WM_PAINT: {
            if (overlay) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                overlay->Render(hdc);
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        
        case WM_ERASEBKGND:
            // Ne pas effacer le fond, on le gère dans Render() / Don't erase background, we handle it in Render()
            return 1;
            
        case WM_SHOWWINDOW:
            if (wParam && overlay) {
                // Forcer un redraw quand la fenêtre devient visible / Force redraw when window becomes visible
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
            
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Ignorer les clics pour éviter les blocages / Ignore clicks to avoid blocking
            return 0;
            
        case WM_NCHITTEST:
            // Rendre la fenêtre "transparente" aux clics / Make window "transparent" to clicks
            return HTTRANSPARENT;
            
        case WM_CLOSE:
            // Empêcher la fermeture manuelle / Prevent manual closing
            return 0;
            
        case WM_DESTROY:
            return 0;

        case WM_COPYDATA: {
            COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
            if (pcds && pcds->dwData == XORDER_IPC_MESSAGE_ID) {
                XOrderOverlayMsgData* msgData = (XOrderOverlayMsgData*)pcds->lpData;
                if (instance) {
                    instance->ShowMessage(msgData->message, msgData->duration, msgData->isSuccess);
                }
                return TRUE; // Indicate that the message was processed
            }
            break;
        }
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI InjectorOverlay::UpdateThreadProc(LPVOID lpParam) {
    InjectorOverlay* overlay = (InjectorOverlay*)lpParam;
    
    while (!overlay->shouldStop) {
        // Traiter les messages Windows pour éviter les blocages / Process Windows messages to avoid blocking
        MSG msg;
        while (PeekMessage(&msg, overlay->overlayWindow, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Mettre à jour l'overlay / Update overlay
        overlay->Update();
        
        // Mécanisme de sécurité: forcer la fermeture après 5 secondes maximum / Safety mechanism: force close after 5 seconds maximum
        if (TryEnterCriticalSection(&overlay->messageCS)) {
            bool hasOldMessages = false;
            auto now = std::chrono::steady_clock::now();
            
            for (const auto& msg : overlay->messages) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - msg.startTime);
                if (elapsed.count() > 5000) { // Plus de 5 secondes / More than 5 seconds
                    hasOldMessages = true;
                    break;
                }
            }
            
            if (hasOldMessages) {
                std::wcout << L"[InjectorOverlay] SECURITE: Suppression forcee des messages anciens" << std::endl;
                overlay->messages.clear();
                ShowWindow(overlay->overlayWindow, SW_HIDE);
            }
            
            LeaveCriticalSection(&overlay->messageCS);
        }
        
        // Attendre un peu avant la prochaine mise à jour / Wait a bit before next update
        Sleep(50); // Mise à jour toutes les 50ms pour plus de fluidité / Update every 50ms for better fluidity
    }
    
    return 0;
}


// Fonctions utilitaires globales / Global utility functions
void InitializeInjectorOverlay() {
    InjectorOverlay::GetInstance()->Initialize();
}

void ShutdownInjectorOverlay() {
    InjectorOverlay* overlay = InjectorOverlay::GetInstance();
    if (overlay) {
        overlay->Shutdown();
        delete overlay;
        InjectorOverlay::instance = nullptr;
    }
}

// Fonction pour détecter la langue du système / Function to detect system language
bool IsSystemLanguageFrench() {
    LANGID langId = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langId);
    return (primaryLang == LANG_FRENCH);
}

void ShowInjectorOverlayMessage(const std::wstring& gameName, bool success) {
    InjectorOverlay* overlay = InjectorOverlay::GetInstance();
    if (overlay) {
        std::wstring message;
        bool isFrench = IsSystemLanguageFrench();
        
        if (success) {
            message = isFrench ? L"[+] JEU AJOUTE: " + gameName : L"[+] GAME ADDED: " + gameName;
        } else {
            message = isFrench ? L"[-] ECHEC: " + gameName : L"[-] FAILED: " + gameName;
        }
        
        // S'assurer que l'overlay est initialisé / Ensure overlay is initialized
        if (!overlay->IsInitialized()) {
            overlay->Initialize();
        }
        
        // CORRECTION: Durée changée de 5000ms à 2000ms (2 secondes) pour TOUS les messages / CORRECTION: Duration changed from 5000ms to 2000ms (2 seconds) for ALL messages
        std::wcout << L"[InjectorOverlay] Affichage message " << (success ? L"SUCCES" : L"ECHEC") 
                   << L" avec duree 2000ms: " << message << std::endl;
        overlay->ShowMessage(message, 2000, success);
        
        std::wcout << L"[InjectorOverlay] Message affiche: " << message << std::endl;
    }
}

void ForceHideInjectorOverlay() {
    InjectorOverlay* overlay = InjectorOverlay::GetInstance();
    if (overlay && overlay->IsInitialized()) {
        overlay->ForceHideOverlay();
    }
}