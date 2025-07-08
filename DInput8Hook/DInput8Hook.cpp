#include "DInput8Hook.h"
#include <windows.h>
#include <dinput.h>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include "../XOrderIPC.h"

// Global variables
HMODULE g_hOriginalDInput8 = nullptr;
DirectInput8Create_t g_pOriginalDirectInput8Create = nullptr;
DllCanUnloadNow_t g_pOriginalDllCanUnloadNow = nullptr;
DllGetClassObject_t g_pOriginalDllGetClassObject = nullptr;
DllRegisterServer_t g_pOriginalDllRegisterServer = nullptr;
DllUnregisterServer_t g_pOriginalDllUnregisterServer = nullptr;

// IPC Globals
HANDLE g_hIPCThread = nullptr;
HWND g_hIPCWnd = nullptr;

void WriteToLog(const std::string& message) {
    std::ofstream log_file("DInput8Hook.log", std::ios_base::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
    }
}

// --- XOrderDirectInputDevice8 implementation ---
XOrderDirectInputDevice8::XOrderDirectInputDevice8(IDirectInputDevice8A* pOriginal) : 
    m_pOriginalDevice(pOriginal), 
    m_ulRefCount(1),
    m_dwStartButtonPressTime(0),
    m_bRemapMessageSent(false),
    m_hInjectorWnd(nullptr)
{
    m_hInjectorWnd = FindWindowW(XORDER_INJECTOR_WNDCLASS_NAME, NULL);
    if (m_hInjectorWnd) {
        WriteToLog("Successfully found injector window.");
    } else {
        WriteToLog("Failed to find injector window.");
    }
}
XOrderDirectInputDevice8::~XOrderDirectInputDevice8() {}

