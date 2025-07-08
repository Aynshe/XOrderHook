#pragma once

#include <windows.h>
#include <dinput.h>
#include <vector>

// Function pointer types
typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
typedef HRESULT(WINAPI* DllCanUnloadNow_t)();
typedef HRESULT(WINAPI* DllGetClassObject_t)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
typedef HRESULT(WINAPI* DllRegisterServer_t)();
typedef HRESULT(WINAPI* DllUnregisterServer_t)();

// Forward declarations
class XOrderDirectInput8;

// Our custom IDirectInputDevice8 wrapper
class XOrderDirectInputDevice8 : public IDirectInputDevice8A
{
private:
    IDirectInputDevice8A* m_pOriginalDevice;
    ULONG m_ulRefCount;
    DWORD m_dwStartButtonPressTime; // Time when the start button was first pressed
    bool m_bRemapMessageSent;     // Flag to prevent sending multiple messages
    HWND m_hInjectorWnd;            // Handle to the injector's window

public:
    XOrderDirectInputDevice8(IDirectInputDevice8A* pOriginal);
    ~XOrderDirectInputDevice8();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // IDirectInputDevice8 methods
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
    STDMETHOD(CreateEffect)(REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT* ppdeff, LPUNKNOWN punkOuter);
    STDMETHOD(EnumEffects)(LPDIENUMEFFECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwEffType);
    STDMETHOD(GetEffectInfo)(LPDIEFFECTINFOA pdei, REFGUID rguid);
    STDMETHOD(GetForceFeedbackState)(LPDWORD pdwOut);
    STDMETHOD(SendForceFeedbackCommand)(DWORD dwFlags);
    STDMETHOD(EnumCreatedEffectObjects)(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback, LPVOID pvRef, DWORD fl);
    STDMETHOD(Escape)(LPDIEFFESCAPE pesc);
    STDMETHOD(Poll)();
    STDMETHOD(SendDeviceData)(DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD fl);
    STDMETHOD(EnumEffectsInFile)(LPCSTR lpszFileName, LPDIENUMEFFECTSINFILECALLBACK pec, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(WriteEffectToFile)(LPCSTR lpszFileName, DWORD dwEntries, LPDIFILEEFFECT rgDiFileEft, DWORD dwFlags);
    STDMETHOD(BuildActionMap)(LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags);
    STDMETHOD(SetActionMap)(LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags);
    STDMETHOD(GetImageInfo)(LPDIDEVICEIMAGEINFOHEADERA lpdiDevImageInfoHeader);
};

// Our custom IDirectInput8 wrapper
class XOrderDirectInput8 : public IDirectInput8A
{
private:
    IDirectInput8A* m_pOriginalDI;
    ULONG m_ulRefCount;

public:
    XOrderDirectInput8(IDirectInput8A* pOriginal);
    ~XOrderDirectInput8();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // IDirectInput8 methods
    STDMETHOD(CreateDevice)(REFGUID rguid, LPDIRECTINPUTDEVICE8A* lplpDirectInputDevice, LPUNKNOWN pUnkOuter);
    STDMETHOD(EnumDevices)(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(GetDeviceStatus)(REFGUID rguidInstance);
    STDMETHOD(RunControlPanel)(HWND hwndOwner, DWORD dwFlags);
    STDMETHOD(Initialize)(HINSTANCE hinst, DWORD dwVersion);
    STDMETHOD(FindDevice)(REFGUID rguidClass, LPCSTR ptszName, LPGUID pguidInstance);
    STDMETHOD(EnumDevicesBySemantics)(LPCSTR ptszUserName, LPDIACTIONFORMATA lpdiActionFormat, LPDIENUMDEVICESBYSEMANTICSCBA lpCallback, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(ConfigureDevices)(LPDICONFIGUREDEVICESCALLBACK lpdiCallback, LPDICONFIGUREDEVICESPARAMSA lpdiCDParams, DWORD dwFlags, LPVOID pvRefData);
};

// Exported functions
extern "C" {
    HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
    HRESULT WINAPI DllCanUnloadNow();
    HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
    HRESULT WINAPI DllRegisterServer();
    HRESULT WINAPI DllUnregisterServer();
}
