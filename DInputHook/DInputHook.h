#pragma once

#include <windows.h>
#include <dinput.h>

// Function pointer types
typedef HRESULT(WINAPI* DirectInputCreateA_t)(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA* lplpDirectInput, LPUNKNOWN punkOuter);
typedef HRESULT(WINAPI* DirectInputCreateW_t)(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW* lplpDirectInput, LPUNKNOWN punkOuter);
typedef HRESULT(WINAPI* DirectInputCreateEx_t)(HINSTANCE hinst, DWORD dwVersion, REFIID riid, LPVOID* ppvOut, LPUNKNOWN punkOuter);
typedef HRESULT(WINAPI* DllCanUnloadNow_t)();
typedef HRESULT(WINAPI* DllGetClassObject_t)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
typedef HRESULT(WINAPI* DllRegisterServer_t)();
typedef HRESULT(WINAPI* DllUnregisterServer_t)();
typedef LPCDIDATAFORMAT(WINAPI* GetdfDIJoystick_t)();

class XOrderDirectInputDevice : public IDirectInputDeviceA {
private:
    IDirectInputDeviceA* m_pOriginalDevice;
    ULONG m_ulRefCount;
    DWORD m_dwStartButtonPressTime; // Time when the start button was first pressed
    bool m_bRemapMessageSent;     // Flag to prevent sending multiple messages
    HWND m_hInjectorWnd;            // Handle to the injector's window

public:
    XOrderDirectInputDevice(IDirectInputDeviceA* pOriginal);
    ~XOrderDirectInputDevice();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // IDirectInputDeviceA methods
    STDMETHOD(GetCapabilities)(LPDIDEVCAPS lpDIDevCaps);
    STDMETHOD(EnumObjects)(LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(GetProperty)(REFGUID rguidProp, LPDIPROPHEADER pdiph);
    STDMETHOD(SetProperty)(REFGUID rguidProp, LPCDIPROPHEADER pdiph);
    STDMETHOD(Acquire)();
    STDMETHOD(Unacquire)();
    STDMETHOD(GetDeviceState)(DWORD cbData, LPVOID lpvData);
    STDMETHOD(GetDeviceData)(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
    STDMETHOD(SetDataFormat)(LPCDIDATAFORMAT lpdf);
    STDMETHOD(SetEventNotification)(HANDLE hEvent);
    STDMETHOD(SetCooperativeLevel)(HWND hwnd, DWORD dwFlags);
    STDMETHOD(GetObjectInfo)(LPDIDEVICEOBJECTINSTANCEA pdidoi, DWORD dwObj, DWORD dwHow);
    STDMETHOD(GetDeviceInfo)(LPDIDEVICEINSTANCEA pdidi);
    STDMETHOD(RunControlPanel)(HWND hwndOwner, DWORD dwFlags);
    STDMETHOD(Initialize)(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid);
};

class XOrderDirectInput : public IDirectInputA {
private:
    IDirectInputA* m_pOriginalDI;
    ULONG m_ulRefCount;
public:
    XOrderDirectInput(IDirectInputA* pOriginal);
    ~XOrderDirectInput();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // IDirectInputA methods
    STDMETHOD(CreateDevice)(REFGUID rguid, LPDIRECTINPUTDEVICEA* lplpDirectInputDevice, LPUNKNOWN pUnkOuter);
    STDMETHOD(EnumDevices)(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(GetDeviceStatus)(REFGUID rguidInstance);
    STDMETHOD(RunControlPanel)(HWND hwndOwner, DWORD dwFlags);
    STDMETHOD(Initialize)(HINSTANCE hinst, DWORD dwVersion);
};

// Exported functions
extern "C" {
    HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA* lplpDirectInput, LPUNKNOWN punkOuter);
    HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW* lplpDirectInput, LPUNKNOWN punkOuter);
    HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riid, LPVOID* ppvOut, LPUNKNOWN punkOuter);
    HRESULT WINAPI DllCanUnloadNow();
    HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
    HRESULT WINAPI DllRegisterServer();
    HRESULT WINAPI DllUnregisterServer();
    LPCDIDATAFORMAT WINAPI GetdfDIJoystick();
}