HRESULT XOrderDirectInputDevice8::QueryInterface(REFIID riid, LPVOID* ppvObj) { return m_pOriginalDevice->QueryInterface(riid, ppvObj); }
ULONG XOrderDirectInputDevice8::AddRef() { return m_pOriginalDevice->AddRef(); }
ULONG XOrderDirectInputDevice8::Release() { 
    ULONG ref = m_pOriginalDevice->Release();
    if (ref == 0) delete this;
    return ref;
}
HRESULT XOrderDirectInputDevice8::GetCapabilities(LPDIDEVCAPS lpDIDevCaps) { return m_pOriginalDevice->GetCapabilities(lpDIDevCaps); }
HRESULT XOrderDirectInputDevice8::EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) { return m_pOriginalDevice->EnumObjects(lpCallback, pvRef, dwFlags); }
HRESULT XOrderDirectInputDevice8::GetProperty(REFGUID rguidProp, LPDIPROPHEADER pdiph) { return m_pOriginalDevice->GetProperty(rguidProp, pdiph); }
HRESULT XOrderDirectInputDevice8::SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pdiph) { return m_pOriginalDevice->SetProperty(rguidProp, pdiph); }
HRESULT XOrderDirectInputDevice8::Acquire() { return m_pOriginalDevice->Acquire(); }
HRESULT XOrderDirectInputDevice8::Unacquire() { return m_pOriginalDevice->Unacquire(); }
HRESULT XOrderDirectInputDevice8::GetDeviceState(DWORD cbData, LPVOID lpvData) {
    HRESULT hr = m_pOriginalDevice->GetDeviceState(cbData, lpvData);

    if (SUCCEEDED(hr) && m_hInjectorWnd) {
        if (cbData == sizeof(DIJOYSTATE) || cbData == sizeof(DIJOYSTATE2)) {
            DIJOYSTATE* joyState = (DIJOYSTATE*)lpvData;
            WriteToLog("GetDeviceState hooked! Button 9 state: " + std::to_string(joyState->rgbButtons[9]));

            // Check Start button (usually button 9 on XInput controllers)
            if (joyState->rgbButtons[9] & 0x80) {
                if (m_dwStartButtonPressTime == 0) {
                    m_dwStartButtonPressTime = GetTickCount();
                }
                else if (!m_bRemapMessageSent && (GetTickCount() - m_dwStartButtonPressTime > 3000)) {
                    WriteToLog("Start button held for 3 seconds. Sending remap trigger.");
                    SendXOrderHookMessage(m_hInjectorWnd, CMD_REMAP_CONTROLLERS_TRIGGER, L"");
                    m_bRemapMessageSent = true; // Prevent sending multiple messages
                }
            } else {
                // Button released, reset state
                m_dwStartButtonPressTime = 0;
                m_bRemapMessageSent = false;
            }
        }
    }

    return hr;
}
HRESULT XOrderDirectInputDevice8::GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) { return m_pOriginalDevice->GetDeviceData(cbObjectData, rgdod, pdwInOut, dwFlags); }
HRESULT XOrderDirectInputDevice8::SetDataFormat(LPCDIDATAFORMAT lpdf) { return m_pOriginalDevice->SetDataFormat(lpdf); }
HRESULT XOrderDirectInputDevice8::SetEventNotification(HANDLE hEvent) { return m_pOriginalDevice->SetEventNotification(hEvent); }
HRESULT XOrderDirectInputDevice8::SetCooperativeLevel(HWND hwnd, DWORD dwFlags) { return m_pOriginalDevice->SetCooperativeLevel(hwnd, dwFlags); }
HRESULT XOrderDirectInputDevice8::GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA pdidoi, DWORD dwObj, DWORD dwHow) { return m_pOriginalDevice->GetObjectInfo(pdidoi, dwObj, dwHow); }
HRESULT XOrderDirectInputDevice8::GetDeviceInfo(LPDIDEVICEINSTANCEA pdidi) { return m_pOriginalDevice->GetDeviceInfo(pdidi); }
HRESULT XOrderDirectInputDevice8::RunControlPanel(HWND hwndOwner, DWORD dwFlags) { return m_pOriginalDevice->RunControlPanel(hwndOwner, dwFlags); }
HRESULT XOrderDirectInputDevice8::Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) { return m_pOriginalDevice->Initialize(hinst, dwVersion, rguid); }
HRESULT XOrderDirectInputDevice8::CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT* ppdeff, LPUNKNOWN punkOuter) { return m_pOriginalDevice->CreateEffect(rguid, lpeff, ppdeff, punkOuter); }
HRESULT XOrderDirectInputDevice8::EnumEffects(LPDIENUMEFFECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwEffType) { return m_pOriginalDevice->EnumEffects(lpCallback, pvRef, dwEffType); }
HRESULT XOrderDirectInputDevice8::GetEffectInfo(LPDIEFFECTINFOA pdei, REFGUID rguid) { return m_pOriginalDevice->GetEffectInfo(pdei, rguid); }
HRESULT XOrderDirectInputDevice8::GetForceFeedbackState(LPDWORD pdwOut) { return m_pOriginalDevice->GetForceFeedbackState(pdwOut); }
HRESULT XOrderDirectInputDevice8::SendForceFeedbackCommand(DWORD dwFlags) { return m_pOriginalDevice->SendForceFeedbackCommand(dwFlags); }
HRESULT XOrderDirectInputDevice8::EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback, LPVOID pvRef, DWORD fl) { return m_pOriginalDevice->EnumCreatedEffectObjects(lpCallback, pvRef, fl); }
HRESULT XOrderDirectInputDevice8::Escape(LPDIEFFESCAPE pesc) { return m_pOriginalDevice->Escape(pesc); }
HRESULT XOrderDirectInputDevice8::Poll() { return m_pOriginalDevice->Poll(); }
HRESULT XOrderDirectInputDevice8::SendDeviceData(DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD fl) { return m_pOriginalDevice->SendDeviceData(cbObjectData, rgdod, pdwInOut, fl); }
HRESULT XOrderDirectInputDevice8::EnumEffectsInFile(LPCSTR lpszFileName, LPDIENUMEFFECTSINFILECALLBACK pec, LPVOID pvRef, DWORD dwFlags) { return m_pOriginalDevice->EnumEffectsInFile(lpszFileName, pec, pvRef, dwFlags); }
HRESULT XOrderDirectInputDevice8::WriteEffectToFile(LPCSTR lpszFileName, DWORD dwEntries, LPDIFILEEFFECT rgDiFileEft, DWORD dwFlags) { return m_pOriginalDevice->WriteEffectToFile(lpszFileName, dwEntries, rgDiFileEft, dwFlags); }
HRESULT XOrderDirectInputDevice8::BuildActionMap(LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags) { return m_pOriginalDevice->BuildActionMap(lpdiaf, lpszUserName, dwFlags); }
HRESULT XOrderDirectInputDevice8::SetActionMap(LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags) { return m_pOriginalDevice->SetActionMap(lpdiaf, lpszUserName, dwFlags); }
HRESULT XOrderDirectInputDevice8::GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA lpdiDevImageInfoHeader) { return m_pOriginalDevice->GetImageInfo(lpdiDevImageInfoHeader); }

