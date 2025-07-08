#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <chrono>

// Structure pour les messages d'overlay de l'injecteur
struct InjectorOverlayMessage {
    std::wstring text;
    std::chrono::steady_clock::time_point startTime;
    DWORD duration; // en millisecondes
    bool isSuccess; // true = vert, false = rouge
    
    InjectorOverlayMessage(const std::wstring& msg, DWORD dur, bool success) 
        : text(msg), duration(dur), isSuccess(success) {
        startTime = std::chrono::steady_clock::now();
    }
    
    bool IsExpired() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        bool expired = elapsed.count() >= static_cast<long long>(duration);
        return expired;
    }
    
    float GetAlpha() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        float progress = (float)elapsed.count() / duration;
        
        // Fade out dans les dernières 500ms
        if (progress > 0.8f) {
            return 1.0f - ((progress - 0.8f) / 0.2f);
        }
        return 1.0f;
    }
};

// Classe pour gérer l'overlay de l'injecteur (fenêtre transparente)
class InjectorOverlay {
private:
    std::vector<InjectorOverlayMessage> messages;
    HWND overlayWindow;
    bool initialized;
    HANDLE updateThread;
    bool shouldStop;
    CRITICAL_SECTION messageCS;
    
    InjectorOverlay();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI UpdateThreadProc(LPVOID lpParam);
    
public:
    static InjectorOverlay* instance;
    static InjectorOverlay* GetInstance();
    
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return initialized; }
    void HideOverlay();
    void ForceHideOverlay(); // Fermeture immédiate
    
    void ShowMessage(const std::wstring& message, DWORD duration = 5000, bool isSuccess = true);
    void Update();
    void Render(HDC hdc);
    
    ~InjectorOverlay();
};

// Fonctions utilitaires pour l'overlay de l'injecteur
void InitializeInjectorOverlay();
void ShutdownInjectorOverlay();
void ShowInjectorOverlayMessage(const std::wstring& message, bool success);
void ForceHideInjectorOverlay(); // Fermeture immédiate de l'overlay