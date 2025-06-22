// XOrderInjector.cpp : Watchdog qui surveille les jeux et injecte XOrderHook.dll automatiquement / Watchdog that monitors games and automatically injects XOrderHook.dll
#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <psapi.h> // Pour EnumProcessModules / For EnumProcessModules
#include <algorithm>
#include <vector>
#include <codecvt>
#include <locale>
#include <winternl.h> // Pour NtQueryInformationProcess / For NtQueryInformationProcess
#include <xinput.h>    // Pour la détection des manettes / For controller detection
#include <map>
#include <commctrl.h>  // Pour les contrôles Windows / For Windows controls
#include "InjectorOverlay.h"

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Variable pour la langue du système (true = français/belge, false = anglais) / Variable for system language (true = French/Belgian, false = English)
static bool g_UseFrenchLanguage = false;

// Fonction pour détecter si le système est en français ou belge / Function to detect if system is French or Belgian
bool DetectFrenchSystem() {
    LCID lcid = GetUserDefaultLCID();
    LANGID langId = LANGIDFROMLCID(lcid);
    WORD primaryLang = PRIMARYLANGID(langId);
    WORD subLang = SUBLANGID(langId);
    
    // Français (France, Belgique, Canada, Suisse, etc.) / French (France, Belgium, Canada, Switzerland, etc.)
    if (primaryLang == LANG_FRENCH) {
        return true;
    }
    
    // Belge (néerlandais de Belgique peut aussi être considéré) / Belgian (Dutch from Belgium can also be considered)
    if (primaryLang == LANG_DUTCH && subLang == SUBLANG_DUTCH_BELGIAN) {
        return true;
    }
    
    return false;
}

// Fonction pour obtenir un message localisé / Function to get localized message
const char* GetLocalizedMessage(const char* frenchMsg, const char* englishMsg) {
    return g_UseFrenchLanguage ? frenchMsg : englishMsg;
}

bool EnableDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        std::cerr << "[XOrderInjector] OpenProcessToken échoué: " << GetLastError() << std::endl; // OpenProcessToken failed
        return false;
    }
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        std::cerr << "[XOrderInjector] LookupPrivilegeValue échoué: " << GetLastError() << std::endl; // LookupPrivilegeValue failed
        CloseHandle(hToken);
        return false;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL)) {
        std::cerr << "[XOrderInjector] AdjustTokenPrivileges échoué: " << GetLastError() << std::endl; // AdjustTokenPrivileges failed
        CloseHandle(hToken);
        return false;
    }
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        std::cerr << "[XOrderInjector] Le privilège SeDebugPrivilege n'a pas pu être assigné." << std::endl; // SeDebugPrivilege could not be assigned
        CloseHandle(hToken);
        return false;
    }
    std::cout << "[XOrderInjector] SeDebugPrivilege activé avec succès." << std::endl; // SeDebugPrivilege enabled successfully
    CloseHandle(hToken);
    return true;
}

// Énumération pour l'architecture des processus (définie en premier) / Enumeration for process architecture (defined first)
enum class ProcessArchitecture {
    Unknown,
    x86,
    x64
};

// Déclarations anticipées (après l'énumération) / Forward declarations (after enumeration)
std::wstring StringToWString(const std::string& str);
bool InjectDLLSimple(DWORD pid, const std::string& dllPath);
bool InjectDLLWithHelper(DWORD pid, const std::string& dllPath, ProcessArchitecture targetArch);

// Fonction pour détecter l'architecture d'un processus / Function to detect process architecture
ProcessArchitecture GetProcessArchitecture(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) {
        return ProcessArchitecture::Unknown;
    }

    ProcessArchitecture arch = ProcessArchitecture::Unknown;

    // Méthode 1: Utiliser IsWow64Process pour détecter les processus 32-bit sur système 64-bit / Method 1: Use IsWow64Process to detect 32-bit processes on 64-bit system
    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProcess, &isWow64)) {
        if (isWow64) {
            // Le processus est 32-bit s'exécutant sur un système 64-bit / Process is 32-bit running on 64-bit system
            arch = ProcessArchitecture::x86;
        } else {
            // Le processus pourrait être 64-bit ou 32-bit sur système 32-bit / Process could be 64-bit or 32-bit on 32-bit system
            // Vérifier si nous sommes sur un système 64-bit / Check if we're on 64-bit system
            BOOL isCurrentWow64 = FALSE;
            if (IsWow64Process(GetCurrentProcess(), &isCurrentWow64)) {
                if (isCurrentWow64) {
                    // Nous sommes 32-bit sur système 64-bit, donc le processus cible est probablement 64-bit / We're 32-bit on 64-bit system, so target process is probably 64-bit
                    arch = ProcessArchitecture::x64;
                } else {
                    // Nous sommes sur un système 64-bit natif, le processus cible est 64-bit / We're on native 64-bit system, target process is 64-bit
                    arch = ProcessArchitecture::x64;
                }
            }
        }
    }

    // Méthode 2: Vérifier l'architecture via les modules chargés si la méthode 1 échoue / Method 2: Check architecture via loaded modules if method 1 fails
    if (arch == ProcessArchitecture::Unknown) {
        HMODULE hMods[1024];
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            if (cbNeeded > 0) {
                wchar_t szModName[MAX_PATH];
                if (GetModuleFileNameExW(hProcess, hMods[0], szModName, MAX_PATH)) {
                    // Analyser le fichier PE pour déterminer l'architecture / Analyze PE file to determine architecture
                    HANDLE hFile = CreateFileW(szModName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        IMAGE_DOS_HEADER dosHeader;
                        DWORD bytesRead;
                        if (ReadFile(hFile, &dosHeader, sizeof(dosHeader), &bytesRead, NULL) && 
                            bytesRead == sizeof(dosHeader) && dosHeader.e_magic == IMAGE_DOS_SIGNATURE) {
                            
                            SetFilePointer(hFile, dosHeader.e_lfanew, NULL, FILE_BEGIN);
                            IMAGE_NT_HEADERS ntHeaders;
                            if (ReadFile(hFile, &ntHeaders, sizeof(ntHeaders), &bytesRead, NULL) && 
                                bytesRead == sizeof(ntHeaders) && ntHeaders.Signature == IMAGE_NT_SIGNATURE) {
                                
                                switch (ntHeaders.FileHeader.Machine) {
                                    case IMAGE_FILE_MACHINE_I386:
                                        arch = ProcessArchitecture::x86;
                                        break;
                                    case IMAGE_FILE_MACHINE_AMD64:
                                        arch = ProcessArchitecture::x64;
                                        break;
                                }
                            }
                        }
                        CloseHandle(hFile);
                    }
                }
            }
        }
    }

    CloseHandle(hProcess);
    return arch;
}

// Fonction pour obtenir le nom de l'architecture sous forme de chaîne / Function to get architecture name as string
std::string ArchitectureToString(ProcessArchitecture arch) {
    switch (arch) {
        case ProcessArchitecture::x86: return "x86";
        case ProcessArchitecture::x64: return "x64";
        default: return "Unknown";
    }
}

// Fonction pour construire le chemin de la DLL appropriée selon l'architecture / Function to build appropriate DLL path according to architecture
std::string GetArchSpecificDllPath(const std::string& baseDllPath, ProcessArchitecture arch) {
    // Extraire le répertoire et le nom de base de la DLL / Extract directory and base name of DLL
    size_t lastSlash = baseDllPath.find_last_of("\\/");
    std::string directory = (lastSlash != std::string::npos) ? baseDllPath.substr(0, lastSlash + 1) : "";
    std::string filename = (lastSlash != std::string::npos) ? baseDllPath.substr(lastSlash + 1) : baseDllPath;
    
    // Enlever l'extension .dll si présente / Remove .dll extension if present
    size_t dotPos = filename.find_last_of('.');
    std::string baseName = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;
    
    // Construire le nom de fichier spécifique à l'architecture / Build architecture-specific filename
    std::string archSpecificName;
    switch (arch) {
        case ProcessArchitecture::x86:
            archSpecificName = baseName + "_x86.dll";
            break;
        case ProcessArchitecture::x64:
            archSpecificName = baseName + "_x64.dll";
            break;
        default:
            // Si l'architecture est inconnue, utiliser la DLL originale / If architecture is unknown, use original DLL
            return baseDllPath;
    }
    
    std::string archSpecificPath = directory + archSpecificName;
    
    // Vérifier si le fichier spécifique à l'architecture existe / Check if architecture-specific file exists
    std::ifstream testFile(archSpecificPath);
    if (testFile.good()) {
        testFile.close();
        return archSpecificPath;
    }
    
    // Si le fichier spécifique n'existe pas, essayer dans un sous-dossier / If specific file doesn't exist, try in subfolder
    std::string archFolder = directory + ArchitectureToString(arch) + "\\" + filename;
    std::ifstream testFile2(archFolder);
    if (testFile2.good()) {
        testFile2.close();
        return archFolder;
    }
    
    // Si aucune version spécifique n'est trouvée, utiliser la DLL originale / If no specific version found, use original DLL
    std::cout << "[WARN] DLL spécifique à l'architecture " << ArchitectureToString(arch) 
              << " non trouvée, utilisation de la DLL par défaut: " << baseDllPath << std::endl; // Architecture-specific DLL not found, using default DLL
    return baseDllPath;
}

// Fonction utilitaire pour nettoyer un nom de fichier / Utility function to clean a filename
std::string CleanExeName(const std::string& name) {
    if (name.empty()) return "";
    
    // Convertir d'abord en wstring pour gérer correctement les caractères spéciaux / First convert to wstring to properly handle special characters
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (int)name.length(), NULL, 0);
    if (size_needed <= 0) return "";
    
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (int)name.length(), &wstr[0], size_needed);
    
    // Supprimer le chemin si présent / Remove path if present
    size_t lastSlash = wstr.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        wstr = wstr.substr(lastSlash + 1);
    }
    
    // Supprimer les guillemets s'ils sont présents / Remove quotes if present
    if (!wstr.empty() && wstr[0] == L'"') {
        wstr.erase(0, 1);
    }
    if (!wstr.empty() && wstr.back() == L'"') {
        wstr.pop_back();
    }
    
    // Supprimer les espaces en fin de chaîne / Remove trailing spaces
    wstr.erase(wstr.find_last_not_of(L" \t") + 1);
    
    // Convertir en minuscules pour la comparaison / Convert to lowercase for comparison
    std::transform(wstr.begin(), wstr.end(), wstr.begin(),
        [](wchar_t c) { return (wchar_t)std::tolower(c); });
    
    // Convertir de nouveau en UTF-8 / Convert back to UTF-8
    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), 
                                      NULL, 0, NULL, NULL);
    if (utf8_size <= 0) return "";
    
    std::string result(utf8_size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(),
                       &result[0], utf8_size, NULL, NULL);
    
    return result;
}

