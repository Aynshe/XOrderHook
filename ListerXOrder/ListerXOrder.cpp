// ListerXOrder.cpp : Liste les manettes en utilisant DirectInput pour obtenir des GUIDs stables.
// ListerXOrder.cpp : Lists controllers using DirectInput to obtain stable GUIDs.
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>
#include <dinput.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <oleauto.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <locale>
#include <codecvt>

// SDL3 includes
#include <SDL3/SDL.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "SDL3.lib")

// Structure pour stocker les informations d'un contrôleur
// Structure to store controller information
struct ControllerInfo {
    std::wstring name;
    std::wstring guidStr;
    std::wstring productGuidStr;
    GUID instanceGuid;
    GUID productGuid;
    unsigned short vid;
    unsigned short pid;
    bool isXInput;
    std::wstring hidPath;
    std::wstring manufacturer;
    std::wstring serialNumber;
    unsigned short usagePage;
    unsigned short usage;
    std::wstring devicePath;
};

// Structure pour les informations HID
// Structure for HID information
struct HIDDeviceInfo {
    std::wstring devicePath;
    std::wstring manufacturer;
    std::wstring product;
    std::wstring serialNumber;
    unsigned short vid;
    unsigned short pid;
    unsigned short usagePage;
    unsigned short usage;
    GUID classGuid;
};

// Structure pour les informations SDL3
// Structure for SDL3 information
struct SDL3ControllerInfo {
    std::wstring name;
    std::wstring guid;
    unsigned short vid;
    unsigned short pid;
    int instanceId;
    bool isGameController;
    std::wstring path;
};

std::vector<ControllerInfo> g_controllers;
std::vector<HIDDeviceInfo> g_hidDevices;
std::vector<SDL3ControllerInfo> g_sdl3Controllers;

// Variable globale pour la langue
// Global variable for language
bool g_isFrench = true;

// Structure pour les chaînes localisées
// Structure for localized strings
struct LocalizedStrings {
    const wchar_t* title;
    const wchar_t* separator;
    const wchar_t* enumeratingHID;
    const wchar_t* foundHIDDevices;
    const wchar_t* enumeratingSDL3;
    const wchar_t* foundSDL3Controllers;
    const wchar_t* enumeratingDirectInput;
    const wchar_t* errorInitDirectInput;
    const wchar_t* errorEnumDevices;
    const wchar_t* noControllerDetected;
    const wchar_t* controllersDetected;
    const wchar_t* name;
    const wchar_t* type;
    const wchar_t* xinputProbable;
    const wchar_t* directInput;
    const wchar_t* guidInstance;
    const wchar_t* guidProduct;
    const wchar_t* hidPath;
    const wchar_t* manufacturer;
    const wchar_t* serialNumber;
    const wchar_t* usagePageUsage;
    const wchar_t* hidInfoNotAvailable;
    const wchar_t* configurationTitle;
    const wchar_t* configurationHeader;
    const wchar_t* configurationInstructions1;
    const wchar_t* configurationInstructions2;
    const wchar_t* configurationWarning;
    const wchar_t* hidDevicesDetected;
    const wchar_t* product;
    const wchar_t* path;
    const wchar_t* sdl3ControllersDetected;
    const wchar_t* gameController;
    const wchar_t* joystick;
    const wchar_t* guidSDL3;
    const wchar_t* instanceID;
    const wchar_t* pressEnterToQuit;
    const wchar_t* unknownController;
    const wchar_t* sdl3Error;
};

