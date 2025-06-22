#pragma once
#include <windows.h>
#include <string>

// Overlay simple qui fonctionne
class SimpleOverlay {
private:
    static SimpleOverlay* instance;
    HWND overlayWindow;
    std::wstring currentMessage;
    DWORD hideTime;
    bool isVisible;
    HANDLE updateThread;
    bool shouldStop;
    
    SimpleOverlay();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI UpdateThreadProc(LPVOID lpParam);
    
public:
    static SimpleOverlay* GetInstance();
    static void ResetInstance();
    bool Initialize();
    void ShowMessage(const std::wstring& message, DWORD duration = 2000);
    void Update();
    void Shutdown();
    ~SimpleOverlay();
};

// Fonctions utilitaires
void InitializeSimpleOverlay();
void ShowSimpleOverlayMessage(const std::wstring& message, DWORD duration = 2000);
void ShutdownSimpleOverlay();