// Variable globale pour le mode debug / Global variable for debug mode
bool g_enableDebugConsole = false;

// Variables pour la détection des manettes et l'ajout de jeux / Variables for controller detection and game addition
static std::map<DWORD, ULONGLONG> g_AddGameComboPressTime; // controllerIndex -> time
static std::map<DWORD, int> g_AddGameSecondsHeld;          // controllerIndex -> seconds
static std::map<DWORD, ULONGLONG> g_VibrationStopTime;     // controllerIndex -> time to stop vibration
static std::map<DWORD, ULONGLONG> g_LastStartPressTime;    // controllerIndex -> time of last START press
static bool g_NeedReloadConfig = false;                    // Indique qu'il faut recharger la configuration / Indicates that configuration needs to be reloaded

// Constantes pour éviter l'interférence avec les commandes START / Constants to avoid interference with START commands
const ULONGLONG COMBO_DELAY_MS = 500;  // Délai de grâce de 500ms après un appui sur START seul / 500ms grace delay after single START press

// Fonction pour obtenir le nom de l'exécutable en focus / Function to get focused executable name
std::string GetForegroundProcessName() {
    HWND hwnd = GetForegroundWindow();
    if (hwnd == NULL) return "";
    
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) return "";
    
    char processName[MAX_PATH] = {0};
    if (GetModuleBaseNameA(hProcess, NULL, processName, MAX_PATH) == 0) {
        CloseHandle(hProcess);
        return "";
    }
    
    CloseHandle(hProcess);
    return std::string(processName);
}

// Fonction pour ajouter un jeu à la liste [Games] dans le fichier de configuration / Function to add game to [Games] list in configuration file
bool AddGameToConfig(const std::string& gameName, const std::string& iniPath) {
    std::cout << "[Debug] AddGameToConfig appelée avec: '" << gameName << "' dans '" << iniPath << "'" << std::endl; // AddGameToConfig called with
    
    if (gameName.empty()) {
        std::cout << "[Debug] Nom de jeu vide, abandon" << std::endl; // Empty game name, aborting
        return false;
    }
    
    // Lire tout le fichier de configuration / Read entire configuration file
    std::ifstream configFile(iniPath);
    if (!configFile.is_open()) {
        std::cout << "[Debug] Impossible d'ouvrir le fichier de configuration: " << iniPath << std::endl; // Unable to open configuration file
        return false;
    }
    
    std::vector<std::string> lines;
    std::string line;
    bool inGamesSection = false;
    bool gameExists = false;
    
    std::cout << "[Debug] Lecture du fichier de configuration..." << std::endl; // Reading configuration file...
    
    // Lire toutes les lignes / Read all lines
    while (std::getline(configFile, line)) {
        // Supprimer les caractères de retour chariot / Remove carriage return characters
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Vérifier si on entre dans la section [Games] / Check if entering [Games] section
        if (line == "[Games]") {
            inGamesSection = true;
            std::cout << "[Debug] Section [Games] trouvée" << std::endl; // [Games] section found
        } else if (line.length() > 0 && line[0] == '[') {
            inGamesSection = false;
        }
        
        // Vérifier si le jeu existe déjà / Check if game already exists
        if (inGamesSection && line == gameName) {
            gameExists = true;
            std::cout << "[Debug] Jeu déjà existant trouvé: " << line << std::endl; // Existing game found
        }
        
        lines.push_back(line);
    }
    configFile.close();
    
    std::cout << "[Debug] Fichier lu, " << lines.size() << " lignes, jeu existe: " << (gameExists ? "oui" : "non") << std::endl; // File read, X lines, game exists: yes/no
    
    if (gameExists) {
        std::cout << "[AddGame] Le jeu '" << gameName << "' existe déjà dans la configuration" << std::endl; // Game already exists in configuration
        return false;
    }
    
    // Ajouter le jeu à la fin de la section [Games] / Add game to end of [Games] section
    bool gameAdded = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] == "[Games]") {
            std::cout << "[Debug] Section [Games] trouvée à la ligne " << i << std::endl; // [Games] section found at line
            
            // Trouver la fin de la section [Games] / Find end of [Games] section
            size_t insertPos = i + 1;
            while (insertPos < lines.size() && 
                   (lines[insertPos].empty() || 
                    lines[insertPos][0] == ';' || 
                    (lines[insertPos][0] != '[' && lines[insertPos].find('=') == std::string::npos))) {
                insertPos++;
            }
            
            std::cout << "[Debug] Position d'insertion: " << insertPos << std::endl; // Insertion position
            
            // Insérer le nouveau jeu avant la prochaine section ou à la fin / Insert new game before next section or at end
            lines.insert(lines.begin() + insertPos, gameName);
            gameAdded = true;
            std::cout << "[Debug] Jeu inséré à la position " << insertPos << std::endl; // Game inserted at position
            break;
        }
    }
    
    if (!gameAdded) {
        std::cout << "[AddGame] Section [Games] non trouvée dans la configuration" << std::endl; // [Games] section not found in configuration
        return false;
    }
    
    // Réécrire le fichier / Rewrite the file
    std::cout << "[Debug] Réécriture du fichier..." << std::endl; // Rewriting file...
    std::ofstream outFile(iniPath);
    if (!outFile.is_open()) {
        std::cout << "[Debug] Impossible d'ouvrir le fichier en écriture: " << iniPath << std::endl; // Unable to open file for writing
        return false;
    }
    
    for (const auto& fileLine : lines) {
        outFile << fileLine << "\n";
    }
    outFile.close();
    
    std::cout << "[AddGame] Jeu '" << gameName << "' ajouté avec succès à la configuration" << std::endl; // Game successfully added to configuration
    return true;
}