// Chaînes en français
// French strings
const LocalizedStrings g_frenchStrings = {
    L"ListerXOrder - Utilitaire de listage des manettes",
    L"--------------------------------------------------",
    L"Énumération des périphériques HID...",
    L"Trouvé %d périphériques HID de jeu.",
    L"Énumération des contrôleurs SDL3...",
    L"Trouvé %d contrôleurs SDL3.",
    L"Énumération des périphériques DirectInput...",
    L"Erreur: Impossible d'initialiser DirectInput.",
    L"Erreur: Impossible d'énumérer les périphériques.",
    L"\nAucune manette détectée.",
    L"\n=== Manettes détectées (%d) ===",
    L"Nom       : ",
    L"  -> Type  : ",
    L"XInput (probable)",
    L"DirectInput",
    L"  -> GUID Instance : ",
    L"  -> GUID Produit  : ",
    L"  -> Chemin HID : ",
    L"  -> Fabricant  : ",
    L"  -> N° Série   : ",
    L"  -> Usage Page/Usage : 0x%x / 0x%x",
    L"  -> Informations HID : Non disponibles",
    L"\n\n==================================================\n           CONFIGURATION XOrderConfig.ini           \n==================================================",
    L"Copiez la ligne suivante dans votre fichier XOrderConfig.ini sous la section [ControllerOrder].",
    L"Réorganisez les GUIDs pour définir l'ordre souhaité des manettes (0, 1, 2, ...).",
    L"ATTENTION: OrderByGUID est expérimental. L'index XInput n'étant pas connu en amont,",
    L"l'ordre des manettes XInput ne fonctionnera pas. XOrder ne prend pas en charge DirectInput (utilisez DevReorder).",
    L"\n=== Périphériques HID de jeu détectés (%d) ===",
    L"Produit   : ",
    L"  -> Chemin : ",
    L"\n=== Contrôleurs SDL3 détectés (%d) ===",
    L"GameController",
    L"Joystick",
    L"  -> GUID SDL3 : ",
    L"  -> Instance ID : ",
    L"Appuyez sur Entrée pour quitter...",
    L"Contrôleur inconnu",
    L"Erreur SDL3: "
};

// Chaînes en anglais
// English strings
const LocalizedStrings g_englishStrings = {
    L"ListerXOrder - Controller Listing Utility",
    L"------------------------------------------",
    L"Enumerating HID devices...",
    L"Found %d HID gaming devices.",
    L"Enumerating SDL3 controllers...",
    L"Found %d SDL3 controllers.",
    L"Enumerating DirectInput devices...",
    L"Error: Unable to initialize DirectInput.",
    L"Error: Unable to enumerate devices.",
    L"\nNo controllers detected.",
    L"\n=== Detected Controllers (%d) ===",
    L"Name      : ",
    L"  -> Type  : ",
    L"XInput (probable)",
    L"DirectInput",
    L"  -> Instance GUID : ",
    L"  -> Product GUID  : ",
    L"  -> HID Path : ",
    L"  -> Manufacturer  : ",
    L"  -> Serial Number : ",
    L"  -> Usage Page/Usage : 0x%x / 0x%x",
    L"  -> HID Information : Not available",
    L"\n\n==================================================\n           XOrderConfig.ini CONFIGURATION           \n==================================================",
    L"Copy the following line to your XOrderConfig.ini file under the [ControllerOrder] section.",
    L"Rearrange the GUIDs to define the desired controller order (0, 1, 2, ...).",
    L"WARNING: OrderByGUID is experimental. Since XInput index is not known in advance,",
    L"XInput controller ordering will not work. XOrder does not support DirectInput (use DevReorder).",
    L"\n=== Detected HID Gaming Devices (%d) ===",
    L"Product   : ",
    L"  -> Path : ",
    L"\n=== Detected SDL3 Controllers (%d) ===",
    L"GameController",
    L"Joystick",
    L"  -> SDL3 GUID : ",
    L"  -> Instance ID : ",
    L"Press Enter to quit...",
    L"Unknown Controller",
    L"SDL3 Error: "
};

// Pointeur vers les chaînes actuelles
// Pointer to current strings
const LocalizedStrings* g_strings = &g_frenchStrings;

// Fonction pour détecter la langue du système
// Function to detect system language
void DetectSystemLanguage() {
    LCID lcid = GetUserDefaultLCID();
    LANGID langId = LANGIDFROMLCID(lcid);
    WORD primaryLang = PRIMARYLANGID(langId);
    
    // Détecter si le système est en français
    // Detect if system is in French
    g_isFrench = (primaryLang == LANG_FRENCH);
    
    // Définir le pointeur vers les chaînes appropriées
    // Set pointer to appropriate strings
    g_strings = g_isFrench ? &g_frenchStrings : &g_englishStrings;
}

