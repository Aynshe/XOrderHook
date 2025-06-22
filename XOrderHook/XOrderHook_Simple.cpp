#ifndef XORDERHOOK_SIMPLE_CPP
#define XORDERHOOK_SIMPLE_CPP

#define UNICODE
#define _UNICODE

#define WIN32_LEAN_AND_MEAN
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <Rpc.h>
#pragma comment(lib, "rpcrt4.lib")
#include <iomanip>
#include <regex>
#include <cwctype>  // Pour std::iswspace / For std::iswspace
#include <shlobj.h>
#include <initguid.h>
#include <devpkey.h>   // Pour obtenir les propriétés du périphérique / For getting device properties
#include <SetupAPI.h>  // Pour les fonctions HID / For HID functions
#include <rpc.h>
#include <hidsdi.h>
#pragma comment(lib, "hid.lib")
#include <cfgmgr32.h>
#include <windows.h>
#include <shlwapi.h>
#include <psapi.h>    // Pour GetModuleBaseNameW / For GetModuleBaseNameW
#include <Xinput.h>    // Pour les définitions XInput / For XInput definitions
#include <dinput.h>    // Pour DirectInput / For DirectInput
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <map>         // Pour std::map / For std::map

// Definition of ControllerHID structure / Définition de la structure ControllerHID
struct ControllerHID {
    std::wstring devicePath;  // Device path / Chemin du périphérique
    std::wstring instanceId;  // Device instance ID / ID d'instance du périphérique
};

// Forward declaration of GetXInputDeviceHID function / Déclaration anticipée de la fonction GetXInputDeviceHID
bool GetXInputDeviceHID(DWORD dwUserIndex, ControllerHID& hid);

// Type declarations for original XInput functions / Déclaration des types pour les fonctions XInput originales
typedef DWORD (WINAPI *XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD (WINAPI *XInputSetState_t)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
typedef DWORD (WINAPI *XInputGetCapabilities_t)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);

// Pointers to original XInput functions / Pointeurs vers les fonctions XInput originales
static XInputGetState_t XInputGetStateFunc = nullptr;
static XInputSetState_t XInputSetStateFunc = nullptr;
typedef void (WINAPI *XInputEnable_t)(BOOL enable);
typedef DWORD (WINAPI *XInputGetDSoundAudioDeviceGuids_t)(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid);
typedef DWORD (WINAPI *XInputGetStateEx_t)(DWORD dwUserIndex, XINPUT_STATE* pState);

// HID GUID definitions / Définition des GUIDs pour HID
#ifndef HID_CLASS_GUID
DEFINE_GUID(HID_CLASS_GUID, 0x745a17a0, 0x74d3, 0x11d0, 0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda);
#endif

// GUID for HID interface / GUID pour l'interface HID
#ifndef GUID_DEVINTERFACE_HID
extern "C" {
    DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);
}
#endif

// GUID definition for HID class (required for SetupDiGetClassDevs) / Définition du GUID pour la classe HID (nécessaire pour SetupDiGetClassDevs)
#ifndef GUID_DEVCLASS_HIDCLASS
DEFINE_GUID(GUID_DEVCLASS_HIDCLASS, 0x745a17a0, 0x74d3, 0x11d0, 0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda);
#endif

// GUID definition for Human Interface Devices (HID) class / Définition du GUID pour la classe Human Interface Devices (HID)
#ifndef GUID_DEVCLASS_HUMANINTERFACE
DEFINE_GUID(GUID_DEVCLASS_HUMANINTERFACE, 0x4d36e96f, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);
#endif

// Pointers to original XInput functions / Pointeurs vers les fonctions XInput originales
static XInputGetState_t OriginalXInputGetState = nullptr;
static XInputSetState_t OriginalXInputSetState = nullptr;
static XInputGetCapabilities_t OriginalXInputGetCapabilities = nullptr;
static XInputEnable_t OriginalXInputEnable = nullptr;
static XInputGetDSoundAudioDeviceGuids_t OriginalXInputGetDSoundAudioDeviceGuids = nullptr;
static XInputGetStateEx_t OriginalXInputGetStateEx = nullptr;

// Links to required libraries / Liens avec les bibliothèques nécessaires
#pragma comment(lib, "xinput.lib")  // For XInput functions / Pour les fonctions XInput
#pragma comment(lib, "rpcrt4.lib")  // For UuidFromStringA / Pour UuidFromStringA
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "Cfgmgr32.lib")

#include "MinHook/include/MinHook.h"
#include "SimpleOverlay.h"

// Structure to store controller information / Structure pour stocker les informations d'un contrôleur
struct ControllerInfo {
    GUID guid;
    std::wstring hidPath;
    std::wstring deviceId;  // Format: USB\VID_XXXX&PID_XXXX&IG_XX\XX
    DWORD index;
    
    // Default constructor / Constructeur par défaut
    ControllerInfo() : index(0) {
        ZeroMemory(&guid, sizeof(GUID));
    }
    
    // Check if DeviceID matches expected format / Vérifie si le DeviceID correspond au format attendu
    bool IsValidDeviceId() const {
        if (deviceId.empty()) return false;
        
        // Check format: USB\VID_XXXX&PID_XXXX&IG_XX\XX / Vérifier le format: USB\VID_XXXX&PID_XXXX&IG_XX\XX
        // where XXXX are hexadecimal digits / où XXXX sont des chiffres hexadécimaux
        const std::wstring pattern = L"^\\\\\\.\\HID#VID_([0-9A-F]{4})&PID_([0-9A-F]{4})&IG_([0-9A-F]{2})#";
        std::wregex re(pattern, std::regex_constants::icase);
        return std::regex_search(deviceId, re);
    }
    
    // Extract VID from DeviceID / Extrait le VID du DeviceID
    DWORD GetVid() const {
        if (!IsValidDeviceId()) return 0;
        std::wstring vidStr = deviceId.substr(deviceId.find(L"VID_") + 4, 4);
        return std::wcstol(vidStr.c_str(), nullptr, 16);
    }
    
    // Extract PID from DeviceID / Extrait le PID du DeviceID
    DWORD GetPid() const {
        if (!IsValidDeviceId()) return 0;
        std::wstring pidStr = deviceId.substr(deviceId.find(L"PID_") + 4, 4);
        return std::wcstol(pidStr.c_str(), nullptr, 16);
    }
};

// Comparator to use GUID as key in std::map / Comparateur pour utiliser GUID comme clé dans std::map
struct GuidCompare {
    bool operator()(const GUID& a, const GUID& b) const {
        return memcmp(&a, &b, sizeof(GUID)) < 0;
    }
};

// Configuration vector declarations / Déclaration des vecteurs de configuration
static std::vector<ControllerInfo> g_ControllerOrderByHID;    // By HID / Par HID
static std::vector<ControllerInfo> g_ControllerOrderByGUID;   // By GUID / Par GUID
static std::vector<DWORD> g_ControllerOrderByIndex;           // By index / Par index

// Map to store correspondence between controller GUID and its physical XInput index / Map pour stocker la correspondance entre le GUID d'un contrôleur et son index physique XInput
static std::map<GUID, DWORD, GuidCompare> g_GuidToPhysicalIndexMap;
// Reverse map to find GUID from physical index / Map inversée pour trouver le GUID à partir de l'index physique
static std::map<DWORD, GUID> g_PhysicalIndexToGuidMap;

// Variables for interactive dynamic mapping / Variables pour le mappage dynamique interactif
static std::vector<ControllerInfo> g_DynamicControllerOrderByGUID;
static std::map<DWORD, ULONGLONG> g_StartButtonPressTime; // physicalIndex -> tickCount
static std::map<DWORD, ULONGLONG> g_VibrationStopTime;    // physicalIndex -> time to stop vibration
static std::map<DWORD, int>      g_SecondsHeld;          // physicalIndex -> number of pulses sent for assignment
static std::map<DWORD, ULONGLONG> g_ResetComboPressTime;  // physicalIndex -> time for reset combo
static std::map<DWORD, int>      g_ResetSecondsHeld;     // physicalIndex -> number of pulses for reset

// Variables for automatic game addition / Variables pour l'ajout automatique de jeux
static std::map<DWORD, ULONGLONG> g_AddGameComboPressTime; // physicalIndex -> time for add game combo
static std::map<DWORD, int>      g_AddGameSecondsHeld;    // physicalIndex -> number of pulses for add game

// Variables for confirmation vibration sequence / Variables pour la séquence de vibrations de confirmation
static std::map<DWORD, int>       g_PulsesToSend;         // physicalIndex -> number of remaining pulses to send / physicalIndex -> nombre de pulsations restantes à envoyer
static std::map<DWORD, ULONGLONG> g_NextPulseTime;        // physicalIndex -> time of next pulse / physicalIndex -> heure de la prochaine pulsation

// Variable to ensure initialization happens only once / Variable pour s'assurer que l'initialisation ne se fait qu'une seule fois
static bool g_MappingInitialized = false;

// Forward declaration of mapping initialization function / Déclaration anticipée de la fonction d'initialisation du mapping
void InitializeControllerMapping();

// Forward declarations of new functions / Déclarations anticipées des nouvelles fonctions
std::wstring GetForegroundProcessName();
bool AddGameToConfig(const std::wstring& gameName);