// Fonction pour vérifier les manettes et détecter les combos / Function to check controllers and detect combos
bool CheckControllerCombos(const std::string& iniPath) {
    ULONGLONG currentTime = GetTickCount64();
    
    // Gérer l'arrêt des vibrations programmées / Handle scheduled vibration stops
    for (auto it = g_VibrationStopTime.begin(); it != g_VibrationStopTime.end(); ) {
        if (currentTime >= it->second) {
            XINPUT_VIBRATION vibration = {0, 0};
            XInputSetState(it->first, &vibration);
            it = g_VibrationStopTime.erase(it);
        } else {
            ++it;
        }
    }
    
    // Vérifier toutes les manettes / Check all controllers
    for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex) {
        XINPUT_STATE state;
        if (XInputGetState(controllerIndex, &state) == ERROR_SUCCESS) {
            bool startPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
            bool dpadUpPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool dpadDownPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            
            // Détecter si START est pressé seul (sans HAUT ni BAS) / Detect if START is pressed alone (without UP or DOWN)
            if (startPressed && !dpadUpPressed && !dpadDownPressed) {
                g_LastStartPressTime[controllerIndex] = currentTime;
            }
            
            // Combo pour ajouter un jeu (START + HAUT) avec délai de grâce / Combo to add a game (START + UP) with grace delay
            // Ne pas activer le combo si START a été pressé seul récemment / Don't activate combo if START was pressed alone recently
            bool canActivateCombo = true;
            if (g_LastStartPressTime.find(controllerIndex) != g_LastStartPressTime.end()) {
                ULONGLONG timeSinceStartPress = currentTime - g_LastStartPressTime[controllerIndex];
                if (timeSinceStartPress < COMBO_DELAY_MS) {
                    canActivateCombo = false;
                }
            }
            
            if (startPressed && dpadUpPressed && !dpadDownPressed && canActivateCombo) {
                if (g_AddGameComboPressTime.find(controllerIndex) == g_AddGameComboPressTime.end()) {
                    std::cout << "[Debug] Combo ADD GAME détecté sur la manette: " << controllerIndex << std::endl; // ADD GAME combo detected on controller: X
                    g_AddGameComboPressTime[controllerIndex] = currentTime;
                    g_AddGameSecondsHeld[controllerIndex] = 0;
                } else {
                    ULONGLONG timeHeld = currentTime - g_AddGameComboPressTime[controllerIndex];
                    int currentSecond = static_cast<int>(timeHeld / 1000);
                    
                    std::cout << "[Debug] Manette " << controllerIndex << " - Temps maintenu: " << timeHeld << "ms (" << currentSecond << "s)" << std::endl; // Controller X - Time held: Yms (Zs)

                    if (currentSecond >= 3 && g_AddGameSecondsHeld[controllerIndex] < 3) {
                        std::cout << "[Debug] 3 secondes atteintes, tentative d'ajout du jeu..." << std::endl; // 3 seconds reached, attempting to add game...
                        
                        // Détecter le jeu en focus et l'ajouter / Detect focused game and add it
                        std::string gameName = GetForegroundProcessName();
                        std::cout << "[Debug] Jeu en focus détecté: '" << gameName << "'" << std::endl; // Focused game detected: 'X'
                        
                        if (!gameName.empty()) {
                            std::cout << "[AddGame] Tentative d'ajout du jeu en focus: " << gameName << std::endl; // Attempting to add focused game: X
                            if (AddGameToConfig(gameName, iniPath)) {
                                std::cout << "[Action] Jeu '" << gameName << "' ajouté à la configuration!" << std::endl; // Game 'X' added to configuration!
                                std::cout << "[Action] Configuration rechargée automatiquement." << std::endl; // Configuration reloaded automatically.
                                
                                // Afficher l'overlay de confirmation / Show confirmation overlay
                                ShowInjectorOverlayMessage(StringToWString(gameName), true);
                                
                                // Vibration longue de confirmation du rechargement sur toutes les manettes / Long confirmation vibration for reload on all controllers
                                std::cout << "[XOrderInjector] Envoi de la vibration de confirmation du rechargement..." << std::endl; // Sending reload confirmation vibration...
                                ULONGLONG confirmationTime = GetTickCount64();
                                for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
                                    XINPUT_STATE state;
                                    if (XInputGetState(i, &state) == ERROR_SUCCESS) {
                                        // Vibration longue et forte pour confirmer le rechargement / Long and strong vibration to confirm reload
                                        XINPUT_VIBRATION vibration = {50000, 50000}; // Vibration forte / Strong vibration
                                        XInputSetState(i, &vibration);
                                        g_VibrationStopTime[i] = confirmationTime + 1500; // 1.5 secondes / 1.5 seconds
                                    }
                                }
                                
                                // Marquer qu'il faut recharger la configuration / Mark that configuration needs to be reloaded
                                g_NeedReloadConfig = true;
                            } else {
                                std::cout << "[AddGame] Échec de l'ajout du jeu (déjà existant ou erreur)" << std::endl; // Failed to add game (already exists or error)
                                
                                // Afficher l'overlay d'erreur / Show error overlay / Show error overlay
                                ShowInjectorOverlayMessage(StringToWString(gameName), false);
                                
                                // Vibration d'erreur / Error vibration (1 longue pulsation) / Error vibration (1 long pulse)
                                XINPUT_VIBRATION vibration = {65535, 65535};
                                XInputSetState(controllerIndex, &vibration);
                                g_VibrationStopTime[controllerIndex] = currentTime + 1000;
                                
                                // CORRECTION: Marquer que la configuration doit être rechargée même en cas d'échec / CORRECTION: Mark that configuration needs to be reloaded even on failure
                                // pour que l'overlay se ferme correctement après 2 secondes / so that the overlay closes correctly after 2 seconds / so that the overlay closes correctly after 2 seconds
                                g_NeedReloadConfig = true;
                            }
                        } else {
                            std::cout << "[AddGame] Impossible de détecter le jeu en focus" << std::endl;
                            
                            // Afficher l'overlay d'erreur
                            ShowInjectorOverlayMessage(L"Aucun jeu détecté", false);
                            
                            // Vibration d'erreur
                            XINPUT_VIBRATION vibration = {65535, 65535};
                            XInputSetState(controllerIndex, &vibration);
                            g_VibrationStopTime[controllerIndex] = currentTime + 1000;
                            
                            // CORRECTION: Marquer que la configuration doit être rechargée même en cas d'échec / CORRECTION: Mark that configuration needs to be reloaded even on failure
                            // pour que l'overlay se ferme correctement après 2 secondes
                            g_NeedReloadConfig = true;
                        }
                        
                        g_AddGameSecondsHeld[controllerIndex] = 3;
                        g_AddGameComboPressTime.erase(controllerIndex);
                    } else if (currentSecond > 0 && currentSecond < 3 && g_AddGameSecondsHeld[controllerIndex] < currentSecond) {
                        std::cout << "[Debug] Vibration de progression - seconde " << currentSecond << std::endl;
                        // Vibration de progression
                        XINPUT_VIBRATION vibration = {20000, 20000}; // Vibration légère
                        XInputSetState(controllerIndex, &vibration);
                        g_VibrationStopTime[controllerIndex] = currentTime + 100;
                        g_AddGameSecondsHeld[controllerIndex] = currentSecond;
                    }
                }
            } else {
                // Les boutons sont relâchés, on réinitialise les timers pour cette manette
                g_AddGameComboPressTime.erase(controllerIndex);
                g_AddGameSecondsHeld.erase(controllerIndex);
                
                // Nettoyer les anciens timestamps START (plus de 2 secondes)
                if (g_LastStartPressTime.find(controllerIndex) != g_LastStartPressTime.end()) {
                    ULONGLONG timeSinceStartPress = currentTime - g_LastStartPressTime[controllerIndex];
                    if (timeSinceStartPress > 2000) { // 2 secondes
                        g_LastStartPressTime.erase(controllerIndex);
                    }
                }
            }
        }
    }
    
    return g_NeedReloadConfig;
}

// Structure pour stocker les informations d'un jeu / Structure to store game information
struct GameInfo {
    std::string exeName;
    std::string forcedXInputVersion; // Version XInput forcée (vide si non spécifiée) / Forced XInput version (empty if not specified)
    bool useSysWOW64;               // Utiliser le dossier SysWOW64 / Use SysWOW64 folder

    GameInfo(const std::string& name, const std::string& version = "", bool wow64 = false)
        : exeName(name), forcedXInputVersion(version), useSysWOW64(wow64) {}

    // Vérifie si une version XInput est forcée / Check if XInput version is forced
    bool HasForcedXInput() const { return !forcedXInputVersion.empty(); }
};

// Surcharge de l'opérateur de sortie pour GameInfo / Output operator overload for GameInfo
inline std::ostream& operator<<(std::ostream& os, const GameInfo& game) {
    os << game.exeName;
    if (game.HasForcedXInput()) {
        os << " (XInput: " << game.forcedXInputVersion;
        if (game.useSysWOW64) {
            os << " [SysWOW64]";
        }
        os << ")";
    }
    return os;
}

// Lit la liste des jeux à surveiller depuis le fichier ini / Read list of games to monitor from ini file
std::vector<GameInfo> LoadWatchedGames(const std::string& iniPath) {
    std::vector<GameInfo> games;
    std::ifstream ini(iniPath);
    
    if (!ini.is_open()) {
        std::cerr << "[ERREUR] Impossible d'ouvrir le fichier de configuration: " << iniPath << std::endl; // ERROR: Unable to open configuration file
        return games;
    }
    
    std::cout << "[DEBUG] Chargement des jeux depuis: " << iniPath << std::endl; // Loading games from
    
    std::string line;
    bool inGames = false;
    int lineNum = 0;
    
    while (std::getline(ini, line)) {
        lineNum++;
        std::string trimmed = line;
        // Supprimer les espaces et tabulations / Remove spaces and tabs
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        
        if (trimmed.empty()) continue;
        
        // Vérifier les commentaires / Check comments
        if (trimmed[0] == ';' || trimmed[0] == '#') continue;
        
        // Vérifier les sections / Check sections
        if (trimmed[0] == '[') {
            inGames = (trimmed.find("[Games]") != std::string::npos);
            if (inGames) {
                std::cout << "[DEBUG] Section [Games] trouvée à la ligne " << lineNum << std::endl; // [Games] section found at line
            }
            continue;
        }
        
        if (inGames) {
            // Vérifier si une version XInput est spécifiée (format: "game.exe" "xinput1_3") / Check if XInput version is specified (format: "game.exe" "xinput1_3")
            size_t quote1 = trimmed.find('"');
            size_t quote2 = (quote1 != std::string::npos) ? trimmed.find('"', quote1 + 1) : std::string::npos;
            
            std::string exeName = trimmed;
            std::string xinputVersion;
            bool useSysWOW64 = false;
            
            if (quote1 != std::string::npos && quote2 != std::string::npos) {
                // Extraire le nom de l'exécutable (avant les guillemets) / Extract executable name (before quotes)
                exeName = trimmed.substr(0, quote1);
                // Supprimer les espaces en fin de nom de fichier / Remove trailing spaces from filename
                exeName.erase(exeName.find_last_not_of(" \t") + 1);
                
                // Extraire la version XInput (entre guillemets) / Extract XInput version (between quotes)
                xinputVersion = trimmed.substr(quote1 + 1, quote2 - quote1 - 1);
                
                // Vérifier si SysWOW64 est spécifié / Check if SysWOW64 is specified
                std::string xinputLower = xinputVersion;
                std::transform(xinputLower.begin(), xinputLower.end(), xinputLower.begin(), ::tolower);
                
                if (xinputLower.find("syswow64") != std::string::npos) {
                    useSysWOW64 = true;
                    // Extraire uniquement le nom du fichier (sans le chemin) / Extract only filename (without path)
                    size_t lastBackslash = xinputLower.find_last_of("\\/");
                    if (lastBackslash != std::string::npos) {
                        xinputVersion = xinputVersion.substr(lastBackslash + 1);
                    }
                }
                
                // Nettoyer la version (enlever l'extension .dll si présente) / Clean version (remove .dll extension if present)
                if (xinputVersion.size() > 4 && 
                    (xinputVersion.substr(xinputVersion.size() - 4) == ".dll" || 
                     xinputVersion.substr(xinputVersion.size() - 4) == ".DLL")) {
                    xinputVersion = xinputVersion.substr(0, xinputVersion.size() - 4);
                }
                
                // Vérifier que c'est une version valide / Check that it's a valid version
                static const std::vector<std::string> validVersions = {
                    "xinput1_4", "xinput1_3", "xinput1_2", "xinput1_1", "xinput9_1_0"
                };
                
                bool isValidVersion = false;
                for (const auto& ver : validVersions) {
                    if (xinputVersion == ver) {
                        isValidVersion = true;
                        break;
                    }
                }
                
                if (!isValidVersion) {
                    std::cerr << "[WARN] Version XInput invalide: " << xinputVersion 
                              << " dans la ligne " << lineNum << std::endl; // Invalid XInput version: X at line Y
                    xinputVersion.clear();
                } else {
                    std::cout << "[INFO] Version XInput forcée pour " << exeName 
                              << ": " << xinputVersion 
                              << (useSysWOW64 ? " (SysWOW64)" : " (System32)") << std::endl; // Forced XInput version for X
                }
            }
            
            // Ajouter le jeu à la liste / Add game to list
            games.emplace_back(exeName, xinputVersion, useSysWOW64);
            std::cout << "[DEBUG] Jeu ajouté: " << exeName; // Game added
            if (!xinputVersion.empty()) {
                std::cout << " (XInput: " << xinputVersion << ")";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "[DEBUG] " << games.size() << " jeux chargés depuis la configuration" << std::endl; // X games loaded from configuration
    return games;
}

// Vérifie si un processus est déjà hooké en listant ses modules / Check if process is already hooked by listing its modules
bool IsAlreadyInjected(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        return false; // Impossible de vérifier, on suppose que non / Unable to check, assume not
    }

    HMODULE hMods[1024];
    DWORD cbNeeded;
    bool injected = false;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName))) {
                if (strstr(szModName, "XOrderHook.dll") != NULL) {
                    injected = true;
                    break;
                }
            }
        }
    }

    CloseHandle(hProcess);
    return injected;
}