// Fonction pour configurer l'encodage UTF-8 de la console
// Function to configure UTF-8 console encoding
void SetupConsoleEncoding() {
    // Définir l'encodage de la console en UTF-8
    // Set console encoding to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Activer le mode UTF-8 pour les flux de sortie standard
    // Enable UTF-8 mode for standard output streams
    std::wcout.imbue(std::locale(""));
    std::wcerr.imbue(std::locale(""));
}

// Helper pour extraire VendorID/ProductID depuis le GUID du produit
// Helper to extract VendorID/ProductID from product GUID
void ExtractVidPidFromProductGuid(const GUID& guid, unsigned short& vid, unsigned short& pid) {
    // Le VID/PID est encodé dans le Data1 du GUID produit pour les périphériques DirectInput.
    // VID/PID is encoded in Data1 of product GUID for DirectInput devices.
    vid = HIWORD(guid.Data1);
    pid = LOWORD(guid.Data1);
}

// Fonction pour convertir une chaîne ANSI en wstring
// Function to convert ANSI string to wstring
std::wstring AnsiToWString(const char* ansiStr) {
    if (!ansiStr) return L"";
    
    int len = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, NULL, 0);
    if (len <= 0) return L"";
    
    std::vector<wchar_t> wideStr(len);
    MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, &wideStr[0], len);
    return std::wstring(&wideStr[0]);
}

// Fonction pour obtenir une chaîne depuis un périphérique HID
// Function to get string from HID device
std::wstring GetHIDString(HANDLE hDevice, ULONG stringType) {
    wchar_t buffer[256] = {0};
    BOOLEAN result = FALSE;
    
    switch (stringType) {
        case 1: // Manufacturer / Fabricant
            result = HidD_GetManufacturerString(hDevice, buffer, sizeof(buffer));
            break;
        case 2: // Product / Produit
            result = HidD_GetProductString(hDevice, buffer, sizeof(buffer));
            break;
        case 3: // Serial Number / Numéro de série
            result = HidD_GetSerialNumberString(hDevice, buffer, sizeof(buffer));
            break;
    }
    
    return result ? std::wstring(buffer) : L"";
}

// Fonction pour énumérer les périphériques HID
// Function to enumerate HID devices
void EnumerateHIDDevices() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return;
    }
    
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    for (DWORD deviceIndex = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid, deviceIndex, &deviceInterfaceData); deviceIndex++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);
        
        if (requiredSize == 0) continue;
        
        PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        if (!deviceInterfaceDetailData) continue;
        
        deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        
        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, deviceInterfaceDetailData, requiredSize, NULL, &deviceInfoData)) {
            HANDLE hDevice = CreateFile(deviceInterfaceDetailData->DevicePath,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL);
            
            if (hDevice != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attributes;
                attributes.Size = sizeof(HIDD_ATTRIBUTES);
                
                if (HidD_GetAttributes(hDevice, &attributes)) {
                    PHIDP_PREPARSED_DATA preparsedData;
                    if (HidD_GetPreparsedData(hDevice, &preparsedData)) {
                        HIDP_CAPS caps;
                        if (HidP_GetCaps(preparsedData, &caps) == HIDP_STATUS_SUCCESS) {
                            // Filtrer les périphériques de jeu (Usage Page 1, Usage 4 ou 5)
                            // Filter gaming devices (Usage Page 1, Usage 4 or 5)
                            if (caps.UsagePage == 1 && (caps.Usage == 4 || caps.Usage == 5)) {
                                HIDDeviceInfo hidInfo;
                                
                                // Convertir le chemin du périphérique de ANSI vers Unicode
                                // Convert device path from ANSI to Unicode
                                hidInfo.devicePath = AnsiToWString(deviceInterfaceDetailData->DevicePath);
                                
                                hidInfo.vid = attributes.VendorID;
                                hidInfo.pid = attributes.ProductID;
                                hidInfo.usagePage = caps.UsagePage;
                                hidInfo.usage = caps.Usage;
                                hidInfo.classGuid = hidGuid;
                                
                                hidInfo.manufacturer = GetHIDString(hDevice, 1);
                                hidInfo.product = GetHIDString(hDevice, 2);
                                hidInfo.serialNumber = GetHIDString(hDevice, 3);
                                
                                g_hidDevices.push_back(hidInfo);
                            }
                        }
                        HidD_FreePreparsedData(preparsedData);
                    }
                }
                CloseHandle(hDevice);
            }
        }
        
        free(deviceInterfaceDetailData);
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}