// Pointer to loaded XInput module / Pointeur vers le module XInput chargé
static HMODULE g_XInputDLL = nullptr;
static std::wstring g_XInputVersion;  // Detected XInput version / Version XInput détectée

// Forward declaration of utility functions / Déclaration anticipée des fonctions utilitaires
std::string WStringToString(const std::wstring& wstr);
std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
void WriteToLog(const char* message);
void WriteToLog(const wchar_t* message);
void WriteToLog(const std::string& message);
void WriteToLog(const std::wstring& wmessage);
void LoadConfiguration();
DWORD GetRemappedIndex(DWORD dwUserIndex);
std::wstring GetConfigPath();

// Utility function to clean a device path / Fonction utilitaire pour nettoyer un chemin de périphérique
std::wstring CleanDevicePath(const std::wstring& path) {
    std::wstring result = path;
    // Convert to uppercase / Convertir en majuscules
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    // Replace double backslashes with single ones / Remplacer les doubles backslashes par des simples
    size_t pos = 0;
    while ((pos = result.find(L"\\\\", pos)) != std::wstring::npos) {
        result.replace(pos, 2, L"\\");
        pos += 1;
    }
    return result;
}

// List of XInput versions to try / Liste des versions de XInput à essayer
const wchar_t* XINPUT_DLLS[] = {
    L"xinput1_3.dll",  // DirectX SDK
    L"xinput1_4.dll",  // Windows 8+
    L"xinput9_1_0.dll", // Windows Vista, 7
    L"xinput1_2.dll",  // Windows 8 (Preview)
    L"xinput1_1.dll"   // DirectX SDK
};
const int NUM_XINPUT_DLLS = sizeof(XINPUT_DLLS) / sizeof(XINPUT_DLLS[0]);

// Structure to store controller GUID / Structure pour stocker le GUID d'une manette
struct ControllerGUID {
    GUID guid;
    bool operator==(const ControllerGUID& other) const {
        return memcmp(&guid, &other.guid, sizeof(GUID)) == 0;
    }
};

// Variable for call counting (declared as static to avoid conflicts) / Variable pour le comptage des appels (déclarée comme static pour éviter les conflits)
static int g_CallCount = 0;

// Controls logging level (0 = minimal, 1 = verbose) / Contrôle le niveau de journalisation (0 = minimal, 1 = verbeux)
static bool g_VerboseLogging = false;  // Initialized to false by default / Initialisé à false par défaut

// Variable for system language (true = French/Belgian, false = English) / Variable pour la langue du système (true = français/belge, false = anglais)
static bool g_UseFrenchLanguage = false;

// Function to detect system language / Fonction pour détecter la langue du système
bool IsSystemLanguageFrench() {
    LANGID langId = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langId);
    return (primaryLang == LANG_FRENCH);
}

// Function to detect if system is French or Belgian / Fonction pour détecter si le système est en français ou belge
bool DetectFrenchSystem() {
    LCID lcid = GetUserDefaultLCID();
    LANGID langId = LANGIDFROMLCID(lcid);
    WORD primaryLang = PRIMARYLANGID(langId);
    WORD subLang = SUBLANGID(langId);
    
    // French (France, Belgium, Canada, Switzerland, etc.) / Français (France, Belgique, Canada, Suisse, etc.)
    if (primaryLang == LANG_FRENCH) {
        return true;
    }
    
    // Belgian (Dutch from Belgium can also be considered) / Belge (néerlandais de Belgique peut aussi être considéré)
    if (primaryLang == LANG_DUTCH && subLang == SUBLANG_DUTCH_BELGIAN) {
        return true;
    }
    
    return false;
}

// Function to get localized message / Fonction pour obtenir un message localisé
const char* GetLocalizedMessage(const char* frenchMsg, const char* englishMsg) {
    return g_UseFrenchLanguage ? frenchMsg : englishMsg;
}

// Function to get localized message (wstring version) / Fonction pour obtenir un message localisé (version wstring)
const wchar_t* GetLocalizedMessageW(const wchar_t* frenchMsg, const wchar_t* englishMsg) {
    return g_UseFrenchLanguage ? frenchMsg : englishMsg;
}

// Fonction pour charger la bonne version de XInput / Function to load the correct XInput version
bool LoadXInput() {
    // Si une version spécifique a été détectée, essayer de la charger en premier / If a specific version was detected, try to load it first
    if (!g_XInputVersion.empty()) {
        g_XInputDLL = LoadLibraryW(g_XInputVersion.c_str());
        if (g_XInputDLL != nullptr) {
            // Charger les fonctions XInput / Load XInput functions
            XInputGetStateFunc = (XInputGetState_t)GetProcAddress(g_XInputDLL, "XInputGetState");
            XInputSetStateFunc = (XInputSetState_t)GetProcAddress(g_XInputDLL, "XInputSetState");
            
            if (XInputGetStateFunc && XInputSetStateFunc) {
                WriteToLog(L"[XInput] Chargement de la version détectée: " + g_XInputVersion);
                return true;
            }
            
            // Si on arrive ici, les fonctions n'ont pas été trouvées / If we get here, the functions were not found
            FreeLibrary(g_XInputDLL);
            g_XInputDLL = nullptr;
            WriteToLog(L"[XInput] Echec du chargement de la version détectée, tentative avec d'autres versions...");
        }
    }

    // Essayer de charger les différentes versions de XInput (fallback) / Try to load different XInput versions (fallback)
    for (int i = 0; i < NUM_XINPUT_DLLS; i++) {
    // Ne pas réessayer la version qui a déjà échoué / Don't retry the version that already failed
        if (!g_XInputVersion.empty() && wcscmp(XINPUT_DLLS[i], g_XInputVersion.c_str()) == 0) {
            continue;
        }
        
        g_XInputDLL = LoadLibraryW(XINPUT_DLLS[i]);
        if (g_XInputDLL != nullptr) {
            // Charger les fonctions XInput / Load XInput functions
            XInputGetStateFunc = (XInputGetState_t)GetProcAddress(g_XInputDLL, "XInputGetState");
            XInputSetStateFunc = (XInputSetState_t)GetProcAddress(g_XInputDLL, "XInputSetState");
            
            if (XInputGetStateFunc && XInputSetStateFunc) {
                WriteToLog(L"[XInput] Chargement de la version: " + std::wstring(XINPUT_DLLS[i]));
                return true;
            }
            
            // Si on arrive ici, les fonctions n'ont pas été trouvées / If we get here, the functions were not found
            FreeLibrary(g_XInputDLL);
            g_XInputDLL = nullptr;
        }
    }
    
    WriteToLog(L"[XInput] Echec du chargement de XInput");
    return false;
}

// Fonction utilitaire pour convertir wstring en string UTF-8 / Utility function to convert wstring to UTF-8 string
std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string();
    
    std::string strTo(size_needed, 0);
    int result = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    if (result <= 0) return std::string();
    
    return strTo;
}

// Fonction utilitaire pour écrire dans un fichier de log (version pour std::string) / Utility function to write to log file (std::string version)
void WriteToLog(const std::string& message) {
    // Convertir en std::wstring et appeler la surcharge pour centraliser la logique. / Convert to std::wstring and call overload to centralize logic.
    std::wstring wmessage(message.begin(), message.end());
    WriteToLog(wmessage);
}

// Surcharge pour wstring / Overload for wstring
void WriteToLog(const std::wstring& wmessage) {
    // Utiliser la conversion ANSI pour la sortie fichier (meilleure compatibilité) / Use ANSI conversion for file output (better compatibility)
    std::string message;
    if (!wmessage.empty()) {
        int size_needed = WideCharToMultiByte(CP_ACP, 0, &wmessage[0], (int)wmessage.size(), NULL, 0, NULL, NULL);
        if (size_needed > 0) {
            message.resize(size_needed);
            WideCharToMultiByte(CP_ACP, 0, &wmessage[0], (int)wmessage.size(), &message[0], size_needed, NULL, NULL);
        }
    }
    
    // Obtenir le chemin du module (DLL) / Get the module (DLL) path
    wchar_t dllPath[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, dllPath, MAX_PATH) == 0) {
        OutputDebugStringW(L"[XOrderHook] Impossible d'obtenir le chemin du module\n");
        return;
    }
    
    // Extraire le répertoire / Extract the directory
    std::wstring wlogPath = dllPath;
    size_t lastBackslash = wlogPath.find_last_of(L"\\/");
    if (lastBackslash != std::wstring::npos) {
        wlogPath = wlogPath.substr(0, lastBackslash + 1) + L"XOrderHook_Simple.log";
        
        // écraser le log au premier appel, sinon ajouter / overwrite log on first call, otherwise append
        static bool isFirstWrite = true;
        std::ios_base::openmode mode = std::ios::app;
        if (isFirstWrite) {
            mode = std::ios::trunc;
            isFirstWrite = false;
        }

        // Écrire dans le fichier log avec encodage ANSI / Write to log file with ANSI encoding
        std::ofstream logFile(wlogPath.c_str(), mode);
        if (logFile.is_open()) {
            // Ajouter un horodatage / Add timestamp
            SYSTEMTIME st;
            GetLocalTime(&st);
            char timestamp[64] = {0};
            sprintf_s(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
                     st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            
            // Écrire l'horodatage / Write timestamp
            logFile.write(timestamp, strlen(timestamp));
            
            // Écrire le message en ANSI / Write message in ANSI
            logFile.write(message.c_str(), message.length());
            logFile << std::endl;
        }
    }
    
    // Toujours écrire dans la sortie de débogage / Always write to debug output
    OutputDebugStringW(L"[XOrderHook] ");
    OutputDebugStringW(wmessage.c_str());
    OutputDebugStringW(L"\n");
}