// Définitions nécessaires pour le shellcode / Necessary definitions for shellcode
using LoadLibraryA_t = HMODULE(WINAPI*)(LPCSTR);
using GetProcAddress_t = FARPROC(WINAPI*)(HMODULE, LPCSTR);
using DllMain_t = BOOL(WINAPI*)(HMODULE, DWORD, LPVOID);

struct LoaderData {
    LPVOID ImageBase;
    DWORD e_lfanew; // Offset vers les NT Headers / Offset to NT Headers
    LoadLibraryA_t pLoadLibraryA;
    GetProcAddress_t pGetProcAddress;
};

// Le loader qui s'exécutera dans le processus cible / The loader that will execute in the target process
DWORD WINAPI Loader(LPVOID pData) {
    LoaderData* pLoaderData = (LoaderData*)pData;
    // Calculer la bonne adresse des NT Headers dans le contexte du processus cible / Calculate correct NT Headers address in target process context
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)pLoaderData->ImageBase + pLoaderData->e_lfanew);

    // 1. Traitement des relocations / 1. Process relocations
    DWORD_PTR delta = (DWORD_PTR)pLoaderData->ImageBase - pNtHeaders->OptionalHeader.ImageBase;
    if (delta != 0) {
        if (pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0) {
            auto pRelocData = (PIMAGE_BASE_RELOCATION)((DWORD_PTR)pLoaderData->ImageBase + pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
            while (pRelocData->VirtualAddress != 0) {
                UINT amountOfEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* pRelativeInfo = (WORD*)(pRelocData + 1);
                for (UINT i = 0; i < amountOfEntries; ++i) {
                    if (pRelativeInfo[i] >> 12 == IMAGE_REL_BASED_HIGHLOW || pRelativeInfo[i] >> 12 == IMAGE_REL_BASED_DIR64) {
                        DWORD_PTR* pPatch = (DWORD_PTR*)((DWORD_PTR)pLoaderData->ImageBase + pRelocData->VirtualAddress + (pRelativeInfo[i] & 0xFFF));
                        *pPatch += delta;
                    }
                }
                pRelocData = (PIMAGE_BASE_RELOCATION)((DWORD_PTR)pRelocData + pRelocData->SizeOfBlock);
            }
        }
    }

    // 2. Résolution des imports / 2. Resolve imports
    if (pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size > 0) {
        auto pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)pLoaderData->ImageBase + pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        for (; pImportDescriptor->Name != 0; ++pImportDescriptor) {
            char* modName = (char*)((DWORD_PTR)pLoaderData->ImageBase + pImportDescriptor->Name);
            HMODULE hMod = pLoaderData->pLoadLibraryA(modName);
            if (hMod) {
                PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)pLoaderData->ImageBase + pImportDescriptor->FirstThunk);
                DWORD originalThunkRVA = pImportDescriptor->OriginalFirstThunk ? pImportDescriptor->OriginalFirstThunk : pImportDescriptor->FirstThunk;
                PIMAGE_THUNK_DATA pOriginalThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)pLoaderData->ImageBase + originalThunkRVA);
                for (; pOriginalThunk->u1.AddressOfData != 0; ++pThunk, ++pOriginalThunk) {
                    if (IMAGE_SNAP_BY_ORDINAL(pOriginalThunk->u1.Ordinal)) {
                        pThunk->u1.Function = (DWORD_PTR)pLoaderData->pGetProcAddress(hMod, (LPCSTR)IMAGE_ORDINAL(pOriginalThunk->u1.Ordinal));
                    } else {
                        auto pImport = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)pLoaderData->ImageBase + pOriginalThunk->u1.AddressOfData);
                        pThunk->u1.Function = (DWORD_PTR)pLoaderData->pGetProcAddress(hMod, pImport->Name);
                    }
                }
            }
        }
    }

    // 3. Appel de DllMain / 3. Call DllMain
    if (pNtHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
        DllMain_t pDllMain = (DllMain_t)((DWORD_PTR)pLoaderData->ImageBase + pNtHeaders->OptionalHeader.AddressOfEntryPoint);
        pDllMain((HMODULE)pLoaderData->ImageBase, DLL_PROCESS_ATTACH, NULL);
    }

    return 0;
}

// Stub pour marquer la fin de la fonction Loader / Stub to mark end of Loader function
void LoaderEnd() {}

// Fonction d'injection simple via LoadLibrary (pour cross-architecture) / Simple injection function via LoadLibrary (for cross-architecture)
bool InjectDLLSimple(DWORD pid, const std::string& dllPath) {
    std::cout << "[Simple Injector] Injection via LoadLibrary..." << std::endl;
    std::cout << "[Simple Injector] DLL: " << dllPath << std::endl;
    
    // Vérifier que la DLL existe / Check that DLL exists
    std::ifstream dllFile(dllPath);
    if (!dllFile.good()) {
        std::cerr << "[Simple Injector] Erreur: DLL non trouvée: " << dllPath << std::endl; // Error: DLL not found
        return false;
    }
    dllFile.close();
    
    // Convertir en chemin absolu si nécessaire / Convert to absolute path if necessary
    char absolutePath[MAX_PATH];
    if (!GetFullPathNameA(dllPath.c_str(), MAX_PATH, absolutePath, NULL)) {
        std::cerr << "[Simple Injector] Erreur: GetFullPathName a échoué: " << GetLastError() << std::endl; // Error: GetFullPathName failed
        return false;
    }
    std::string fullDllPath(absolutePath);
    std::cout << "[Simple Injector] Chemin absolu: " << fullDllPath << std::endl; // Absolute path
    
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "[Simple Injector] Erreur: OpenProcess a échoué: " << GetLastError() << std::endl; // Error: OpenProcess failed
        return false;
    }

    // Allouer de la mémoire pour le chemin de la DLL / Allocate memory for DLL path
    SIZE_T pathLen = fullDllPath.length() + 1;
    LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemotePath) {
        std::cerr << "[Simple Injector] Erreur: VirtualAllocEx a échoué: " << GetLastError() << std::endl; // Error: VirtualAllocEx failed
        CloseHandle(hProcess);
        return false;
    }

    // Écrire le chemin de la DLL dans le processus cible / Write DLL path to target process
    if (!WriteProcessMemory(hProcess, pRemotePath, fullDllPath.c_str(), pathLen, NULL)) {
        std::cerr << "[Simple Injector] Erreur: WriteProcessMemory a échoué: " << GetLastError() << std::endl; // Error: WriteProcessMemory failed
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Obtenir l'adresse de LoadLibraryA / Get LoadLibraryA address
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        std::cerr << "[Simple Injector] Erreur: GetModuleHandle(kernel32) a échoué" << std::endl; // Error: GetModuleHandle(kernel32) failed
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPVOID pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibrary) {
        std::cerr << "[Simple Injector] Erreur: GetProcAddress(LoadLibraryA) a échoué" << std::endl; // Error: GetProcAddress(LoadLibraryA) failed
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    std::cout << "[Simple Injector] Création du thread distant..." << std::endl; // Creating remote thread...

    // Créer un thread distant pour charger la DLL / Create remote thread to load DLL
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, 
        (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemotePath, 0, NULL);
    if (!hThread) {
        std::cerr << "[Simple Injector] Erreur: CreateRemoteThread a échoué: " << GetLastError() << std::endl; // Error: CreateRemoteThread failed
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Attendre que le thread se termine / Wait for thread to finish
    std::cout << "[Simple Injector] Attente de la fin du thread..." << std::endl; // Waiting for thread to finish...
    DWORD waitResult = WaitForSingleObject(hThread, 15000); // 15 secondes max / 15 seconds max
    if (waitResult != WAIT_OBJECT_0) {
        std::cerr << "[Simple Injector] Avertissement: Le thread d'injection n'a pas terminé dans les temps (résultat: " << waitResult << ")" << std::endl; // Warning: Injection thread didn't finish in time
    }

    // Vérifier le code de retour du thread / Check thread return code
    DWORD threadExitCode = 0;
    if (GetExitCodeThread(hThread, &threadExitCode)) {
        std::cout << "[Simple Injector] Code de retour du thread: 0x" << std::hex << threadExitCode << std::dec << std::endl; // Thread return code
        if (threadExitCode == 0) {
            std::cerr << "[Simple Injector] Erreur: LoadLibrary a retourné NULL - DLL non chargée" << std::endl; // Error: LoadLibrary returned NULL - DLL not loaded
            
            // Essayer d'obtenir plus d'informations sur l'erreur / Try to get more error information
            DWORD lastError = GetLastError();
            std::cerr << "[Simple Injector] GetLastError: " << lastError << std::endl;
            
            CloseHandle(hThread);
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
    } else {
        std::cerr << "[Simple Injector] Erreur: GetExitCodeThread a échoué: " << GetLastError() << std::endl; // Error: GetExitCodeThread failed
    }

    std::cout << "[Simple Injector] Injection terminée avec succès (HMODULE: 0x" << std::hex << threadExitCode << std::dec << ")" << std::endl; // Injection completed successfully

    // Nettoyage / Cleanup
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return true;
}

// Fonction d'injection avec processus auxiliaire (pour cross-architecture) / Injection function with helper process (for cross-architecture)
bool InjectDLLWithHelper(DWORD pid, const std::string& dllPath, ProcessArchitecture targetArch) {
    std::cout << "[Helper Injector] Injection via processus auxiliaire..." << std::endl; // Injection via helper process...
    
    // Obtenir le chemin de l'exécutable actuel / Get current executable path
    char currentPath[MAX_PATH];
    GetModuleFileNameA(NULL, currentPath, MAX_PATH);
    std::string currentDir = currentPath;
    currentDir = currentDir.substr(0, currentDir.find_last_of("\\/"));
    
    // Chercher XOrderInjector_x86.exe / Look for XOrderInjector_x86.exe
    std::string helperPath = currentDir + "\\XOrderInjector_x86.exe";
    
    // Vérifier que le fichier existe / Check that file exists
    std::ifstream test(helperPath);
    if (!test.good()) {
        std::cout << "[Helper Injector] XOrderInjector_x86.exe non trouvé dans " << currentDir << std::endl; // XOrderInjector_x86.exe not found in
        std::cout << "[Helper Injector] Fallback vers l'injection directe..." << std::endl; // Fallback to direct injection...
        return InjectDLLSimple(pid, dllPath);
    }
    test.close();
    
    // Créer un fichier temporaire pour la communication / Create temporary file for communication
    std::string tempFile = currentDir + "\\inject_result_" + std::to_string(pid) + ".tmp";
    
    // Construire la commande pour le processus auxiliaire / Build command for helper process
    std::string command = "\"" + helperPath + "\" --inject " + std::to_string(pid) + " \"" + dllPath + "\" \"" + tempFile + "\"";
    
    std::cout << "[Helper Injector] Lancement: " << command << std::endl; // Launching
    
    // Lancer le processus auxiliaire / Launch helper process
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        // Attendre que le processus se termine (max 15 secondes) / Wait for process to finish (max 15 seconds)
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 15000);
        
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        std::cout << "[Helper Injector] Processus auxiliaire terminé (code: " << exitCode << ")" << std::endl; // Helper process finished (code: X)
        
        if (waitResult == WAIT_OBJECT_0 && exitCode == 0) {
            // Lire le résultat du fichier temporaire / Read result from temporary file
            std::ifstream resultFile(tempFile);
            if (resultFile.is_open()) {
                std::string result;
                std::getline(resultFile, result);
                resultFile.close();
                DeleteFileA(tempFile.c_str()); // Nettoyer le fichier temporaire / Clean up temporary file
                
                if (result == "SUCCESS") {
                    std::cout << "[Helper Injector] [+] Injection reussie via processus auxiliaire x86" << std::endl; // Injection successful via x86 helper process
                    return true;
                } else {
                    std::cout << "[Helper Injector] [-] Echec rapporte par le processus auxiliaire: " << result << std::endl; // Failure reported by helper process
                }
            } else {
                std::cout << "[Helper Injector] [-] Impossible de lire le fichier de resultat" << std::endl; // Unable to read result file
            }
        } else {
            std::cout << "[Helper Injector] [-] Le processus auxiliaire a echoue ou timeout" << std::endl; // Helper process failed or timed out
        }
    } else {
        std::cout << "[Helper Injector] [-] Impossible de lancer le processus auxiliaire: " << GetLastError() << std::endl; // Unable to launch helper process
    }
    
    // Si le processus auxiliaire échoue, essayer l'injection directe en dernier recours / If helper process fails, try direct injection as last resort
    std::cout << "[Helper Injector] Tentative d'injection directe en dernier recours..." << std::endl; // Attempting direct injection as last resort...
    return InjectDLLSimple(pid, dllPath);
}