// --- XOrderDirectInput8 implementation ---
XOrderDirectInput8::XOrderDirectInput8(IDirectInput8A* pOriginal) : m_pOriginalDI(pOriginal), m_ulRefCount(1) {}
XOrderDirectInput8::~XOrderDirectInput8() {}

HRESULT XOrderDirectInput8::QueryInterface(REFIID riid, LPVOID* ppvObj) { return m_pOriginalDI->QueryInterface(riid, ppvObj); }
ULONG XOrderDirectInput8::AddRef() { return m_pOriginalDI->AddRef(); }
ULONG XOrderDirectInput8::Release() {
    ULONG ref = m_pOriginalDI->Release();
    if (ref == 0) delete this;
    return ref;
}
HRESULT XOrderDirectInput8::CreateDevice(REFGUID rguid, LPDIRECTINPUTDEVICE8A* lplpDirectInputDevice, LPUNKNOWN pUnkOuter) {
    WriteToLog("XOrderDirectInput8::CreateDevice hooked!");
    HRESULT hr = m_pOriginalDI->CreateDevice(rguid, lplpDirectInputDevice, pUnkOuter);
    if (SUCCEEDED(hr)) {
        *lplpDirectInputDevice = new XOrderDirectInputDevice8(*lplpDirectInputDevice);
    }
    return hr;
}
HRESULT XOrderDirectInput8::EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) { return m_pOriginalDI->EnumDevices(dwDevType, lpCallback, pvRef, dwFlags); }
HRESULT XOrderDirectInput8::GetDeviceStatus(REFGUID rguidInstance) { return m_pOriginalDI->GetDeviceStatus(rguidInstance); }
HRESULT XOrderDirectInput8::RunControlPanel(HWND hwndOwner, DWORD dwFlags) { return m_pOriginalDI->RunControlPanel(hwndOwner, dwFlags); }
HRESULT XOrderDirectInput8::Initialize(HINSTANCE hinst, DWORD dwVersion) { return m_pOriginalDI->Initialize(hinst, dwVersion); }
HRESULT XOrderDirectInput8::FindDevice(REFGUID rguidClass, LPCSTR ptszName, LPGUID pguidInstance) { return m_pOriginalDI->FindDevice(rguidClass, ptszName, pguidInstance); }
HRESULT XOrderDirectInput8::EnumDevicesBySemantics(LPCSTR ptszUserName, LPDIACTIONFORMATA lpdiActionFormat, LPDIENUMDEVICESBYSEMANTICSCBA lpCallback, LPVOID pvRef, DWORD dwFlags) { return m_pOriginalDI->EnumDevicesBySemantics(ptszUserName, lpdiActionFormat, lpCallback, pvRef, dwFlags); }
HRESULT XOrderDirectInput8::ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK lpdiCallback, LPDICONFIGUREDEVICESPARAMSA lpdiCDParams, DWORD dwFlags, LPVOID pvRefData) { return m_pOriginalDI->ConfigureDevices(lpdiCallback, lpdiCDParams, dwFlags, pvRefData); }