// Fonction pour trouver les informations HID correspondantes
// Function to find matching HID information
HIDDeviceInfo* FindMatchingHIDDevice(unsigned short vid, unsigned short pid) {
    for (auto& hidDevice : g_hidDevices) {
        if (hidDevice.vid == vid && hidDevice.pid == pid) {
            return &hidDevice;
        }
    }
    return nullptr;
}

// Fonction pour énumérer les contrôleurs SDL3
// Function to enumerate SDL3 controllers
void EnumerateSDL3Controllers() {
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) < 0) {
        std::wcerr << g_strings->sdl3Error << AnsiToWString(SDL_GetError()) << std::endl;
        return;
    }

    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    
    if (joysticks) {
        for (int i = 0; i < numJoysticks; i++) {
            SDL_Joystick* joystick = SDL_OpenJoystick(joysticks[i]);
            if (!joystick) {
                continue;
            }
            
            SDL3ControllerInfo sdlInfo;
            
            // Obtenir les informations de base
            // Get basic information
            const char* name = SDL_GetJoystickName(joystick);
            sdlInfo.name = name ? AnsiToWString(name) : g_strings->unknownController;
            sdlInfo.instanceId = joysticks[i];
            
            // Obtenir le GUID
            // Get GUID
            SDL_GUID guid = SDL_GetJoystickGUID(joystick);
            char guidStr[33];
            SDL_GUIDToString(guid, guidStr, sizeof(guidStr));
            sdlInfo.guid = AnsiToWString(guidStr);
            
            // Obtenir VID/PID
            // Get VID/PID
            sdlInfo.vid = SDL_GetJoystickVendor(joystick);
            sdlInfo.pid = SDL_GetJoystickProduct(joystick);
            
            // Vérifier si c'est un contrôleur de jeu
            // Check if it's a game controller
            sdlInfo.isGameController = SDL_IsGamepad(joysticks[i]);
            
            // Obtenir le chemin si disponible
            // Get path if available
            const char* path = SDL_GetJoystickPath(joystick);
            sdlInfo.path = path ? AnsiToWString(path) : L"";
            
            g_sdl3Controllers.push_back(sdlInfo);
            
            // Fermer le joystick
            // Close joystick
            SDL_CloseJoystick(joystick);
        }
        SDL_free(joysticks);
    }
    
    SDL_Quit();
}