bool InjectDLL(DWORD pid, const std::string& baseDllPath, ProcessArchitecture targetArch) {
    // Sélectionner la DLL appropriée selon l'architecture / Select the appropriate DLL according to architecture
    std::string dllPath = GetArchSpecificDllPath(baseDllPath, targetArch);
    std::cout << "[Shellcode Injector] DLL sélectionnée pour architecture " << ArchitectureToString(targetArch) 
              << ": " << dllPath << std::endl;
    
    // --- ÉTAPE 1: Lire la DLL en mémoire / --- STEP 1: Read the DLL into memory ---
    std::cout << "[Shellcode Injector] 1. Lecture du fichier DLL..." << std::endl;
    std::ifstream dllFile(dllPath, std::ios::binary | std::ios::ate);
    if (!dllFile.is_open()) { 
        std::cerr << "Erreur: Impossible d'ouvrir le fichier DLL: " << dllPath << std::endl; 
        return false; 
    }
    std::streamsize size = dllFile.tellg();
    dllFile.seekg(0, std::ios::beg);
    char* pDllData = new char[size];
    dllFile.read(pDllData, size);
    dllFile.close();

    // --- ÉTAPE 2: Parser les headers PE / --- STEP 2: Parse the PE headers ---
    auto pDosHeader = (PIMAGE_DOS_HEADER)pDllData;
    auto pNtHeaders = (PIMAGE_NT_HEADERS)(pDllData + pDosHeader->e_lfanew);

    // --- ÉTAPE 3: Ouvrir le processus cible / --- STEP 3: Open the target process ---
    std::cout << "[Shellcode Injector] 2. Ouverture du processus cible..." << std::endl;
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) { std::cerr << "Erreur: OpenProcess a échoué." << std::endl; delete[] pDllData; return false; }

    // --- ÉTAPE 4: Allouer la mémoire pour la DLL dans le processus cible / --- STEP 4: Allocate memory for the DLL in the target process ---
    std::cout << "[Shellcode Injector] 3. Allocation de la mémoire pour la DLL..." << std::endl;
    LPVOID pTargetBase = VirtualAllocEx(hProcess, nullptr, pNtHeaders->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pTargetBase) { std::cerr << "Erreur: VirtualAllocEx pour la DLL a échoué." << std::endl; CloseHandle(hProcess); delete[] pDllData; return false; }

    // --- ÉTAPE 5: Copier les sections de la DLL / --- STEP 5: Copy the DLL sections ---
    std::cout << "[Shellcode Injector] 4. Copie des sections de la DLL..." << std::endl;
    WriteProcessMemory(hProcess, pTargetBase, pDllData, pNtHeaders->OptionalHeader.SizeOfHeaders, nullptr);
    auto pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    for (UINT i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i, ++pSectionHeader) {
        WriteProcessMemory(hProcess, (LPVOID)((DWORD_PTR)pTargetBase + pSectionHeader->VirtualAddress), (LPVOID)(pDllData + pSectionHeader->PointerToRawData), pSectionHeader->SizeOfRawData, nullptr);
    }

    // --- ÉTAPE 6: Préparer et injecter le loader et ses données / --- STEP 6: Prepare and inject the loader and its data ---
    std::cout << "[Shellcode Injector] 5. Préparation et injection du loader..." << std::endl;
    LoaderData loaderData;
    loaderData.ImageBase = pTargetBase;
    loaderData.e_lfanew = pDosHeader->e_lfanew; // On passe l'offset, pas le pointeur
    loaderData.pLoadLibraryA = LoadLibraryA;
    loaderData.pGetProcAddress = GetProcAddress;

    LPVOID pLoaderDataRemote = VirtualAllocEx(hProcess, nullptr, sizeof(LoaderData), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pLoaderDataRemote) { std::cerr << "Erreur: VirtualAllocEx pour LoaderData a échoué." << std::endl; VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE); CloseHandle(hProcess); delete[] pDllData; return false; }
    WriteProcessMemory(hProcess, pLoaderDataRemote, &loaderData, sizeof(LoaderData), nullptr);

    DWORD loaderSize = (DWORD_PTR)LoaderEnd - (DWORD_PTR)Loader;
    LPVOID pLoaderRemote = VirtualAllocEx(hProcess, nullptr, loaderSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pLoaderRemote) { std::cerr << "Erreur: VirtualAllocEx pour le Loader a échoué." << std::endl; VirtualFreeEx(hProcess, pLoaderDataRemote, 0, MEM_RELEASE); VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE); CloseHandle(hProcess); delete[] pDllData; return false; }
    WriteProcessMemory(hProcess, pLoaderRemote, Loader, loaderSize, nullptr);

    // --- ÉTAPE 7: Créer le thread distant pour exécuter le loader / --- STEP 7: Create the remote thread to execute the loader ---
    std::cout << "[Shellcode Injector] 6. Exécution du lopuisader..." << std::endl;
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoaderRemote, pLoaderDataRemote, 0, NULL);
    if (!hThread) { std::cerr << "Erreur: CreateRemoteThread a échoué." << std::endl; /* ... cleanup ... */ return false; }
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    std::cout << "[Shellcode Injector] Injection terminée." << std::endl;
    CloseHandle(hProcess);
    delete[] pDllData;
    return true;
}

// Fonction utilitaire pour vérifier si un fichier existe / Utility function to check if file exists
bool FileExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

// Fonction pour obtenir le chemin complet d'une version XInput / Function to get full path of XInput version
std::wstring GetXInputPath(const std::string& version, bool useSysWOW64) {
    wchar_t windowsDir[MAX_PATH];
    if (GetWindowsDirectoryW(windowsDir, MAX_PATH) == 0) {
        return L"";
    }
    
    std::wstring path = windowsDir;
    path += useSysWOW64 ? L"\\SysWOW64\\" : L"\\System32\\";
    
    // Ajouter le préfixe "xinput" si nécessaire / Add "xinput" prefix if necessary
    std::wstring dllName = StringToWString(version);
    if (dllName.find(L"xinput") == std::wstring::npos) {
        dllName = L"xinput" + dllName;
    }
    
    // Ajouter l'extension .dll si nécessaire / Add .dll extension if necessary
    if (dllName.size() < 4 || 
        (dllName.substr(dllName.size() - 4) != L".dll" && 
         dllName.substr(dllName.size() - 4) != L".DLL")) {
        dllName += L".dll";
    }
    
    return path + dllName;
}

