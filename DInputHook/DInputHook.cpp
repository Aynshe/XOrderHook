#include "DInputHook.h"
#include <windows.h>
#include <dinput.h>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include "../XOrderIPC.h"

// Global variables
HMODULE g_hOriginalDInput = nullptr;
DirectInputCreateA_t g_pOriginalDirectInputCreateA = nullptr;
DirectInputCreateW_t g_pOriginalDirectInputCreateW = nullptr;
DirectInputCreateEx_t g_pOriginalDirectInputCreateEx = nullptr;
DllCanUnloadNow_t g_pOriginalDllCanUnloadNow = nullptr;
DllGetClassObject_t g_pOriginalDllGetClassObject = nullptr;
DllRegisterServer_t g_pOriginalDllRegisterServer = nullptr;
DllUnregisterServer_t g_pOriginalDllUnregisterServer = nullptr;
GetdfDIJoystick_t g_pOriginalGetdfDIJoystick = nullptr;

// IPC Globals
HANDLE g_hIPCThread = nullptr;
HWND g_hIPCWnd = nullptr;

void WriteToLog(const std::string& message) {
    std::ofstream log_file("DInputHook.log", std::ios_base::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
    }
}

// --- XOrderDirectInputDevice implementation ---
XOrderDirectInputDevice::XOrderDirectInputDevice(IDirectInputDeviceA* pOriginal) : 
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
XOrderDirectInputDevice::~XOrderDirectInputDevice() {}

HRESULT XOrderDirectInputDevice::QueryInterface(REFIID riid, LPVOID* ppvObj) { return m_pOriginalDevice->QueryInterface(riid, ppvObj); }
ULONG XOrderDirectInputDevice::AddRef() { return m_pOriginalDevice->AddRef(); }
ULONG XOrderDirectInputDevice::Release() {
    ULONG ref = m_pOriginalDevice->Release();
    if (ref == 0) delete this;
    return ref;
}
HRESULT XOrderDirectInputDevice::GetCapabilities(LPDIDEVCAPS lpDIDevCaps) { return m_pOriginalDevice->GetCapabilities(lpDIDevCaps); }
HRESULT XOrderDirectInputDevice::EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) { return m_pOriginalDevice->EnumObjects(lpCallback, pvRef, dwFlags); }
HRESULT XOrderDirectInputDevice::GetProperty(REFGUID rguidProp, LPDIPROPHEADER pdiph) { return m_pOriginalDevice->GetProperty(rguidProp, pdiph); }
HRESULT XOrderDirectInputDevice::SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pdiph) { return m_pOriginalDevice->SetProperty(rguidProp, pdiph); }
HRESULT XOrderDirectInputDevice::Acquire() { return m_pOriginalDevice->Acquire(); }
HRESULT XOrderDirectInputDevice::Unacquire() { return m_pOriginalDevice->Unacquire(); }
HRESULT XOrderDirectInputDevice::GetDeviceState(DWORD cbData, LPVOID lpvData) {
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
HRESULT XOrderDirectInputDevice::GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) { return m_pOriginalDevice->GetDeviceData(cbObjectData, rgdod, pdwInOut, dwFlags); }
HRESULT XOrderDirectInputDevice::SetDataFormat(LPCDIDATAFORMAT lpdf) { return m_pOriginalDevice->SetDataFormat(lpdf); }
HRESULT XOrderDirectInputDevice::SetEventNotification(HANDLE hEvent) { return m_pOriginalDevice->SetEventNotification(hEvent); }
HRESULT XOrderDirectInputDevice::SetCooperativeLevel(HWND hwnd, DWORD dwFlags) { return m_pOriginalDevice->SetCooperativeLevel(hwnd, dwFlags); }
HRESULT XOrderDirectInputDevice::GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA pdidoi, DWORD dwObj, DWORD dwHow) { return m_pOriginalDevice->GetObjectInfo(pdidoi, dwObj, dwHow); }
HRESULT XOrderDirectInputDevice::GetDeviceInfo(LPDIDEVICEINSTANCEA pdidi) { return m_pOriginalDevice->GetDeviceInfo(pdidi); }
HRESULT XOrderDirectInputDevice::RunControlPanel(HWND hwndOwner, DWORD dwFlags) { return m_pOriginalDevice->RunControlPanel(hwndOwner, dwFlags); }
HRESULT XOrderDirectInputDevice::Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) { return m_pOriginalDevice->Initialize(hinst, dwVersion, rguid); }

// --- XOrderDirectInput implementation ---
XOrderDirectInput::XOrderDirectInput(IDirectInputA* pOriginal) : m_pOriginalDI(pOriginal), m_ulRefCount(1) {}
XOrderDirectInput::~XOrderDirectInput() {}