// Fonction pour trouver le chemin du fichier de configuration avec fallback via un fichier temporaire / Function to find configuration file path with fallback via temporary file
std::wstring GetConfigPath() {
    // Essayer d'abord le chemin standard / Try standard path first
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(GetModuleHandle(NULL), modulePath, MAX_PATH);
    std::wstring exePath = modulePath;
    std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));
    std::wstring defaultPath = exeDir + L"\\XOrderConfig.ini";

    // Vérifier si le fichier de configuration existe à l'emplacement par défaut / Check if configuration file exists at default location
    if (PathFileExistsW(defaultPath.c_str())) {
        return defaultPath;
    }

    // Sinon, essayer le fichier temporaire / Otherwise, try temporary file
    std::wstring tmpFilePath = exeDir + L"\\XOrderPath.tmp";
    if (PathFileExistsW(tmpFilePath.c_str())) {
        HANDLE hFile = CreateFileW(tmpFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            char buffer[1024] = {0};
            DWORD bytesRead = 0;
            
            if (ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                
                // Analyser le contenu du fichier / Parse file content
                std::istringstream iss(buffer);
                std::string line;
                
                // Lire la première ligne (chemin de base) / Read first line (base path)
                if (std::getline(iss, line)) {
                    // Supprimer les espaces et retours à la ligne / Remove spaces and line returns
                    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
                    
                    if (!line.empty()) {
                        // Lire la deuxième ligne (version XInput si elle existe) / Read second line (XInput version if it exists)
                        std::string xinputVersion;
                        if (std::getline(iss, xinputVersion)) {
                            // Nettoyer la version XInput / Clean XInput version
                            xinputVersion.erase(std::remove(xinputVersion.begin(), xinputVersion.end(), '\r'), xinputVersion.end());
                            xinputVersion.erase(std::remove(xinputVersion.begin(), xinputVersion.end(), '\n'), xinputVersion.end());
                            
                            if (!xinputVersion.empty()) {
                                g_XInputVersion = StringToWString(xinputVersion);
                                WriteToLog(L"[Config] Version XInput détectée: " + g_XInputVersion);
                            }
                        }
                        
                        // Vérifier si le fichier de configuration existe dans le dossier de base / Check if configuration file exists in base folder
                        std::wstring fallbackPath = StringToWString(line) + L"\\XOrderConfig.ini";
                        if (PathFileExistsW(fallbackPath.c_str())) {
                            WriteToLog(L"[Config] Utilisation du fichier de configuration de fallback: " + fallbackPath);
                            CloseHandle(hFile);
                            return fallbackPath;
                        } else {
                            WriteToLog(L"[Config] Fichier de configuration non trouvé dans: " + fallbackPath);
                        }
                    }
                }
            }
            CloseHandle(hFile);
        } else {
            WriteToLog(L"[Config] Impossible d'ouvrir le fichier temporaire pour lecture");
        }
    } else {
        WriteToLog(L"[Config] Fichier XOrderPath.tmp non trouvé. Le fallback n'est pas possible.");
    }

    WriteToLog(L"Aucun fichier XOrderConfig.ini trouvé.");
    return L""; // Retourner un chemin vide si non trouvé / Return empty path if not found
}

// Fonction pour charger la configuration depuis un fichier INI / Function to load configuration from INI file
void LoadConfiguration() {
    bool tempVerbose = g_VerboseLogging;
    g_VerboseLogging = true; // Forcer les logs pendant le chargement / Force logging during loading

    std::wstring configPath = GetConfigPath();
    if (configPath.empty()) {
        WriteToLog("XOrderConfig.ini non trouvé. Aucune configuration chargée.");
        g_VerboseLogging = false; // Pas de verbose si pas de config / No verbose if no config
        return;
    }

    wchar_t log_buffer[MAX_PATH + 100];
    swprintf_s(log_buffer, L"Chargement de la configuration depuis: %s", configPath.c_str());
    WriteToLog(log_buffer);

    // Réinitialiser les configurations / Reset configurations
    g_ControllerOrderByHID.clear();
    g_ControllerOrderByGUID.clear();
    g_ControllerOrderByIndex.clear();

    // Lire la configuration par HID (le plus fiable) / Read HID configuration (most reliable)
    wchar_t hidOrder[4096] = {0};
    GetPrivateProfileStringW(L"Mapping", L"OrderByHID", L"", hidOrder, _countof(hidOrder), configPath.c_str());

    if (wcslen(hidOrder) > 0) {
        WriteToLog((std::wstring(L"Configuration HID lue: ") + hidOrder).c_str());
        std::wstringstream ss(hidOrder);
        std::wstring hidStr;

        while (std::getline(ss, hidStr, L',')) {
            // Nettoyer les espaces (si nécessaire, à adapter pour wstring) / Clean spaces (if needed, adapt for wstring)
            hidStr.erase(std::remove(hidStr.begin(), hidStr.end(), L' '), hidStr.end());

            if (!hidStr.empty()) {
                std::transform(hidStr.begin(), hidStr.end(), hidStr.begin(), ::toupper);
                ControllerInfo info;
                info.hidPath = hidStr;
                g_ControllerOrderByHID.push_back(info);
            }
        }
    }

    // Lire la configuration par GUID (moins fiable) / Read GUID configuration (less reliable)
    wchar_t guidOrder[4096] = {0};
    GetPrivateProfileStringW(L"Mapping", L"OrderByGUID", L"", guidOrder, _countof(guidOrder), configPath.c_str());

    if (wcslen(guidOrder) > 0) {
        WriteToLog((std::wstring(L"Configuration GUID lue: ") + guidOrder).c_str());
        std::wstringstream ss(guidOrder);
        std::wstring guidStr;

        while (std::getline(ss, guidStr, L',')) {
            guidStr.erase(std::remove(guidStr.begin(), guidStr.end(), L' '), guidStr.end());
            guidStr.erase(std::remove(guidStr.begin(), guidStr.end(), L'{'), guidStr.end());
            guidStr.erase(std::remove(guidStr.begin(), guidStr.end(), L'}'), guidStr.end());

            if (!guidStr.empty()) {
                GUID guid = {0};
                if (UuidFromStringW((RPC_WSTR)guidStr.c_str(), &guid) == RPC_S_OK) {
                    ControllerInfo info;
                    info.guid = guid;
                    g_ControllerOrderByGUID.push_back(info);
                    WriteToLog((std::wstring(L"GUID ajouté: ") + guidStr).c_str());
                } else {
                    WriteToLog((std::wstring(L"Erreur de conversion GUID: ") + guidStr).c_str());
                }
            }
        }
        WriteToLog((std::wstring(L"Nombre de GUID chargés: ") + std::to_wstring(g_ControllerOrderByGUID.size())).c_str());
    }

    // Lire la configuration par index (fallback) / Read index configuration (fallback)
    wchar_t indexOrder[256] = {0};
    GetPrivateProfileStringW(L"Mapping", L"Order", L"", indexOrder, _countof(indexOrder), configPath.c_str());

    if (wcslen(indexOrder) > 0) {
        WriteToLog((std::wstring(L"Configuration index lue: ") + indexOrder).c_str());
        std::wstringstream ss(indexOrder);
        std::wstring indexStr;

        while (std::getline(ss, indexStr, L',')) {
            indexStr.erase(std::remove(indexStr.begin(), indexStr.end(), L' '), indexStr.end());
            if (!indexStr.empty()) {
                try {
                    int index = std::stoi(indexStr);
                    g_ControllerOrderByIndex.push_back(index);
                    WriteToLog((std::wstring(L"Index ajouté: ") + std::to_wstring(index)).c_str());
                } catch (const std::invalid_argument&) {
                    WriteToLog((std::wstring(L"Index invalide ignoré: ") + indexStr).c_str());
                } catch (const std::out_of_range&) {
                    WriteToLog((std::wstring(L"Index hors de portée ignoré: ") + indexStr).c_str());
                }
            }
        }
        WriteToLog((std::wstring(L"Nombre d'index chargés: ") + std::to_wstring(g_ControllerOrderByIndex.size())).c_str());
    }

    // Lire les paramètres de la section [Settings] depuis le fichier de l'injecteur / Read [Settings] section parameters from injector file
    std::wstring injectorConfigPath;
    
    // Utiliser le fichier temporaire pour obtenir le chemin de l'injecteur / Use temporary file to get injector path
    std::wstring gameDir = configPath.substr(0, configPath.find_last_of(L"\\/"));
    std::wstring tmpFilePath = gameDir + L"\\XOrderPath.tmp";
    
    if (PathFileExistsW(tmpFilePath.c_str())) {
        HANDLE hFile = CreateFileW(tmpFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            char buffer[1024] = {0};
            DWORD bytesRead = 0;
            
            if (ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::istringstream iss(buffer);
                std::string line;
                
                // Lire la première ligne (chemin de l'injecteur) / Read first line (injector path)
                if (std::getline(iss, line)) {
                    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
                    
                    if (!line.empty()) {
                        injectorConfigPath = StringToWString(line) + L"\\XOrderConfig.ini";
                        WriteToLog(L"[Settings] Chemin injecteur depuis tmp: " + StringToWString(line));
                    }
                }
            }
            CloseHandle(hFile);
        }
    }
    
    // Si on n'a pas trouvé le chemin de l'injecteur, utiliser le fichier local / If injector path not found, use local file
    if (injectorConfigPath.empty()) {
        injectorConfigPath = configPath;
        WriteToLog(L"[Settings] Fichier tmp non trouvé, utilisation du fichier local");
    }
    
    WriteToLog(L"[Settings] Lecture depuis: " + injectorConfigPath);
    
    // Lire VerboseLogging depuis le fichier de l'injecteur / Read VerboseLogging from injector file
    int verboseLogging = GetPrivateProfileIntW(L"Settings", L"VerboseLogging", 0, injectorConfigPath.c_str());
    
    // Debug
    wchar_t verboseStr[256] = {0};
    GetPrivateProfileStringW(L"Settings", L"VerboseLogging", L"0", verboseStr, _countof(verboseStr), injectorConfigPath.c_str());
    
    WriteToLog(L"[Settings] Debug - Chaîne lue: '" + std::wstring(verboseStr) + L"'");
    WriteToLog(L"[Settings] Debug - Entier lu: " + std::to_wstring(verboseLogging));
    
    g_VerboseLogging = (verboseLogging != 0);
    const wchar_t* statusMsg = g_VerboseLogging ? 
        GetLocalizedMessageW(L"Activé", L"Enabled") : 
        GetLocalizedMessageW(L"Désactivé", L"Disabled");
    WriteToLog(L"[Settings] VerboseLogging final: " + std::wstring(statusMsg));

    // Ne pas appliquer d'ordre par défaut automatiquement / Don't apply default order automatically
    // Cela permet aux autres méthodes de mappage (GUID, dynamique) de fonctionner / This allows other mapping methods (GUID, dynamic) to work
    if (g_ControllerOrderByHID.empty() && g_ControllerOrderByGUID.empty() && g_ControllerOrderByIndex.empty()) {
        WriteToLog(L"Aucune configuration de mappage trouvée - utilisation de l'ordre Windows par défaut");
    }

    // La valeur finale de g_VerboseLogging est maintenant définie par le fichier INI / Final g_VerboseLogging value is now set by INI file
    // (pas besoin de restaurer tempVerbose car on a lu la vraie valeur) / (no need to restore tempVerbose since we read the real value)
}