// IPC Window Procedure
LRESULT CALLBACK IPCWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COPYDATA: {
            COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
            if (pcds->dwData == XORDER_IPC_MESSAGE_ID) {
                WriteToLog("Received IPC message from injector.");
                // Command processing logic will go here
            }
            return 1;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// IPC Thread Function
DWORD WINAPI IPCThread(LPVOID lpParam) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = IPCWindowProc;
    wc.hInstance = (HINSTANCE)lpParam;
    wc.lpszClassName = XORDER_IPC_WNDCLASS_NAME;

    if (!RegisterClassW(&wc)) {
        WriteToLog("Failed to register IPC window class.");
        return 1;
    }

    g_hIPCWnd = CreateWindowW(
        XORDER_IPC_WNDCLASS_NAME,
        L"XOrder DInput8Hook IPC",
        0, 0, 0, 0, 0,
        HWND_MESSAGE,
        NULL, (HINSTANCE)lpParam, NULL
    );

    if (!g_hIPCWnd) {
        WriteToLog("Failed to create IPC window.");
        return 1;
    }

    WriteToLog("IPC listener thread started successfully.");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassW(XORDER_IPC_WNDCLASS_NAME, (HINSTANCE)lpParam);
    WriteToLog("IPC listener thread finished.");
    return 0;
}

void LoadOriginalDInput8() {
    if (g_hOriginalDInput8) return;

    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    strcat_s(systemPath, "\\dinput8.dll");

    g_hOriginalDInput8 = LoadLibraryA(systemPath);
    if (g_hOriginalDInput8) {
        g_pOriginalDirectInput8Create = (DirectInput8Create_t)GetProcAddress(g_hOriginalDInput8, "DirectInput8Create");
        g_pOriginalDllCanUnloadNow = (DllCanUnloadNow_t)GetProcAddress(g_hOriginalDInput8, "DllCanUnloadNow");
        g_pOriginalDllGetClassObject = (DllGetClassObject_t)GetProcAddress(g_hOriginalDInput8, "DllGetClassObject");
        g_pOriginalDllRegisterServer = (DllRegisterServer_t)GetProcAddress(g_hOriginalDInput8, "DllRegisterServer");
        g_pOriginalDllUnregisterServer = (DllUnregisterServer_t)GetProcAddress(g_hOriginalDInput8, "DllUnregisterServer");
    } else {
        WriteToLog("Failed to load original dinput8.dll");
    }
}

// Hooked DirectInput8Create
HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter) {
    WriteToLog("DirectInput8Create hooked!");

    if (!g_pOriginalDirectInput8Create) {
        LoadOriginalDInput8();
    }

    IDirectInput8A* pDI = nullptr;
    HRESULT hr = g_pOriginalDirectInput8Create(hinst, dwVersion, riidltf, (LPVOID*)&pDI, punkOuter);

    if (SUCCEEDED(hr)) {
        *ppvOut = new XOrderDirectInput8(pDI);
    }

    return hr;
}

HRESULT WINAPI DllCanUnloadNow() {
    if (g_pOriginalDllCanUnloadNow) return g_pOriginalDllCanUnloadNow();
    return S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (g_pOriginalDllGetClassObject) return g_pOriginalDllGetClassObject(rclsid, riid, ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllRegisterServer() {
    if (g_pOriginalDllRegisterServer) return g_pOriginalDllRegisterServer();
    return E_FAIL;
}

HRESULT WINAPI DllUnregisterServer() {
    if (g_pOriginalDllUnregisterServer) return g_pOriginalDllUnregisterServer();
    return E_FAIL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            WriteToLog("DInput8Hook attached.");
            LoadOriginalDInput8();
            g_hIPCThread = CreateThread(NULL, 0, IPCThread, hModule, 0, NULL);
            if (g_hIPCThread == NULL) {
                WriteToLog("Failed to create IPC thread.");
            }
            break;
        case DLL_PROCESS_DETACH:
            WriteToLog("DInput8Hook detached.");
            if (g_hIPCWnd) {
                DestroyWindow(g_hIPCWnd);
            }
            if (g_hIPCThread) {
                WaitForSingleObject(g_hIPCThread, 2000);
                CloseHandle(g_hIPCThread);
            }
            if (g_hOriginalDInput8) {
                FreeLibrary(g_hOriginalDInput8);
            }
            break;
    }
    return TRUE;
}
