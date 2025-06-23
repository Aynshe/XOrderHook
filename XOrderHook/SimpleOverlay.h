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
    DWORD showTime;  // Temps où le message a été affiché / Time when message was shown
    bool isVisible;
    HANDLE updateThread;
    bool shouldStop;
    
    // Mécanisme de sécurité pour forcer la fermeture / Safety mechanism to force closure
    DWORD lastForceHideCheck;
    int consecutiveFailedHides;
    bool emergencyTransparentMode;
    bool isInCleanup;  // Pour éviter les appels récursifs / To prevent recursive calls
    
    void CleanupOverlay();  // Nouvelle méthode de nettoyage / New cleanup method

    // For asynchronous message sending to prevent deadlocks
    // Pour l'envoi de messages asynchrones afin d'éviter les interblocages
    struct AsyncShowMessageData {
        std::wstring message;
        DWORD duration;
    };
    static DWORD WINAPI AsyncShowMessageThreadProc(LPVOID lpParam);
    
    SimpleOverlay();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI UpdateThreadProc(LPVOID lpParam);
    
public:
    static SimpleOverlay* GetInstance();
    static void ResetInstance();
    bool Initialize();
    void ShowMessage(const std::wstring& message, DWORD duration = 2000);
    void Update();
    void ForceHide();
    void EmergencyHide(); // Nouvelle méthode de fermeture d'urgence / New emergency hide method
    void Shutdown();
    ~SimpleOverlay();
};

// Fonctions utilitaires
void InitializeSimpleOverlay();
void ShowSimpleOverlayMessage(const std::wstring& message, DWORD duration = 2000);
void ShowSimpleOverlayMessageForced(const std::wstring& message, DWORD duration = 2000); // Force l'affichage même si désactivé / Force display even if disabled
void ForceHideSimpleOverlay();
void EmergencyHideSimpleOverlay(); // Nouvelle fonction d'urgence / New emergency function
void ShutdownSimpleOverlay();