// Callback pour l'énumération des périphériques DirectInput
// Callback for DirectInput device enumeration
BOOL CALLBACK EnumJoysticksCallback(LPCDIDEVICEINSTANCEW pdidInstance, LPVOID pContext) {
    ControllerInfo info;
    info.name = pdidInstance->tszProductName;
    info.instanceGuid = pdidInstance->guidInstance;
    info.productGuid = pdidInstance->guidProduct;

    // Convertir le GUID d'instance en chaîne
    // Convert instance GUID to string
    LPOLESTR guidOleStr;
    if (SUCCEEDED(StringFromCLSID(pdidInstance->guidInstance, &guidOleStr))) {
        info.guidStr = guidOleStr;
        CoTaskMemFree(guidOleStr);
    }

    // Convertir le GUID produit en chaîne
    // Convert product GUID to string
    if (SUCCEEDED(StringFromCLSID(pdidInstance->guidProduct, &guidOleStr))) {
        info.productGuidStr = guidOleStr;
        CoTaskMemFree(guidOleStr);
    }

    // Extraire VID/PID depuis le GUID produit
    // Extract VID/PID from product GUID
    ExtractVidPidFromProductGuid(pdidInstance->guidProduct, info.vid, info.pid);

    // Heuristique simple pour détecter un périphérique XInput
    // Simple heuristic to detect XInput device
    info.isXInput = (wcsstr(pdidInstance->tszProductName, L"XBOX") != nullptr || 
                     wcsstr(pdidInstance->tszProductName, L"XInput") != nullptr ||
                     wcsstr(pdidInstance->tszProductName, L"Controller") != nullptr);

    // Chercher les informations HID correspondantes
    // Search for matching HID information
    HIDDeviceInfo* hidInfo = FindMatchingHIDDevice(info.vid, info.pid);
    if (hidInfo) {
        info.hidPath = hidInfo->devicePath;
        info.manufacturer = hidInfo->manufacturer;
        info.serialNumber = hidInfo->serialNumber;
        info.usagePage = hidInfo->usagePage;
        info.usage = hidInfo->usage;
        info.devicePath = hidInfo->devicePath;
        
        // Si le nom DirectInput est générique, utiliser le nom HID
        // If DirectInput name is generic, use HID name
        if (info.name == L"Controller" || info.name.empty()) {
            info.name = hidInfo->product;
        }
    }

    g_controllers.push_back(info);

    return DIENUM_CONTINUE;
}