// Fonction pour scanner les dossiers système et trouver la version XInput la plus élevée disponible / Function to scan system folders and find highest available XInput version
std::string FindHighestAvailableXInputVersion(ProcessArchitecture targetArch = ProcessArchitecture::Unknown) {
    // Liste des versions XInput dans l'ordre de préférence (du plus récent au plus ancien) / List of XInput versions in preference order (newest to oldest)
    const std::vector<std::string> xinputDlls = {
        "xinput1_4.dll",   // Windows 10
        "xinput1_3.dll",   // Windows 8
        "xinput1_2.dll",   // Windows 7
        "xinput1_1.dll",   // Windows Vista
        "xinput9_1_0.dll"  // Windows XP
    };

    // Liste des dossiers système à scanner selon l'architecture / List of system folders to scan according to architecture
    std::vector<std::wstring> systemDirs;
    
    // Récupérer le dossier système Windows / Get Windows system folder
    wchar_t winDir[MAX_PATH];
    if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
        std::wstring winDirStr(winDir);
        
        if (targetArch == ProcessArchitecture::x86) {
            // Pour les processus x86, prioriser SysWOW64 puis System32 / For x86 processes, prioritize SysWOW64 then System32
            systemDirs.push_back(winDirStr + L"\\SysWOW64\\");
            systemDirs.push_back(winDirStr + L"\\System32\\");
        } else if (targetArch == ProcessArchitecture::x64) {
            // Pour les processus x64, utiliser uniquement System32 / For x64 processes, use only System32
            systemDirs.push_back(winDirStr + L"\\System32\\");
        } else {
            // Architecture inconnue, scanner les deux / Unknown architecture, scan both
            systemDirs.push_back(winDirStr + L"\\System32\\");
            systemDirs.push_back(winDirStr + L"\\SysWOW64\\");
        }
    }
    
    // Ajouter le dossier de l'application actuelle / Add current application folder
    wchar_t appDir[MAX_PATH];
    if (GetModuleFileNameW(NULL, appDir, MAX_PATH) > 0) {
        std::wstring appPath(appDir);
        size_t lastSlash = appPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            systemDirs.push_back(appPath.substr(0, lastSlash + 1));
        }
    }
    
    // Parcourir toutes les versions de XInput du plus récent au plus ancien / Go through all XInput versions from newest to oldest
    for (const auto& xinputDll : xinputDlls) {
        std::wstring xinputDllW(xinputDll.begin(), xinputDll.end());
        
        // Vérifier dans chaque dossier système / Check in each system folder
        for (const auto& dir : systemDirs) {
            std::wstring fullPath = dir + xinputDllW;
            if (FileExists(fullPath)) {
                std::wcout << L"[XInputDetect] Version XInput disponible pour " 
                          << StringToWString(ArchitectureToString(targetArch)) 
                          << L": " << fullPath << std::endl; // XInput version available for X
                return xinputDll;  // Retourne la première (donc la plus récente) version trouvée / Return first (therefore newest) version found
            }
        }
    }
    
    // Si aucune version n'est trouvée, retourner la version par défaut / If no version found, return default version
    return "xinput1_3.dll";
}

// Fonction pour détecter la version XInput chargée par un processus / Function to detect XInput version loaded by a process
std::string DetectXInputVersion(DWORD pid, ProcessArchitecture processArch) {
    std::cout << "[XInputDetect] Détection XInput pour processus " << pid << " (arch: " << ArchitectureToString(processArch) << ")" << std::endl; // XInput detection for process X (arch: Y)
    
    // Pour les processus x86, utiliser une approche différente car EnumProcessModulesEx depuis x64 n'est pas fiable / For x86 processes, use different approach as EnumProcessModulesEx from x64 is unreliable
    if (processArch == ProcessArchitecture::x86) {
        std::cout << "[XInputDetect] Processus x86 détecté, utilisation de la détection spécialisée..." << std::endl; // x86 process detected, using specialized detection...
        
        // Pour les jeux x86, ils utilisent généralement SysWOW64 / For x86 games, they generally use SysWOW64
        // Vérifier d'abord les versions dans SysWOW64 / Check versions in SysWOW64 first
        wchar_t winDir[MAX_PATH];
        if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
            std::wstring sysWow64Dir = std::wstring(winDir) + L"\\SysWOW64\\";
            
            // Liste des versions XInput dans l'ordre de préférence pour x86 / List of XInput versions in preference order for x86
            const std::vector<std::string> xinputDlls = {
                "xinput1_3.dll",   // Préféré pour x86 / Preferred for x86
                "xinput1_4.dll",   // Windows 10
                "xinput1_2.dll",   // Windows 7
                "xinput1_1.dll",   // Windows Vista
                "xinput9_1_0.dll"  // Windows XP
            };
            
            for (const auto& xinputDll : xinputDlls) {
                std::wstring xinputPath = sysWow64Dir + StringToWString(xinputDll);
                if (FileExists(xinputPath)) {
                    std::wcout << L"[XInputDetect] Version XInput x86 trouvée: " << xinputPath << std::endl; // x86 XInput version found
                    return xinputDll;
                }
            }
        }
        
        // Si rien trouvé dans SysWOW64, essayer System32 (moins probable pour x86) / If nothing found in SysWOW64, try System32 (less likely for x86)
        std::cout << "[XInputDetect] Aucune version trouvée dans SysWOW64, essai System32..." << std::endl; // No version found in SysWOW64, trying System32...
        return FindHighestAvailableXInputVersion(processArch);
    }
    
    // Pour les processus x64, utiliser la détection normale / For x64 processes, use normal detection
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        HMODULE hMods[1024];
        DWORD cbNeeded;
        
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded) && cbNeeded > 0) {
            std::cout << "[XInputDetect] " << (cbNeeded / sizeof(HMODULE)) << " modules trouvés pour processus x64" << std::endl; // X modules found for x64 process
            
            // Liste des versions XInput dans l'ordre de préférence / List of XInput versions in preference order
            const std::vector<std::string> xinputDlls = {
                "xinput1_4.dll", "xinput1_3.dll", "xinput1_2.dll", 
                "xinput1_1.dll", "xinput9_1_0.dll"
            };
            
            std::string latestDetectedVersion;
            
            // Parcourir tous les modules chargés / Go through all loaded modules
            for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
                wchar_t szModName[MAX_PATH];
                if (GetModuleFileNameExW(hProcess, hMods[i], szModName, MAX_PATH)) {
                    std::wstring moduleName(szModName);
                    std::transform(moduleName.begin(), moduleName.end(), moduleName.begin(), ::towlower);
                    
                    // Vérifier contre toutes les versions XInput connues / Check against all known XInput versions
                    for (const auto& xinputDll : xinputDlls) {
                        std::wstring xinputDllW(xinputDll.begin(), xinputDll.end());
                        if (moduleName.find(xinputDllW) != std::wstring::npos) {
                            if (latestDetectedVersion.empty() || 
                                (std::find(xinputDlls.begin(), xinputDlls.end(), xinputDll) < 
                                 std::find(xinputDlls.begin(), xinputDlls.end(), latestDetectedVersion))) {
                                latestDetectedVersion = xinputDll;
                                std::wcout << L"[XInputDetect] Version XInput chargée détectée: " << moduleName << std::endl; // Loaded XInput version detected
                                break;
                            }
                        }
                    }
                }
            }
            
            CloseHandle(hProcess);
            
            if (!latestDetectedVersion.empty()) {
                return latestDetectedVersion;
            }
        }
        
        CloseHandle(hProcess);
    }
    
    // Fallback vers la détection système / Fallback to system detection
    std::cout << "[XInputDetect] Utilisation de la détection système pour " << ArchitectureToString(processArch) << std::endl; // Using system detection for X
    return FindHighestAvailableXInputVersion(processArch);
}

// Fonction pour obtenir le chemin complet d'un processus / Function to get full path of a process
std::string GetProcessFullPath(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        return "";
    }
    char processPath[MAX_PATH] = {0};
    if (GetModuleFileNameExA(hProcess, NULL, processPath, MAX_PATH) == 0) {
        CloseHandle(hProcess);
        return "";
    }
    CloseHandle(hProcess);
    return std::string(processPath);
}

// Fonction utilitaire pour convertir une chaîne en wstring / Utility function to convert string to wstring
std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Fonction utilitaire pour convertir une wstring en chaîne / Utility function to convert wstring to string
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Fonction utilitaire pour créer un mutex global / Utility function to create global mutex
HANDLE CreateGlobalMutex() {
    // Créer un mutex avec un nom global / Create mutex with global name
    HANDLE hMutex = CreateMutexA(NULL, FALSE, "Global\\XOrderInjector_Mutex");
    if (hMutex == NULL) {
        std::cerr << "[XOrderInjector] Erreur lors de la création du mutex: " << GetLastError() << std::endl; // Error creating mutex
    }
    return hMutex;
}

// Vérifier si une autre instance est déjà en cours d'exécution / Check if another instance is already running
bool IsAnotherInstanceRunning() {
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "Global\\XOrderInjector_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return true;
    }
    // Le mutex sera libéré à la fermeture du programme / Mutex will be released when program closes
    return false;
}