// Fonction pour obtenir l'index réel de la manette en fonction de la configuration / Function to get real controller index based on configuration
DWORD GetRemappedIndex(DWORD dwUserIndex) {
    // Priorité 1: Mappage dynamique interactif / Priority 1: Interactive dynamic mapping
    if (!g_DynamicControllerOrderByGUID.empty()) {
        if (dwUserIndex < g_DynamicControllerOrderByGUID.size()) {
            GUID targetGuid = g_DynamicControllerOrderByGUID[dwUserIndex].guid;
            auto it = g_GuidToPhysicalIndexMap.find(targetGuid);
            if (it != g_GuidToPhysicalIndexMap.end()) {
                DWORD physicalIndex = it->second;
                if (g_VerboseLogging) {
                    wchar_t guidStr[40];
                    StringFromGUID2(targetGuid, guidStr, 40);
                    WriteToLog(L"[GetRemappedIndex][Dynamic] GUID " + std::wstring(guidStr) + L" trouvé! L'index logique " + std::to_wstring(dwUserIndex) + L" est mappé sur l'index PHYSIQUE " + std::to_wstring(physicalIndex));
                }
                return physicalIndex;
            } else {
                if (g_VerboseLogging) {
                    wchar_t guidStr[40];
                    StringFromGUID2(targetGuid, guidStr, 40);
                    WriteToLog(L"[GetRemappedIndex][Dynamic] Le périphérique avec GUID " + std::wstring(guidStr) + L" demandé pour l'index logique " + std::to_wstring(dwUserIndex) + L" n'est pas connecté.");
                }
                return XUSER_MAX_COUNT; // Non connecté / Not connected
            }
        } else {
            return XUSER_MAX_COUNT; // Index logique hors limites pour le mappage dynamique / Logical index out of bounds for dynamic mapping
        }
    }

    if (dwUserIndex >= XUSER_MAX_COUNT) {
        return dwUserIndex;
    }

    // Priorité 2: Mappage par index simple (Order=) - PRIORITÉ ÉLEVÉE / Priority 2: Simple index mapping (Order=) - HIGH PRIORITY
    if (!g_ControllerOrderByIndex.empty() && dwUserIndex < g_ControllerOrderByIndex.size()) {
        DWORD remappedIndex = g_ControllerOrderByIndex[dwUserIndex];
        if (g_VerboseLogging) {
            WriteToLog(L"[GetRemappedIndex][Order] Remapping par index: " + std::to_wstring(dwUserIndex) + L" -> " + std::to_wstring(remappedIndex));
        }
        return remappedIndex;
    }

    // Priorité 3: Mappage par GUID depuis le fichier INI / Priority 3: GUID mapping from INI file
    if (!g_ControllerOrderByGUID.empty()) {
        if (dwUserIndex < g_ControllerOrderByGUID.size()) {
            GUID targetGuid = g_ControllerOrderByGUID[dwUserIndex].guid;
            
            auto it = g_GuidToPhysicalIndexMap.find(targetGuid);
            if (it != g_GuidToPhysicalIndexMap.end()) {
                DWORD physicalIndex = it->second;
                if (g_VerboseLogging) {
                    wchar_t guidStr[40];
                    StringFromGUID2(targetGuid, guidStr, 40);
                    WriteToLog(L"[GetRemappedIndex][INI] GUID " + std::wstring(guidStr) + L" trouvé! L'index logique " + std::to_wstring(dwUserIndex) + L" est mappé sur l'index PHYSIQUE " + std::to_wstring(physicalIndex));
                }
                return physicalIndex;
            } else {
                if (g_VerboseLogging) {
                    wchar_t guidStr[40];
                    StringFromGUID2(targetGuid, guidStr, 40);
                    WriteToLog(L"[GetRemappedIndex][INI] Le périphérique avec GUID " + std::wstring(guidStr) + L" demandé pour l'index logique " + std::to_wstring(dwUserIndex) + L" n'a pas été trouvé. Le jeu le verra comme déconnecté.");
                }
                return XUSER_MAX_COUNT; // Le périphérique n'est pas connecté / Device is not connected
            }
        }
    }

    // --- FALLBACK sur les autres méthodes --- / --- FALLBACK to other methods ---

    if (!g_ControllerOrderByHID.empty() && dwUserIndex < g_ControllerOrderByHID.size()) {
        // La logique de mappage par HID (plus complexe) pourrait être implémentée ici si nécessaire / HID mapping logic (more complex) could be implemented here if needed
        if (g_VerboseLogging) {
            WriteToLog(L"Remapping par HID non encore implémenté pour l'index " + std::to_wstring(dwUserIndex));
        }
    }

    // Si aucune configuration ne correspond, retourner l'index original / If no configuration matches, return original index
    return dwUserIndex;
}