HRESULT XOrderDirectInput::QueryInterface(REFIID riid, LPVOID* ppvObj) { return m_pOriginalDI->QueryInterface(riid, ppvObj); }
ULONG XOrderDirectInput::AddRef() { return m_pOriginalDI->AddRef(); }
ULONG XOrderDirectInput::Release() {
    ULONG ref = m_pOriginalDI->Release();
    if (ref == 0) delete this;
    return ref;
}
HRESULT XOrderDirectInput::CreateDevice(REFGUID rguid, LPDIRECTINPUTDEVICEA* lplpDirectInputDevice, LPUNKNOWN pUnkOuter) {
    WriteToLog("XOrderDirectInput::CreateDevice hooked!");
    HRESULT hr = m_pOriginalDI->CreateDevice(rguid, lplpDirectInputDevice, pUnkOuter);
    if (SUCCEEDED(hr)) {
        *lplpDirectInputDevice = new XOrderDirectInputDevice(*lplpDirectInputDevice);
    }
    return hr;
}
HRESULT XOrderDirectInput::EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) { return m_pOriginalDI->EnumDevices(dwDevType, lpCallback, pvRef, dwFlags); }
HRESULT XOrderDirectInput::GetDeviceStatus(REFGUID rguidInstance) { return m_pOriginalDI->GetDeviceStatus(rguidInstance); }
HRESULT XOrderDirectInput::RunControlPanel(HWND hwndOwner, DWORD dwFlags) { return m_pOriginalDI->RunControlPanel(hwndOwner, dwFlags); }
HRESULT XOrderDirectInput::Initialize(HINSTANCE hinst, DWORD dwVersion) { return m_pOriginalDI->Initialize(hinst, dwVersion); }

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
        L"XOrder DInputHook IPC",
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

void LoadOriginalDInput() {
    if (g_hOriginalDInput) return;

    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    strcat_s(systemPath, "\\dinput.dll");

    g_hOriginalDInput = LoadLibraryA(systemPath);
    if (g_hOriginalDInput) {
        g_pOriginalDirectInputCreateA = (DirectInputCreateA_t)GetProcAddress(g_hOriginalDInput, "DirectInputCreateA");
        g_pOriginalDirectInputCreateW = (DirectInputCreateW_t)GetProcAddress(g_hOriginalDInput, "DirectInputCreateW");
        g_pOriginalDirectInputCreateEx = (DirectInputCreateEx_t)GetProcAddress(g_hOriginalDInput, "DirectInputCreateEx");
        g_pOriginalDllCanUnloadNow = (DllCanUnloadNow_t)GetProcAddress(g_hOriginalDInput, "DllCanUnloadNow");
        g_pOriginalDllGetClassObject = (DllGetClassObject_t)GetProcAddress(g_hOriginalDInput, "DllGetClassObject");
        g_pOriginalDllRegisterServer = (DllRegisterServer_t)GetProcAddress(g_hOriginalDInput, "DllRegisterServer");
        g_pOriginalDllUnregisterServer = (DllUnregisterServer_t)GetProcAddress(g_hOriginalDInput, "DllUnregisterServer");
        g_pOriginalGetdfDIJoystick = (GetdfDIJoystick_t)GetProcAddress(g_hOriginalDInput, "GetdfDIJoystick");
    } else {
        WriteToLog("Failed to load original dinput.dll");
    }
}

// Hooked functions
HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA* lplpDirectInput, LPUNKNOWN punkOuter) {
    WriteToLog("DirectInputCreateA hooked!");
    if (!g_pOriginalDirectInputCreateA) LoadOriginalDInput();
    
    IDirectInputA* pDI = nullptr;
    HRESULT hr = g_pOriginalDirectInputCreateA(hinst, dwVersion, &pDI, punkOuter);

    if (SUCCEEDED(hr)) {
        *lplpDirectInput = new XOrderDirectInput(pDI);
    }

    return hr;
}

HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW* lplpDirectInput, LPUNKNOWN punkOuter) {
    if (!g_pOriginalDirectInputCreateW) LoadOriginalDInput();
    if (g_pOriginalDirectInputCreateW) return g_pOriginalDirectInputCreateW(hinst, dwVersion, lplpDirectInput, punkOuter);
    return E_FAIL;
}

HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riid, LPVOID* ppvOut, LPUNKNOWN punkOuter) {
    if (!g_pOriginalDirectInputCreateEx) LoadOriginalDInput();
    if (g_pOriginalDirectInputCreateEx) return g_pOriginalDirectInputCreateEx(hinst, dwVersion, riid, ppvOut, punkOuter);
    return E_FAIL;
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

LPCDIDATAFORMAT WINAPI GetdfDIJoystick() {
    if (g_pOriginalGetdfDIJoystick) return g_pOriginalGetdfDIJoystick();
    return nullptr;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            WriteToLog("DInputHook attached.");
            LoadOriginalDInput();
            g_hIPCThread = CreateThread(NULL, 0, IPCThread, hModule, 0, NULL);
            if (g_hIPCThread == NULL) {
                WriteToLog("Failed to create IPC thread.");
            }
            break;
        case DLL_PROCESS_DETACH:
            WriteToLog("DInputHook detached.");
            if (g_hIPCWnd) {
                DestroyWindow(g_hIPCWnd);
            }
            if (g_hIPCThread) {
                WaitForSingleObject(g_hIPCThread, 2000);
                CloseHandle(g_hIPCThread);
            }
            if (g_hOriginalDInput) {
                FreeLibrary(g_hOriginalDInput);
            }
            break;
    }
    return TRUE;
}