int main(int argc, char* argv[]) {
    // Vérifier les arguments de ligne de commande pour les modes spéciaux / Check command line arguments for special modes
    if (argc >= 3 && strcmp(argv[1], "--inject") == 0) {
        // Mode injection directe (appelé par le processus auxiliaire) / Direct injection mode (called by helper process)
        DWORD pid = atoi(argv[2]);
        std::string dllPath = (argc >= 4) ? argv[3] : "";
        std::string resultFile = (argc >= 5) ? argv[4] : "";
        
        if (pid > 0 && !dllPath.empty()) {
            bool success = InjectDLLSimple(pid, dllPath);
            
            // Écrire le résultat dans le fichier temporaire / Write result to temporary file
            if (!resultFile.empty()) {
                std::ofstream result(resultFile);
                if (result.is_open()) {
                    result << (success ? "SUCCESS" : "FAILED");
                    result.close();
                }
            }
            
            return success ? 0 : 1;
        }
        return 1;
    }
    
    // Vérifier si une autre instance est en cours d'exécution / Check if another instance is running
    bool isFirstInstance = !IsAnotherInstanceRunning();
    
    // Si ce n'est pas la première instance, attendre 15 secondes que l'autre instance se termine / If not first instance, wait 15 seconds for other instance to finish
    if (!isFirstInstance) {
        std::cout << "[XOrderInjector] Une autre instance est déjà en cours d'exécution. Attente de 15 secondes..." << std::endl; // Another instance is already running. Waiting 15 seconds...
        
        const int WAIT_TIME_MS = 15000; // 15 secondes / 15 seconds
        const int CHECK_INTERVAL = 100;  // Vérifier toutes les 100ms / Check every 100ms
        int timeWaited = 0;
        
        while (timeWaited < WAIT_TIME_MS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL));
            timeWaited += CHECK_INTERVAL;
            
            // Vérifier si l'autre instance s'est terminée / Check if other instance has finished
            if (!IsAnotherInstanceRunning()) {
                isFirstInstance = true;
                std::cout << "[XOrderInjector] L'instance précédente s'est terminée. Démarrage..." << std::endl; // Previous instance finished. Starting...
                break;
            }
        }
        
        // Si après 15 secondes l'autre instance est toujours en cours, on quitte / If after 15 seconds other instance is still running, exit
        if (!isFirstInstance) {
            std::cerr << "[XOrderInjector] Une autre instance est toujours en cours après 15 secondes. Fermeture." << std::endl; // Another instance still running after 15 seconds. Closing.
            return 1;
        }
    }

    // Détecter la langue du système / Detect system language
    g_UseFrenchLanguage = DetectFrenchSystem();
    
    // Définir l'encodage de la console en UTF-8 / Set console encoding to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Activer le mode UTF-8 pour les flux de sortie standard / Enable UTF-8 mode for standard output streams
    std::wcout.imbue(std::locale(""));
    std::wcerr.imbue(std::locale(""));
    std::wcin.imbue(std::locale(""));
    
    if (!EnableDebugPrivilege()) {
        std::cout << GetLocalizedMessage("[XOrderInjector] AVERTISSEMENT: Impossible d'activer SeDebugPrivilege. L'injection échouera probablement sur les processus protégés.", "[XOrderInjector] WARNING: Unable to enable SeDebugPrivilege. Injection will likely fail on protected processes.") << std::endl;
        std::cout << GetLocalizedMessage("[XOrderInjector] Assurez-vous de lancer ce programme en tant qu'administrateur.", "[XOrderInjector] Make sure to run this program as administrator.") << std::endl;
    }

    // Initialiser l'overlay de l'injecteur / Initialize injector overlay
    InitializeInjectorOverlay();

    // Obtenir le chemin du répertoire de l'exécutable / Get executable directory path
    char modulePath[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    std::string baseDir = modulePath;
    baseDir = baseDir.substr(0, baseDir.find_last_of("\\/"));
    std::string iniPath = baseDir + "\\XOrderConfig.ini";
    std::string dllPath = baseDir + "\\XOrderHook.dll";

    // Lire la configuration / Read configuration
    char settingsValue[256] = {0};
    if (GetPrivateProfileStringA("Settings", nullptr, "", settingsValue, sizeof(settingsValue), iniPath.c_str()) > 0) {
        // Lire le paramètre EnableDebugConsole (0=désactivé par défaut) / Read EnableDebugConsole parameter (0=disabled by default)
        g_enableDebugConsole = (GetPrivateProfileIntA("Settings", "EnableDebugConsole", 0, iniPath.c_str()) != 0);
        
        if (g_enableDebugConsole) {
            std::cout << "[DEBUG] Console de débogage activée" << std::endl; // Debug console enabled
        }
    }

    std::cout << GetLocalizedMessage("[XOrderInjector] DLL à injecter : ", "[XOrderInjector] DLL to inject: ") << dllPath << std::endl;

    // Charger la liste des jeux à surveiller / Load list of games to monitor
    auto games = LoadWatchedGames(iniPath);
    if (games.empty()) {
        std::cout << GetLocalizedMessage("[ERREUR] Aucun jeu à surveiller trouvé dans XOrderConfig.ini", "[ERROR] No games to monitor found in XOrderConfig.ini") << std::endl;
        std::cout << GetLocalizedMessage("\nAssurez-vous que le fichier ", "\nMake sure the file ") << iniPath << GetLocalizedMessage(" contient une section [Games]", " contains a [Games] section") << std::endl;
        std::cout << GetLocalizedMessage("avec la liste des exécutables à surveiller, par exemple:\n", "with list of executables to monitor, for example:\n") << std::endl;
        std::cout << "[Games]" << std::endl;
        std::cout << "  game.exe" << std::endl;
        std::cout << "  another_game.exe\n" << std::endl;
        std::cout << GetLocalizedMessage("Les noms sont insensibles à la casse et l'extension .exe est optionnelle.", "Names are case-insensitive and .exe extension is optional.") << std::endl;
        std::cout << GetLocalizedMessage("\nVous pouvez ajouter des jeux en utilisant START+HAUT sur votre manette.", "\nYou can add games using START+UP on your controller.") << std::endl;
    } else {
        std::cout << "Surveillance des jeux :" << std::endl; // Game monitoring:
        for (auto& g : games) {
            std::cout << "  - " << g.exeName;
            if (g.HasForcedXInput()) {
                std::cout << " (XInput: " << g.forcedXInputVersion;
                if (g.useSysWOW64) {
                    std::cout << " [SysWOW64]";
                }
                std::cout << ")";
            }
            std::cout << std::endl;
        }
    }

    std::vector<DWORD> injected_pids;
    std::vector<DWORD> failed_pids;
    
    // Constantes de temporisation / Timing constants
    const int INITIAL_DELAY_MS = 5000;           // Délai initial avant de commencer à scanner / Initial delay before starting to scan
    const int STARTUP_TIMEOUT_MS = 90000;        // 90s max pour trouver un premier jeu / 90s max to find first game
    const int SHUTDOWN_DELAY_MS = 10000;         // 10s avant la fermeture après la fin du dernier jeu / 10s before closing after last game ends
    
    // Variables d'état / State variables
    auto startTime = std::chrono::steady_clock::now();
    auto lastGameDetectedTime = startTime;
    bool initialDelayPassed = false;
    bool startupTimeoutPassed = false;
    bool hasGameBeenAttached = false;
    bool shutdownRequested = false;
    auto shutdownTime = std::chrono::steady_clock::time_point();

    while (true) {
        auto currentTime = std::chrono::steady_clock::now();
        
        // Vérifier les combos de manettes (toujours actif) / Check controller combos (always active)
        if (CheckControllerCombos(iniPath)) {
            std::cout << "[XOrderInjector] Nouveau jeu ajouté, rechargement de la configuration..." << std::endl; // New game added, reloading configuration...
            
            // Attendre 2 secondes pour que l'utilisateur puisse lire le message / Wait 2 seconds for user to read message
            std::cout << "[XOrderInjector] Attente de 2 secondes pour affichage du message..." << std::endl; // Waiting 2 seconds for message display...
            Sleep(2000);
            
            // Fermer l'overlay après le délai / Close overlay after delay
            ForceHideInjectorOverlay();
            
            // Recharger la liste des jeux / Reload game list
            games = LoadWatchedGames(iniPath);
            std::cout << "[XOrderInjector] Configuration rechargée. Nouveaux jeux surveillés :" << std::endl; // Configuration reloaded. New monitored games:
            for (auto& g : games) {
                std::cout << "  - " << g.exeName;
                if (g.HasForcedXInput()) {
                    std::cout << " (XInput: " << g.forcedXInputVersion;
                    if (g.useSysWOW64) {
                        std::cout << " [SysWOW64]";
                    }
                    std::cout << ")";
                }
                std::cout << std::endl;
            }
            
            // Réinitialiser le flag / Reset flag
            g_NeedReloadConfig = false;
        }
        
        // Vérifier si le délai initial est écoulé / Check if initial delay has elapsed
        if (!initialDelayPassed) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime).count();
                
            if (elapsed < INITIAL_DELAY_MS) {
                // Période d'attente initiale / Initial waiting period
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (!initialDelayPassed) {
                initialDelayPassed = true;
                std::cout << "[XOrderInjector] Délai initial de 5 secondes écoulé, début du scan des processus..." << std::endl; // Initial 5-second delay elapsed, starting process scan...
                std::cout << "[XOrderInjector] Détection des combos de manettes activée (START+HAUT pour ajouter un jeu)" << std::endl; // Controller combo detection enabled (START+UP to add game)
                startTime = std::chrono::steady_clock::now(); // Réinitialiser pour le timeout de démarrage / Reset for startup timeout
            }
        }
        
        // Vérifier le timeout de démarrage (90s pour trouver un premier jeu) / Check startup timeout (90s to find first game)
        if (!hasGameBeenAttached && initialDelayPassed) {
            auto elapsedSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime).count();
                
            if (elapsedSinceStart >= STARTUP_TIMEOUT_MS) {
                std::cout << "[XOrderInjector] Aucun jeu trouvé après " << (STARTUP_TIMEOUT_MS/1000) 
                          << " secondes. Fermeture..." << std::endl; // No game found after X seconds. Closing...
                break;
            }
        }
        
        // Vérifier si on doit fermer l'application (uniquement si un jeu a déjà été attaché) / Check if application should close (only if game has been attached)
        if (shutdownRequested && hasGameBeenAttached) {
            auto elapsedShutdown = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - shutdownTime).count();
                
            if (elapsedShutdown >= SHUTDOWN_DELAY_MS) {
                std::cout << "[XOrderInjector] Fermeture automatique après inactivité..." << std::endl; // Automatic shutdown after inactivity...
                break;
            }
        }
        // Vérifier les processus terminés / Check terminated processes
        auto it = injected_pids.begin();
        while (it != injected_pids.end()) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, *it);
            if (hProcess) {
                DWORD exitCode;
                if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                    std::cout << "[XOrderInjector] Processus terminé détecté (PID: " << *it << ")" << std::endl; // Terminated process detected (PID: X)
                    it = injected_pids.erase(it);
                    
                    // Si c'était le dernier processus, démarrer le compte à rebours de fermeture / If it was the last process, start shutdown countdown
                    if (injected_pids.empty()) {
                        shutdownRequested = true;
                        shutdownTime = std::chrono::steady_clock::now();
                        std::cout << "[XOrderInjector] Dernier jeu détaché. Fermeture dans " 
                                 << (SHUTDOWN_DELAY_MS/1000) << " secondes..." << std::endl; // Last game detached. Closing in X seconds...
                    }
                } else {
                    ++it;
                }
                CloseHandle(hProcess);
            } else {
                // Le processus n'existe plus ou erreur d'accès / Process no longer exists or access error
                std::cout << "[XOrderInjector] Processus terminé ou erreur d'accès (PID: " << *it << ")" << std::endl; // Process terminated or access error (PID: X)
                it = injected_pids.erase(it);
            }
        }

        // Nettoyer les échecs pour les processus qui n'existent plus / Clean up failures for processes that no longer exist
        auto failed_it = failed_pids.begin();
        while (failed_it != failed_pids.end()) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, *failed_it);
            if (!hProcess) {
                failed_it = failed_pids.erase(failed_it);
            } else {
                CloseHandle(hProcess);
                ++failed_it;
            }
        }
        // Vérifier si des processus injectés sont toujours actifs / Check if injected processes are still active
        bool hasActiveProcesses = !injected_pids.empty();

        // Scanner les nouveaux processus / Scan new processes
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(PROCESSENTRY32W);
            
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    // Convertir le nom du processus depuis UTF-16 (wchar_t) vers UTF-8 / Convert process name from UTF-16 (wchar_t) to UTF-8
                    std::wstring wExe(pe.szExeFile);
                    std::string exe = WStringToString(wExe);
                    
                    // Vérifier si le processus est déjà injecté ou en échec / Check if process is already injected or failed
                    bool alreadyInjected = (std::find(injected_pids.begin(), injected_pids.end(), pe.th32ProcessID) != injected_pids.end());
                    bool alreadyFailed = (std::find(failed_pids.begin(), failed_pids.end(), pe.th32ProcessID) != failed_pids.end());
                    
                    if (alreadyInjected || alreadyFailed) {
                        continue;
                    }
                
                    // Convertir le nom du processus en minuscules UTF-8 pour la comparaison / Convert process name to lowercase UTF-8 for comparison
                    std::string cleanExe = CleanExeName(exe);
                    
                    // Log pour le débogage si activé / Debug log if enabled
                    if (g_enableDebugConsole) {
                        std::wcout << L"[DEBUG] Processus trouvé: " << wExe << L" (nettoyé: " << StringToWString(cleanExe) << L")" << std::endl; // Process found: X (cleaned: Y)
                    }
                    
                    // Vérifier si c'est un processus à surveiller / Check if it's a process to monitor
                    const GameInfo* targetGame = nullptr;
                    for (const auto& game : games) {
                        // Nettoyer le nom du jeu de la configuration / Clean game name from configuration
                        std::string cleanGame = CleanExeName(game.exeName);
                        
                        // Vérifier si le nom du jeu correspond au nom du processus (avec ou sans .exe) / Check if game name matches process name (with or without .exe)
                        bool match = (cleanExe == cleanGame || 
                                     cleanExe + ".exe" == cleanGame ||
                                     cleanExe == cleanGame + ".exe");
                        
                        if (match) {
                            targetGame = &game;
                            if (g_enableDebugConsole) {
                                std::wcout << L"[DEBUG] Correspondance trouvée pour: " << StringToWString(cleanExe) 
                                         << L" (recherché: " << StringToWString(cleanGame) << L")" << std::endl; // Match found for: X (searched: Y)
                            }
                            break;
                        }
                    }
                    
                    if (targetGame) {
                        hasGameBeenAttached = true; // Un jeu a été trouvé et sera traité / A game has been found and will be processed
                        lastGameDetectedTime = currentTime;
                        
                        if (!IsAlreadyInjected(pe.th32ProcessID)) {
                            // Détecter l'architecture du processus cible / Detect target process architecture
                            ProcessArchitecture processArch = GetProcessArchitecture(pe.th32ProcessID);
                            std::wcout << L"[INFO] Architecture détectée pour " << wExe 
                                     << L": " << StringToWString(ArchitectureToString(processArch)) << std::endl; // Architecture detected for X: Y
                            
                            if (processArch == ProcessArchitecture::Unknown) {
                                std::wcerr << L"[ERREUR] Impossible de détecter l'architecture du processus " 
                                          << wExe << L". Injection annulée." << std::endl; // ERROR: Unable to detect process architecture X. Injection cancelled.
                                failed_pids.push_back(pe.th32ProcessID);
                                continue;
                            }
                            
                            // Détecter la version XInput utilisée par le jeu / Detect XInput version used by game
                            std::string xinputVersion;
                            bool useSysWOW64 = targetGame->useSysWOW64;
                            
                            // Vérifier si une version XInput est forcée pour ce jeu / Check if XInput version is forced for this game
                            if (targetGame->HasForcedXInput()) {
                                xinputVersion = targetGame->forcedXInputVersion;
                                std::wcout << L"[INFO] Utilisation de la version XInput forcée: " 
                                          << StringToWString(xinputVersion); // Using forced XInput version: X
                                if (useSysWOW64) {
                                    std::wcout << L" (SysWOW64)";
                                }
                                std::wcout << std::endl;
                                
                                // Vérifier que le fichier XInput existe / Check that XInput file exists
                                std::wstring xinputPath = GetXInputPath(xinputVersion, useSysWOW64);
                                if (!FileExists(xinputPath)) {
                                    std::wcerr << L"[ERREUR] Le fichier XInput spécifié n'existe pas: " << xinputPath << std::endl; // ERROR: Specified XInput file does not exist: X
                                    xinputVersion.clear();
                                }
                            }
                            
                            // Si aucune version n'est forcée ou si le fichier n'existe pas, détecter automatiquement / If no version is forced or file doesn't exist, detect automatically
                            if (xinputVersion.empty()) {
                                xinputVersion = DetectXInputVersion(pe.th32ProcessID, processArch);
                                std::wcout << L"[INFO] Version XInput détectée: " << StringToWString(xinputVersion) << std::endl; // XInput version detected: X
                            }
                            
                            // Obtenir le chemin complet du jeu / Get full path of game
                            std::string processPath = GetProcessFullPath(pe.th32ProcessID);
                            
                            if (!processPath.empty()) {
                                std::string processDir = processPath.substr(0, processPath.find_last_of("\\/"));
                                std::string tmpFilePath = processDir + "\\XOrderPath.tmp";
                                
                                // Créer un fichier temporaire avec le chemin de l'injecteur et les infos du jeu / Create temporary file with injector path and game info
                                std::ofstream tmpFile(tmpFilePath, std::ios::out | std::ios::trunc);
                                if (tmpFile.is_open()) {
                                    // Écrire les informations nécessaires dans le fichier temporaire / Write necessary information to temporary file
                                    // 1. Chemin de l'injecteur (pour les [Settings]) / 1. Injector path (for [Settings])
                                    // 2. Version XInput à utiliser / 2. XInput version to use
                                    // 3. Indicateur SysWOW64 (1 si vrai, 0 sinon) / 3. SysWOW64 indicator (1 if true, 0 otherwise)
                                    // 4. Architecture du processus / 4. Process architecture
                                    tmpFile << baseDir << "\n" << xinputVersion << "\n" << (useSysWOW64 ? "1" : "0") 
                                           << "\n" << ArchitectureToString(processArch);
                                    tmpFile.close();
                                    
                                    std::wcout << L"[XOrderInjector] Injection dans " << wExe 
                                             << L" (PID: " << pe.th32ProcessID 
                                             << L", Arch: " << StringToWString(ArchitectureToString(processArch))
                                             << L") - Version XInput: " << StringToWString(xinputVersion)
                                             << (useSysWOW64 ? L" (SysWOW64)" : L" (System32)") << std::endl; // Injection into X (PID: Y, Arch: Z) - XInput version: W
                                    
                                    // Sélectionner la méthode d'injection selon l'architecture / Select injection method according to architecture
                                    std::string targetDllPath = GetArchSpecificDllPath(dllPath, processArch);
                                    bool injectionSuccess = false;
                                    
                                    // Si injection cross-architecture (x64 -> x86), utiliser un processus auxiliaire / If cross-architecture injection (x64 -> x86), use helper process
                                    ProcessArchitecture currentArch = GetProcessArchitecture(GetCurrentProcessId());
                                    if (currentArch == ProcessArchitecture::x64 && processArch == ProcessArchitecture::x86) {
                                        std::cout << "[XOrderInjector] Injection cross-architecture détectée, utilisation d'un processus auxiliaire..." << std::endl; // Cross-architecture injection detected, using helper process...
                                        injectionSuccess = InjectDLLWithHelper(pe.th32ProcessID, targetDllPath, processArch);
                                    } else {
                                        injectionSuccess = InjectDLLSimple(pe.th32ProcessID, targetDllPath);
                                    }
                                    
                                    if (injectionSuccess) {
                                        injected_pids.push_back(pe.th32ProcessID);
                                        // Attendre un peu pour laisser la DLL s'initialiser / Wait a bit to let DLL initialize
                                        std::this_thread::sleep_for(std::chrono::seconds(1));
                                    } else {
                                        std::wcerr << L"[XOrderInjector] ÉCHEC de l'injection dans " << wExe << std::endl; // INJECTION FAILED in X
                                        // Empêcher de réessayer immédiatement sur un processus qui bloque l'injection / Prevent immediate retry on process that blocks injection
                                        failed_pids.push_back(pe.th32ProcessID);
                                    }
                                } else {
                                    std::wcerr << L"[XOrderInjector] ERREUR: Impossible de créer le fichier de configuration temporaire "
                                    << StringToWString(tmpFilePath) << std::endl; // ERROR: Unable to create temporary configuration file
                                }
                            } else {
                                std::wcerr << L"[XOrderInjector] AVERTISSEMENT: Impossible d'obtenir le chemin du processus " 
                                          << wExe << L". L'injection est impossible." << std::endl; // WARNING: Unable to get process path X. Injection is impossible.
                            }
                        } else {
                            std::wcout << L"[XOrderInjector] Le processus " << wExe << L" (PID: " << pe.th32ProcessID 
                                     << L") est déjà injecté, ignoré." << std::endl; // is already injected, ignored.
                        }
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
        
        // Gestion de l'arrêt automatique
        if (initialDelayPassed && !hasActiveProcesses && hasGameBeenAttached) {
            if (!shutdownRequested) {
                std::cout << "[XOrderInjector] Aucun processus actif. Fermeture dans 10 secondes..." << std::endl;
                shutdownRequested = true;
                shutdownTime = std::chrono::steady_clock::now();
            }
        } else if (shutdownRequested && hasActiveProcesses) {
            std::cout << "[XOrderInjector] Nouveau processus détecté, annulation de la fermeture." << std::endl; // New process detected, cancelling shutdown.
            shutdownRequested = false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Vérification plus fréquente pour les manettes / More frequent checking for controllers
    }
    
    // Nettoyage final / Final cleanup
    std::cout << "[XOrderInjector] Nettoyage et fermeture..." << std::endl; // Cleanup and shutdown...
    
    // Fermer l'overlay de l'injecteur / Close injector overlay
    ShutdownInjectorOverlay();
    
    return 0;
}