// Structure pour XInputGetStateEx (non documentée) / Structure for XInputGetStateEx (undocumented)
#ifndef XINPUT_STATE_EX_DEFINED
#define XINPUT_STATE_EX_DEFINED
typedef struct _XINPUT_STATE_EX {
    DWORD          dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE_EX, *PXINPUT_STATE_EX;
#endif // XINPUT_STATE_EX_DEFINED

// Fonctions de hook / Hook functions
DWORD WINAPI XInputGetState_Hook(DWORD dwUserIndex, XINPUT_STATE* pState) {
    ULONGLONG currentTime = GetTickCount64();

    // --- Gérer la séquence de vibrations de confirmation --- / --- Handle confirmation vibration sequence ---
    if (OriginalXInputSetState) {
        for (auto it = g_PulsesToSend.begin(); it != g_PulsesToSend.end(); ) {
            DWORD physicalIdx = it->first;
            if (g_NextPulseTime.count(physicalIdx) && currentTime >= g_NextPulseTime[physicalIdx]) {
                // Envoyer une pulsation intense / Send intense pulse
                XINPUT_VIBRATION vibration = { 50000, 50000 };
                OriginalXInputSetState(physicalIdx, &vibration);
                g_VibrationStopTime[physicalIdx] = currentTime + 150; // Durée de la pulsation / Pulse duration

                it->second--; // Une pulsation de moins à envoyer / One less pulse to send

                if (it->second > 0) {
                    // Programmer la prochaine pulsation après une courte pause / Schedule next pulse after short pause
                    g_NextPulseTime[physicalIdx] = currentTime + 300; // Intervalle de 300ms / 300ms interval
                    ++it;
                } else {
                    // Séquence terminée, on nettoie / Sequence finished, clean up
                    g_NextPulseTime.erase(physicalIdx);
                    it = g_PulsesToSend.erase(it);
                }
            } else {
                ++it;
            }
        }
    }

    // Gérer l'arrêt des vibrations programmées / Handle stopping of scheduled vibrations
    if (OriginalXInputSetState) {
        for (auto it = g_VibrationStopTime.begin(); it != g_VibrationStopTime.end(); ) {
            if (currentTime >= it->second) {
                XINPUT_VIBRATION vibration = {0, 0};
                OriginalXInputSetState(it->first, &vibration);
                it = g_VibrationStopTime.erase(it);
            } else {
                ++it;
            }
        }
    }

    // --- Logique de mappage interactif --- / --- Interactive mapping logic ---
    for (DWORD physicalIdx = 0; physicalIdx < XUSER_MAX_COUNT; ++physicalIdx) {
        XINPUT_STATE physicalState;
        if (OriginalXInputGetState && OriginalXInputGetState(physicalIdx, &physicalState) == ERROR_SUCCESS) {
            bool startPressed = (physicalState.Gamepad.wButtons & XINPUT_GAMEPAD_START);
            bool dpadDownPressed = (physicalState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool dpadUpPressed = (physicalState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);

            // Priorité 1: Combo pour ajouter un jeu (START + HAUT) / Priority 1: Combo to add game (START + UP)
            if (startPressed && dpadUpPressed && !dpadDownPressed) {
                if (g_AddGameComboPressTime.find(physicalIdx) == g_AddGameComboPressTime.end()) {
                    bool isFrench = IsSystemLanguageFrench();
                    std::wstring debugMessage = isFrench ?
                        L"[Debug] Combo ADD GAME détecté sur l'index physique: " + std::to_wstring(physicalIdx) :
                        L"[Debug] ADD GAME combo detected on physical index: " + std::to_wstring(physicalIdx);
                    WriteToLog(debugMessage);
                    g_AddGameComboPressTime[physicalIdx] = currentTime;
                    g_AddGameSecondsHeld[physicalIdx] = 0;
                } else {
                    ULONGLONG timeHeld = currentTime - g_AddGameComboPressTime[physicalIdx];
                    int currentSecond = static_cast<int>(timeHeld / 1000);

                    if (currentSecond >= 3 && g_AddGameSecondsHeld[physicalIdx] < 3) {
                        // Détecter le jeu en focus et l'ajouter / Detect focused game and add it
                        std::wstring gameName = GetForegroundProcessName();
                        if (!gameName.empty()) {
                            WriteToLog(L"[AddGame] Tentative d'ajout du jeu en focus: " + gameName);
                            if (AddGameToConfig(gameName)) {
                                WriteToLog(L"[Action] Jeu '" + gameName + L"' ajouté à la configuration!");
                                
                                // Afficher l'overlay de confirmation / Show confirmation overlay
                                WriteToLog(L"[Overlay] Jeu ajouté: " + gameName);
                                
                                // Vibration de confirmation (3 pulsations courtes) / Confirmation vibration (3 short pulses)
                                if (OriginalXInputSetState) {
                                    XINPUT_VIBRATION vibration = {30000, 30000};
                                    OriginalXInputSetState(physicalIdx, &vibration);
                                    g_VibrationStopTime[physicalIdx] = currentTime + 200;
                                }
                                g_PulsesToSend[physicalIdx] = 3; // 3 pulsations pour confirmer l'ajout / 3 pulses to confirm addition
                                g_NextPulseTime[physicalIdx] = currentTime + 400;
                            } else {
                                WriteToLog(L"[AddGame] Echec de l'ajout du jeu (déjà existant ou erreur)");
                                
                                // Afficher l'overlay d'erreur / Show error overlay
                                WriteToLog(L"[Overlay] Erreur jeu: " + gameName);
                                
                                // Vibration d'erreur (1 longue pulsation) / Error vibration (1 long pulse)
                                if (OriginalXInputSetState) {
                                    XINPUT_VIBRATION vibration = {65535, 65535};
                                    OriginalXInputSetState(physicalIdx, &vibration);
                                    g_VibrationStopTime[physicalIdx] = currentTime + 1000;
                                }
                            }
                        } else {
                            WriteToLog(L"[AddGame] Impossible de détecter le jeu en focus");
                            
                            // Afficher l'overlay d'erreur / Show error overlay
                            WriteToLog(L"[Overlay] Aucun jeu détecté");
                            
                            // Vibration d'erreur / Error vibration
                            if (OriginalXInputSetState) {
                                XINPUT_VIBRATION vibration = {65535, 65535};
                                OriginalXInputSetState(physicalIdx, &vibration);
                                g_VibrationStopTime[physicalIdx] = currentTime + 1000;
                            }
                        }
                        
                        g_AddGameSecondsHeld[physicalIdx] = 3;
                        g_AddGameComboPressTime.erase(physicalIdx);
                    } else if (currentSecond > 0 && currentSecond < 3 && g_AddGameSecondsHeld[physicalIdx] < currentSecond) {
                        // Vibration de progression / Progress vibration
                        if (OriginalXInputSetState) {
                            XINPUT_VIBRATION vibration = {20000, 20000}; // Vibration légère / Light vibration
                            OriginalXInputSetState(physicalIdx, &vibration);
                            g_VibrationStopTime[physicalIdx] = currentTime + 100;
                        }
                        g_AddGameSecondsHeld[physicalIdx] = currentSecond;
                    }
                }
            }
            // Priorité 2: Combo de réinitialisation (START + BAS) / Priority 2: Reset combo (START + DOWN)
            else if (startPressed && dpadDownPressed && !dpadUpPressed) {
                if (g_ResetComboPressTime.find(physicalIdx) == g_ResetComboPressTime.end()) {
                    bool isFrench = IsSystemLanguageFrench();
                    std::wstring debugMessage = isFrench ?
                        L"[Debug] Combo RESET détecté sur l'index physique: " + std::to_wstring(physicalIdx) :
                        L"[Debug] RESET combo detected on physical index: " + std::to_wstring(physicalIdx);
                    WriteToLog(debugMessage);
                    g_ResetComboPressTime[physicalIdx] = currentTime;
                    g_ResetSecondsHeld[physicalIdx] = 0;
                } else {
                    ULONGLONG timeHeld = currentTime - g_ResetComboPressTime[physicalIdx];
                    int currentSecond = static_cast<int>(timeHeld / 1000);

                    if (currentSecond >= 3 && g_ResetSecondsHeld[physicalIdx] < 3) {
                        bool isFrench = IsSystemLanguageFrench();
                        std::wstring actionMessage = isFrench ?
                            L"[Action] Ordre des manettes réinitialisé." :
                            L"[Action] Controller order reset.";
                        WriteToLog(actionMessage);
                        
                        // Afficher l'overlay de réinitialisation / Show reset overlay
                        std::wstring overlayResetMessage = isFrench ?
                            L"REINITIALISATION PAR DEFAUT" :
                            L"DEFAULT RESET";
                        ShowSimpleOverlayMessage(overlayResetMessage, 2000);
                        
                        g_DynamicControllerOrderByGUID.clear();
                        g_StartButtonPressTime.clear(); // Nettoyer aussi les états d'assignation / Also clean assignment states
                        g_SecondsHeld.clear();
                        
                        if (OriginalXInputSetState) {
                            XINPUT_VIBRATION vibration = {40000, 40000};
                            OriginalXInputSetState(physicalIdx, &vibration);
                            g_VibrationStopTime[physicalIdx] = currentTime + 500;
                        }
                        g_ResetSecondsHeld[physicalIdx] = 3;
                        g_ResetComboPressTime.erase(physicalIdx);
                    } else if (currentSecond > 0 && currentSecond < 3 && g_ResetSecondsHeld[physicalIdx] < currentSecond) {
                        if (OriginalXInputSetState) {
                            XINPUT_VIBRATION vibration = {32767, 32767}; // 50% de puissance / 50% power
                            OriginalXInputSetState(physicalIdx, &vibration);
                            g_VibrationStopTime[physicalIdx] = currentTime + 100;
                        }
                        g_ResetSecondsHeld[physicalIdx] = currentSecond;
                    }
                }
            }
            // Priorité 3: Mappage dynamique simple (START seul) / Priority 3: Simple dynamic mapping (START only)
            else if (startPressed && !dpadUpPressed && !dpadDownPressed) {
                if (g_StartButtonPressTime.find(physicalIdx) == g_StartButtonPressTime.end()) {
                    bool isFrench = IsSystemLanguageFrench();
                    std::wstring debugMessage = isFrench ?
                        L"[Debug] START press détecté sur l'index physique: " + std::to_wstring(physicalIdx) :
                        L"[Debug] START press detected on physical index: " + std::to_wstring(physicalIdx);
                    WriteToLog(debugMessage);
                    
                    // Afficher l'overlay avec l'index XInput (utiliser l'index physique) / Show overlay with XInput index (use physical index)
                    
                    g_StartButtonPressTime[physicalIdx] = currentTime;
                    g_SecondsHeld[physicalIdx] = 0;
                } else {
                    ULONGLONG timeHeld = currentTime - g_StartButtonPressTime[physicalIdx];
                    int currentSecond = static_cast<int>(timeHeld / 1000);

                    if (currentSecond >= 3 && g_SecondsHeld[physicalIdx] < 3) {
                        if (g_PhysicalIndexToGuidMap.count(physicalIdx)) {
                            GUID controllerGuid = g_PhysicalIndexToGuidMap[physicalIdx];
                            bool alreadyAdded = false;
                            for (const auto& info : g_DynamicControllerOrderByGUID) {
                                if (IsEqualGUID(info.guid, controllerGuid)) { alreadyAdded = true; break; }
                            }
                            if (!alreadyAdded) {
                                ControllerInfo newInfo;
                                newInfo.guid = controllerGuid;
                                newInfo.index = (DWORD)g_DynamicControllerOrderByGUID.size();
                                g_DynamicControllerOrderByGUID.push_back(newInfo);

                                // Afficher l'overlay avec l'index assigné / Show overlay with assigned index
                                bool isFrench = IsSystemLanguageFrench();
                                std::wstring overlayMessage = isFrench ? 
                                    L"Manette positionnée sur (XINPUT " + std::to_wstring(newInfo.index) + L")" :
                                    L"Controller positioned on (XINPUT " + std::to_wstring(newInfo.index) + L")";
                                ShowSimpleOverlayMessage(overlayMessage, 2000);

                                wchar_t guidStr[40];
                                StringFromGUID2(controllerGuid, guidStr, 40);
                                std::wstring logMessage = isFrench ?
                                    L"[Mappage Dynamique] Manette " + std::wstring(guidStr) + L" assignée à l'index " + std::to_wstring(newInfo.index) :
                                    L"[Dynamic Mapping] Controller " + std::wstring(guidStr) + L" assigned to index " + std::to_wstring(newInfo.index);
                                WriteToLog(logMessage);

                                // Lancer la séquence de vibrations de confirmation / Start confirmation vibration sequence
                                g_PulsesToSend[physicalIdx] = newInfo.index + 1;
                                g_NextPulseTime[physicalIdx] = currentTime; // Démarrer immédiatement / Start immediately
                                std::wstring vibrationMessage = isFrench ?
                                    L"[Vibration] Lancement de " + std::to_wstring(newInfo.index + 1) + L" pulsations pour l'index " + std::to_wstring(newInfo.index) :
                                    L"[Vibration] Starting " + std::to_wstring(newInfo.index + 1) + L" pulses for index " + std::to_wstring(newInfo.index);
                                WriteToLog(vibrationMessage);
                            }
                        }
                        g_SecondsHeld[physicalIdx] = 3;
                        g_StartButtonPressTime.erase(physicalIdx);
                    } else if (currentSecond > 0 && currentSecond < 3 && g_SecondsHeld[physicalIdx] < currentSecond) {
                        if (OriginalXInputSetState) {
                            XINPUT_VIBRATION vibration = {32767, 32767}; // 50% de puissance / 50% power
                            OriginalXInputSetState(physicalIdx, &vibration);
                            g_VibrationStopTime[physicalIdx] = currentTime + 100;
                        }
                        g_SecondsHeld[physicalIdx] = currentSecond;
                    }
                }
            } else {
                // Les boutons sont relâchés, on réinitialise tous les timers pour cet index / Buttons are released, reset all timers for this index
                g_StartButtonPressTime.erase(physicalIdx);
                g_SecondsHeld.erase(physicalIdx);
                g_ResetComboPressTime.erase(physicalIdx);
                g_ResetSecondsHeld.erase(physicalIdx);
                g_AddGameComboPressTime.erase(physicalIdx);
                g_AddGameSecondsHeld.erase(physicalIdx);
            }
        }
    }

    // --- Logique de remappage standard --- / --- Standard remapping logic ---
    DWORD remappedIndex = GetRemappedIndex(dwUserIndex);
    
    if (remappedIndex >= XUSER_MAX_COUNT) {
        ZeroMemory(pState, sizeof(XINPUT_STATE));
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    // Appeler la fonction originale avec l'index remappé / Call original function with remapped index
    return OriginalXInputGetState(remappedIndex, pState);
}

DWORD WINAPI XInputSetState_Hook(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration) {
    if (g_VerboseLogging && g_CallCount++ < 10) { // Ne pas inonder les logs / Don't flood logs
        WriteToLog((std::string("XInputSetState_Hook appelé pour la manette ") + std::to_string(dwUserIndex)).c_str());
    }
    
    // Obtenir l'index réel de la manette / Get real controller index
    DWORD realIndex = GetRemappedIndex(dwUserIndex);
    
    if (OriginalXInputSetState) {
        DWORD result = OriginalXInputSetState(realIndex, pVibration);
        
        // Si la manette n'est pas connectée, essayer avec l'index d'origine / If controller not connected, try with original index
        if (result == ERROR_DEVICE_NOT_CONNECTED && realIndex != dwUserIndex) {
            result = OriginalXInputSetState(dwUserIndex, pVibration);
        }
        
        return result;
    }
    return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetCapabilities_Hook(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities) {
    if (g_CallCount++ < 10) { // Ne pas inonder les logs / Don't flood logs
        WriteToLog((std::string("XInputGetCapabilities_Hook appelé pour la manette ") + std::to_string(dwUserIndex)).c_str());
    }
    
    // Obtenir l'index réel de la manette / Get real controller index
    DWORD realIndex = GetRemappedIndex(dwUserIndex);
    
    if (OriginalXInputGetCapabilities) {
        DWORD result = OriginalXInputGetCapabilities(realIndex, dwFlags, pCapabilities);
        
        // Si la manette n'est pas connectée, essayer avec l'index d'origine / If controller not connected, try with original index
        if (result == ERROR_DEVICE_NOT_CONNECTED && realIndex != dwUserIndex) {
            result = OriginalXInputGetCapabilities(dwUserIndex, dwFlags, pCapabilities);
        }
        
        return result;
    }
    return ERROR_DEVICE_NOT_CONNECTED;
}

void WINAPI XInputEnable_Hook(BOOL enable) {
    WriteToLog("XInputEnable_Hook appelé");
    if (OriginalXInputEnable) {
        OriginalXInputEnable(enable);
    }
}

DWORD WINAPI XInputGetDSoundAudioDeviceGuids_Hook(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid) {
    WriteToLog("XInputGetDSoundAudioDeviceGuids_Hook appelé");
    if (OriginalXInputGetDSoundAudioDeviceGuids) {
        return OriginalXInputGetDSoundAudioDeviceGuids(dwUserIndex, pDSoundRenderGuid, pDSoundCaptureGuid);
    }
    return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetStateEx_Hook(DWORD dwUserIndex, XINPUT_STATE* pState) {
    if (g_CallCount++ < 10) { // Ne pas inonder les logs / Don't flood logs
        WriteToLog((std::string("XInputGetStateEx_Hook appelé pour la manette ") + std::to_string(dwUserIndex)).c_str());
    }
    
    // Obtenir l'index réel de la manette / Get real controller index
    DWORD realIndex = GetRemappedIndex(dwUserIndex);
    
    if (OriginalXInputGetStateEx) {
        DWORD result = OriginalXInputGetStateEx(realIndex, pState);
        
        // Si la manette n'est pas connectée, essayer avec l'index d'origine / If controller not connected, try with original index
        if (result == ERROR_DEVICE_NOT_CONNECTED && realIndex != dwUserIndex) {
            result = OriginalXInputGetStateEx(dwUserIndex, pState);
        }
        
        return result;
    } else if (OriginalXInputGetState) {
        // Fallback sur XInputGetState si XInputGetStateEx n'est pas disponible / Fallback to XInputGetState if XInputGetStateEx not available
        return XInputGetState_Hook(dwUserIndex, pState);
    }
    return ERROR_DEVICE_NOT_CONNECTED;
}

// Suppression de la deuxième définition de LoadXInput / Removal of second LoadXInput definition

// Fonction utilitaire pour convertir std::wstring en std::string / Utility function to convert std::wstring to std::string
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Fonction pour récupérer le HID d'une manette XInput / Function to get HID of an XInput controller
bool GetXInputDeviceHID(DWORD dwUserIndex, ControllerHID& hid) {
    if (dwUserIndex >= XUSER_MAX_COUNT) return false;

    GUID hidInterfaceGuid;
    HidD_GetHidGuid(&hidInterfaceGuid);

    // Utilisation des fonctions SetupAPI génériques (mappées sur W par #define UNICODE) / Use generic SetupAPI functions (mapped to W by #define UNICODE)
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&hidInterfaceGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        WriteToLog(L"Echec de SetupDiGetClassDevs.");
        return false;
    }

    DWORD currentDeviceIndex = 0;
    for (DWORD i = 0; ; ++i) {
        SP_DEVICE_INTERFACE_DATA devInterfaceData = {0};
        devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        if (!SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &hidInterfaceGuid, i, &devInterfaceData)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break; // Fin de la liste / End of list
            continue;
        }

        DWORD dwRequiredSize = 0;
        // La première passe obtient la taille requise / First pass gets required size
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, NULL, 0, &dwRequiredSize, NULL);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }

        std::vector<BYTE> detailDataBuffer(dwRequiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)detailDataBuffer.data();
        pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        SP_DEVINFO_DATA devInfoData = { sizeof(SP_DEVINFO_DATA) };

        // La deuxième passe obtient les détails / Second pass gets details
        if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, pDetail, dwRequiredSize, NULL, &devInfoData)) {
            continue;
        }

        // Rendre la recherche de "IG_" insensible à la casse en passant le chemin en majuscules / Make "IG_" search case-insensitive by converting path to uppercase
        std::wstring devicePathUpper = pDetail->DevicePath;
        std::transform(devicePathUpper.begin(), devicePathUpper.end(), devicePathUpper.begin(), ::towupper);

        if (wcsstr(devicePathUpper.c_str(), L"IG_") != nullptr) {
            if (currentDeviceIndex == dwUserIndex) {
                // C'est la manette que nous cherchons / This is the controller we're looking for
                hid.devicePath = pDetail->DevicePath;

                wchar_t instanceId[MAX_DEVICE_ID_LEN];
                if (CM_Get_Device_ID(devInfoData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
                    hid.instanceId = instanceId;
                    WriteToLog(L"[GetHID] Manette trouvée pour l'index " + std::to_wstring(dwUserIndex) + L": " + hid.instanceId);
                    SetupDiDestroyDeviceInfoList(hDevInfo);
                    return true;
                }
            }
            currentDeviceIndex++;
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    WriteToLog(L"[GetHID] Aucune manette trouvée pour l'index " + std::to_wstring(dwUserIndex));
    return false;
}

// Structure pour stocker les infos d'un périphérique DirectInput / Structure to store DirectInput device info
struct DInputDeviceInfo {
    GUID instanceGuid;
    GUID productGuid; // Contient VID/PID / Contains VID/PID
};

// Callback pour l'énumération DirectInput / Callback for DirectInput enumeration
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext) {
    std::vector<DInputDeviceInfo>* devices = static_cast<std::vector<DInputDeviceInfo>*>(pContext);
    DInputDeviceInfo info;
    info.instanceGuid = pdidInstance->guidInstance;
    info.productGuid = pdidInstance->guidProduct;
    devices->push_back(info);

    wchar_t guidStr[40];
    StringFromGUID2(pdidInstance->guidInstance, guidStr, 40);
    WriteToLog(L"[DInput] Périphérique trouvé: " + std::wstring(pdidInstance->tszProductName) + L" - GUID: " + guidStr);

    return DIENUM_CONTINUE;
}

void InitializeControllerMapping() {
    if (g_MappingInitialized) {
        return;
    }

    WriteToLog(L"--- Initialisation du mapping GUID -> Index Physique ---");
    g_GuidToPhysicalIndexMap.clear();

    // 1. énumérer les périphériques avec DirectInput pour obtenir les GUIDs / 1. enumerate devices with DirectInput to get GUIDs
    std::vector<DInputDeviceInfo> dinputDevices;
    LPDIRECTINPUT8 pDI;
    if (SUCCEEDED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&pDI, NULL))) {
        pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, &dinputDevices, DIEDFL_ATTACHEDONLY);
        pDI->Release();
    } else {
        WriteToLog(L"[Erreur] Echec de la création de l'objet DirectInput8.");
        return;
    }

    WriteToLog(L"Nombre de périphériques DirectInput trouvés: " + std::to_wstring(dinputDevices.size()));

    // Trier les périphériques DInput par GUID pour assurer un ordre déterministe / Sort DInput devices by GUID to ensure deterministic order
    std::sort(dinputDevices.begin(), dinputDevices.end(), [](const DInputDeviceInfo& a, const DInputDeviceInfo& b) {
        return memcmp(&a.instanceGuid, &b.instanceGuid, sizeof(GUID)) < 0;
    });

    // Créer un vecteur pour marquer les périphériques DInput déjà mappés / Create vector to mark already mapped DInput devices
    std::vector<bool> dinputDeviceUsed(dinputDevices.size(), false);

    // 2. énumérer les périphériques XInput pour obtenir les index physiques et les DeviceIDs / 2. enumerate XInput devices to get physical indexes and DeviceIDs
    for (DWORD physicalIndex = 0; physicalIndex < XUSER_MAX_COUNT; ++physicalIndex) {
        XINPUT_STATE state;
        if (OriginalXInputGetState && OriginalXInputGetState(physicalIndex, &state) == ERROR_SUCCESS) {
            ControllerHID hid;
            if (GetXInputDeviceHID(physicalIndex, hid) && !hid.instanceId.empty()) {
                // Extraire VID/PID du DeviceID de XInput / Extract VID/PID from XInput DeviceID
                std::wstring xinputDeviceId = hid.instanceId;
                std::transform(xinputDeviceId.begin(), xinputDeviceId.end(), xinputDeviceId.begin(), ::towupper);

                size_t vidPos = xinputDeviceId.find(L"VID_");
                size_t pidPos = xinputDeviceId.find(L"PID_");

                if (vidPos != std::wstring::npos && pidPos != std::wstring::npos) {
                    std::wstring vidStr = xinputDeviceId.substr(vidPos + 4, 4);
                    std::wstring pidStr = xinputDeviceId.substr(pidPos + 4, 4);
                    
                    DWORD xinputVid = std::wcstol(vidStr.c_str(), nullptr, 16);
                    DWORD xinputPid = std::wcstol(pidStr.c_str(), nullptr, 16);

                    // 3. Chercher une correspondance dans les périphériques DirectInput non encore utilisés / 3. Look for match in unused DirectInput devices
                    for (size_t i = 0; i < dinputDevices.size(); ++i) {
                        if (dinputDeviceUsed[i]) {
                            continue; // Ce périphérique est déjà mappé / This device is already mapped
                        }

                        const auto& dinputDevice = dinputDevices[i];
                        
                        // Le VID/PID est dans les 2 premiers DWORDs du productGuid / VID/PID is in the first 2 DWORDs of productGuid
                        DWORD dinputVid = LOWORD(dinputDevice.productGuid.Data1);
                        DWORD dinputPid = HIWORD(dinputDevice.productGuid.Data1);

                        if (dinputVid == xinputVid && dinputPid == xinputPid) {
                            // Correspondance trouvée! / Match found!
                            g_GuidToPhysicalIndexMap[dinputDevice.instanceGuid] = physicalIndex;
                            dinputDeviceUsed[i] = true; // Marquer comme utilisé / Mark as used
                            
                            wchar_t guidStr[40];
                            StringFromGUID2(dinputDevice.instanceGuid, guidStr, 40);
                            WriteToLog(L"[Mapping] Trouvé! " + std::wstring(guidStr) + L" -> Index Physique " + std::to_wstring(physicalIndex));
                            
                            // On a trouvé le mapping pour ce périphérique XInput, on peut passer au suivant / Found mapping for this XInput device, can move to next
                            break; 
                        }
                    }
                }
            }
        }
    }

    // 4. Remplir la map inversée pour une recherche rapide / 4. Fill reverse map for fast lookup
    g_PhysicalIndexToGuidMap.clear();
    for (const auto& pair : g_GuidToPhysicalIndexMap) {
        g_PhysicalIndexToGuidMap[pair.second] = pair.first;
    }

    g_MappingInitialized = true;
    WriteToLog(L"--- Fin de l'initialisation du mapping ---");
}