int main() {
    // Détecter la langue du système
    // Detect system language
    DetectSystemLanguage();
    
    // Configurer l'encodage de la console
    // Configure console encoding
    SetupConsoleEncoding();
    
    std::wcout << g_strings->title << std::endl;
    std::wcout << g_strings->separator << std::endl;

    // Énumérer d'abord les périphériques HID
    // First enumerate HID devices
    std::wcout << g_strings->enumeratingHID << std::endl;
    EnumerateHIDDevices();
    wprintf(g_strings->foundHIDDevices, (int)g_hidDevices.size());
    std::wcout << std::endl;

    // Énumérer les contrôleurs SDL3
    // Enumerate SDL3 controllers
    std::wcout << g_strings->enumeratingSDL3 << std::endl;
    EnumerateSDL3Controllers();
    wprintf(g_strings->foundSDL3Controllers, (int)g_sdl3Controllers.size());
    std::wcout << std::endl;

    LPDIRECTINPUT8W pDI;
    HRESULT hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8W, (VOID**)&pDI, NULL);

    if (FAILED(hr)) {
        std::wcerr << g_strings->errorInitDirectInput << std::endl;
        return 1;
    }

    // Énumérer les joysticks
    // Enumerate joysticks
    std::wcout << g_strings->enumeratingDirectInput << std::endl;
    hr = pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY);

    if (FAILED(hr)) {
        std::wcerr << g_strings->errorEnumDevices << std::endl;
        pDI->Release();
        return 1;
    }

    if (g_controllers.empty()) {
        std::wcout << g_strings->noControllerDetected << std::endl;
    } else {
        wprintf(g_strings->controllersDetected, (int)g_controllers.size());
        std::wcout << std::endl;
        
        std::vector<std::wstring> guidList;

        for (const auto& controller : g_controllers) {
            std::wcout << L"\n--------------------------------------------------" << std::endl;
            std::wcout << g_strings->name << controller.name << std::endl;
            std::wcout << g_strings->type << (controller.isXInput ? g_strings->xinputProbable : g_strings->directInput) << std::endl;
            std::wcout << L"  -> VID/PID : 0x" << std::hex << std::setw(4) << std::setfill(L'0') << controller.vid
                      << L" / 0x" << std::setw(4) << std::setfill(L'0') << controller.pid << std::dec << std::endl;
            std::wcout << g_strings->guidInstance << controller.guidStr << std::endl;
            std::wcout << g_strings->guidProduct << controller.productGuidStr << std::endl;
            
            // Afficher les informations HID si disponibles
            // Display HID information if available
            if (!controller.hidPath.empty()) {
                std::wcout << g_strings->hidPath << controller.hidPath << std::endl;
                if (!controller.manufacturer.empty()) {
                    std::wcout << g_strings->manufacturer << controller.manufacturer << std::endl;
                }
                if (!controller.serialNumber.empty()) {
                    std::wcout << g_strings->serialNumber << controller.serialNumber << std::endl;
                }
                wprintf(g_strings->usagePageUsage, controller.usagePage, controller.usage);
                std::wcout << std::endl;
            } else {
                std::wcout << g_strings->hidInfoNotAvailable << std::endl;
            }
            
            guidList.push_back(controller.guidStr);
        }

        std::wcout << g_strings->configurationTitle << std::endl;
        std::wcout << g_strings->configurationHeader << std::endl;
        std::wcout << g_strings->configurationInstructions1 << std::endl;
        std::wcout << g_strings->configurationInstructions2 << std::endl;
        std::wcout << g_strings->configurationWarning << std::endl;
        std::wcout << L"\nOrderByGUID=";
        for (size_t i = 0; i < guidList.size(); ++i) {
            std::wcout << guidList[i] << (i == guidList.size() - 1 ? L"" : L",");
        }
        std::wcout << L"\n\n" << std::endl;
    }

    // Afficher les périphériques HID détectés pour le débogage
    // Display detected HID devices for debugging
    if (!g_hidDevices.empty()) {
        wprintf(g_strings->hidDevicesDetected, (int)g_hidDevices.size());
        std::wcout << std::endl;
        for (const auto& hidDevice : g_hidDevices) {
            std::wcout << L"\n--------------------------------------------------" << std::endl;
            std::wcout << g_strings->product << (hidDevice.product.empty() ? L"(Non spécifié)" : hidDevice.product) << std::endl;
            std::wcout << L"  -> VID/PID : 0x" << std::hex << std::setw(4) << std::setfill(L'0') << hidDevice.vid
                      << L" / 0x" << std::setw(4) << std::setfill(L'0') << hidDevice.pid << std::dec << std::endl;
            if (!hidDevice.manufacturer.empty()) {
                std::wcout << g_strings->manufacturer << hidDevice.manufacturer << std::endl;
            }
            if (!hidDevice.serialNumber.empty()) {
                std::wcout << g_strings->serialNumber << hidDevice.serialNumber << std::endl;
            }
            wprintf(g_strings->usagePageUsage, hidDevice.usagePage, hidDevice.usage);
            std::wcout << std::endl;
            std::wcout << g_strings->path << hidDevice.devicePath << std::endl;
        }
        std::wcout << L"\n" << std::endl;
    }

    // Afficher les contrôleurs SDL3 détectés
    // Display detected SDL3 controllers
    if (!g_sdl3Controllers.empty()) {
        wprintf(g_strings->sdl3ControllersDetected, (int)g_sdl3Controllers.size());
        std::wcout << std::endl;
        for (const auto& sdlController : g_sdl3Controllers) {
            std::wcout << L"\n--------------------------------------------------" << std::endl;
            std::wcout << g_strings->name << sdlController.name << std::endl;
            std::wcout << g_strings->type << (sdlController.isGameController ? g_strings->gameController : g_strings->joystick) << std::endl;
            std::wcout << L"  -> VID/PID : 0x" << std::hex << std::setw(4) << std::setfill(L'0') << sdlController.vid
                      << L" / 0x" << std::setw(4) << std::setfill(L'0') << sdlController.pid << std::dec << std::endl;
            std::wcout << g_strings->guidSDL3 << sdlController.guid << std::endl;
            std::wcout << g_strings->instanceID << sdlController.instanceId << std::endl;
            if (!sdlController.path.empty()) {
                std::wcout << g_strings->path << sdlController.path << std::endl;
            }
        }
        std::wcout << L"\n" << std::endl;
    }

    pDI->Release();

    std::wcout << g_strings->pressEnterToQuit;
    std::cin.get();

    return 0;
}