bool InitializeMinHook() {
    WriteToLog("Initialisation de MinHook...");
    
    // Initialiser MinHook / Initialize MinHook
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        WriteToLog(("Echec de l'initialisation de MinHook: " + std::to_string(status)).c_str());
        return false;
    }
    
    // Charger la bonne version de XInput / Load correct XInput version
    if (!LoadXInput()) {
        return false;
    }
    
    // Fonction utilitaire pour logger le résultat de la création d'un hook / Utility function to log hook creation result
    auto CreateAndLogHook = [](LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour, LPVOID* ppOriginal) -> MH_STATUS {
        std::string hookMsg = GetLocalizedMessage("Creation du hook pour ", "Creating hook for ") + std::string(pszProcName) + "...";
        WriteToLog(hookMsg.c_str());
        MH_STATUS status = MH_CreateHookApi(pszModule, pszProcName, pDetour, ppOriginal);
        if (status != MH_OK) {
            std::string errorMsg = GetLocalizedMessage("Echec du hook ", "Hook failed for ") + std::string(pszProcName) + ": " + std::to_string(status);
            WriteToLog(errorMsg.c_str());
        } else {
            std::string successMsg = GetLocalizedMessage("Hook reussi pour ", "Hook successful for ") + std::string(pszProcName);
            WriteToLog(successMsg.c_str());
        }
        return status;
    };

    // Créer les hooks pour les fonctions XInput une par une / Create hooks for XInput functions one by one
    bool allHooksSucceeded = true;
    
    // Utiliser le nom de la DLL chargée (sans le chemin) / Use loaded DLL name (without path)
    wchar_t baseDllName[MAX_PATH] = L"xinput1_4.dll";  // Par défaut, on utilise xinput1_4 / By default, use xinput1_4
    
    // Essayer d'obtenir le nom de base plus précisément / Try to get base name more precisely
    wchar_t dllPath[MAX_PATH];
    if (g_XInputDLL && GetModuleFileNameW(g_XInputDLL, dllPath, MAX_PATH) != 0) {
        // Extraire juste le nom du fichier / Extract just the filename
        wchar_t* lastBackslash = wcsrchr(dllPath, L'\\\\');
        if (lastBackslash != nullptr) {
            wcscpy_s(baseDllName, lastBackslash + 1);
        }
    }
    
    WriteToLog(("Utilisation de la DLL XInput: " + WStringToString(baseDllName)).c_str());
    
    // Créer chaque hook individuellement pour un meilleur suivi / Create each hook individually for better tracking
    if (CreateAndLogHook(baseDllName, "XInputGetState", &XInputGetState_Hook, reinterpret_cast<LPVOID*>(&OriginalXInputGetState)) != MH_OK) {
        allHooksSucceeded = false;
    }
    
    if (CreateAndLogHook(baseDllName, "XInputSetState", &XInputSetState_Hook, reinterpret_cast<LPVOID*>(&OriginalXInputSetState)) != MH_OK) {
        allHooksSucceeded = false;
    }
    
    if (CreateAndLogHook(baseDllName, "XInputGetCapabilities", &XInputGetCapabilities_Hook, 
                         reinterpret_cast<LPVOID*>(&OriginalXInputGetCapabilities)) != MH_OK) {
        allHooksSucceeded = false;
    }
    
    if (CreateAndLogHook(baseDllName, "XInputEnable", &XInputEnable_Hook, 
                         reinterpret_cast<LPVOID*>(&OriginalXInputEnable)) != MH_OK) {
        allHooksSucceeded = false;
    }
    
    // XInputGetDSoundAudioDeviceGuids peut ne pas exister dans toutes les versions / XInputGetDSoundAudioDeviceGuids may not exist in all versions
    if (GetProcAddress(g_XInputDLL, "XInputGetDSoundAudioDeviceGuids") != nullptr) {
        if (CreateAndLogHook(baseDllName, "XInputGetDSoundAudioDeviceGuids", &XInputGetDSoundAudioDeviceGuids_Hook, reinterpret_cast<LPVOID*>(&OriginalXInputGetDSoundAudioDeviceGuids)) != MH_OK) {
            allHooksSucceeded = false;
        }
    } else {
        WriteToLog("XInputGetDSoundAudioDeviceGuids non trouvé, ignoré");
    }
    
    if (!allHooksSucceeded) {
        WriteToLog("Un ou plusieurs hooks ont échoué, mais on continue quand même");
    }
    
    // XInputGetStateEx (fonction non documentée, utilise l'ordinal 100) / XInputGetStateEx (undocumented function, uses ordinal 100)
    if (g_XInputDLL) {
        FARPROC pGetStateEx = GetProcAddress(g_XInputDLL, (LPCSTR)100);
        if (pGetStateEx != nullptr) {
            WriteToLog(GetLocalizedMessage("Tentative de creation du hook XInputGetStateEx (ordinal 100)...", "Attempting to create XInputGetStateEx hook (ordinal 100)..."));
            MH_STATUS status = MH_CreateHook(pGetStateEx, &XInputGetStateEx_Hook, reinterpret_cast<LPVOID*>(&OriginalXInputGetStateEx));
            if (status != MH_OK) {
                std::string errorMsg = GetLocalizedMessage("Echec du hook XInputGetStateEx: ", "XInputGetStateEx hook failed: ") + std::to_string(status);
                WriteToLog(errorMsg.c_str());
            } else {
                WriteToLog(GetLocalizedMessage("Hook XInputGetStateEx reussi", "XInputGetStateEx hook successful"));
            }
        } else {
            WriteToLog(GetLocalizedMessage("XInputGetStateEx non trouve, ignore", "XInputGetStateEx not found, ignored"));
        }
    }
    
    // Activer tous les hooks / Enable all hooks
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        WriteToLog("Echec de l'activation des hooks");
        return false;
    }

    WriteToLog("MinHook initialise avec succes avec les hooks XInput");
    return true;
}

// Point d'entrée de la DLL / DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            // Désactiver les appels de thread inutiles / Disable unnecessary thread calls
            DisableThreadLibraryCalls(hModule);
            
            // Détecter la langue du système / Detect system language
            g_UseFrenchLanguage = DetectFrenchSystem();
            
            // Message de test / Test message
            WriteToLog(GetLocalizedMessage("DLL chargee avec succes!", "DLL loaded successfully!"));
            
            // Charger la configuration AVANT d'initialiser les hooks / Load configuration BEFORE initializing hooks
            LoadConfiguration();
            
            // Initialiser MinHook / Initialize MinHook
            if (!InitializeMinHook()) {
                WriteToLog("Echec de l'initialisation des hooks");
                return TRUE; // On continue quand même le chargement / Continue loading anyway
            }
            
            // Initialiser le mapping après avoir récupéré les pointeurs originaux / Initialize mapping after getting original pointers
            InitializeControllerMapping();
            
            // Initialiser l'overlay système / Initialize system overlay
            WriteToLog(L"[Overlay] Overlay initialisé");
            
            break;
        }
        case DLL_PROCESS_DETACH: {
            // Nettoyer l'overlay système / Clean up system overlay
            WriteToLog(L"[Overlay] Overlay fermé");
            
            // Nettoyer MinHook / Clean up MinHook
            MH_Uninitialize();
            WriteToLog(GetLocalizedMessage("DLL dechargee", "DLL unloaded"));
            break;
        }
    }
    return TRUE;
}

// Implémentation de WriteToLog pour const char* / WriteToLog implementation for const char*
void WriteToLog(const char* message) {
    if (!message) return;
    std::string msg(message);
    WriteToLog(msg); // Appel à la version std::string / Call std::string version
}

// Implémentation de WriteToLog pour const wchar_t* / WriteToLog implementation for const wchar_t*
void WriteToLog(const wchar_t* message) {
    if (!message) return;
    std::wstring wmsg(message);
    WriteToLog(wmsg); // Appel à la version std::wstring / Call std::wstring version
}

// Fonction pour obtenir le nom de l'exécutable en focus / Function to get focused executable name
std::wstring GetForegroundProcessName() {
    HWND hwnd = GetForegroundWindow();
    if (hwnd == NULL) return L"";
    
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) return L"";
    
    wchar_t processName[MAX_PATH] = {0};
    if (GetModuleBaseNameW(hProcess, NULL, processName, MAX_PATH) == 0) {
        CloseHandle(hProcess);
        return L"";
    }
    
    CloseHandle(hProcess);
    return std::wstring(processName);
}

// Fonction pour ajouter un jeu à la liste [Games] dans le fichier de configuration / Function to add game to [Games] list in configuration file
bool AddGameToConfig(const std::wstring& gameName) {
    if (gameName.empty()) return false;
    
    std::wstring configPath = GetConfigPath();
    if (configPath.empty()) return false;
    
    // Lire tout le fichier de configuration / Read entire configuration file
    std::wifstream configFile(configPath);
    if (!configFile.is_open()) return false;
    
    std::vector<std::wstring> lines;
    std::wstring line;
    bool inGamesSection = false;
    bool gameExists = false;
    
    // Lire toutes les lignes / Read all lines
    while (std::getline(configFile, line)) {
        // Supprimer les caractères de retour chariot / Remove carriage return characters
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        
        // Vérifier si on entre dans la section [Games] / Check if entering [Games] section
        if (line == L"[Games]") {
            inGamesSection = true;
        } else if (line.length() > 0 && line[0] == L'[') {
            inGamesSection = false;
        }
        
        // Vérifier si le jeu existe déjà / Check if game already exists
        if (inGamesSection && line == gameName) {
            gameExists = true;
        }
        
        lines.push_back(line);
    }
    configFile.close();
    
    if (gameExists) {
        WriteToLog(L"[AddGame] Le jeu '" + gameName + L"' existe déjà dans la configuration");
        return false;
    }
    
    // Ajouter le jeu à la fin de la section [Games] / Add game to end of [Games] section
    bool gameAdded = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] == L"[Games]") {
            // Trouver la fin de la section [Games] / Find end of [Games] section
            size_t insertPos = i + 1;
            while (insertPos < lines.size() && 
                   (lines[insertPos].empty() || 
                    lines[insertPos][0] == L';' || 
                    (lines[insertPos][0] != L'[' && lines[insertPos].find(L'=') == std::wstring::npos))) {
                insertPos++;
            }
            
            // Insérer le nouveau jeu avant la prochaine section ou à la fin / Insert new game before next section or at end
            lines.insert(lines.begin() + insertPos, gameName);
            gameAdded = true;
            break;
        }
    }
    
    if (!gameAdded) {
        WriteToLog(L"[AddGame] Section [Games] non trouvée dans la configuration");
        return false;
    }
    
    // Réécrire le fichier / Rewrite file
    std::wofstream outFile(configPath);
    if (!outFile.is_open()) return false;
    
    for (const auto& fileLine : lines) {
        outFile << fileLine << L"\n";
    }
    outFile.close();
    
    WriteToLog(L"[AddGame] Jeu '" + gameName + L"' ajouté avec succès à la configuration");
    return true;
}

#endif // XORDERHOOK_SIMPLE_CPP

