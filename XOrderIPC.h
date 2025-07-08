#pragma once

#include <windows.h>
#include <string>

// Unique identifier for our WM_COPYDATA message
#define XORDER_IPC_MESSAGE_ID 0x584F // 'XO'

// Window class names for IPC
#define XORDER_INJECTOR_WNDCLASS_NAME L"XOrderInjectorWindowClass" // Used by Injector to create its window
#define XORDER_IPC_WNDCLASS_NAME      L"XOrderIPCHookWindowClass"      // Used by Hooks to create their windows

// IPC Command Types
enum XOrderCommandType {
    CMD_OVERLAY,
    CMD_REMAP_CONTROLLERS_TRIGGER
};

// Structure for all IPC messages
struct XOrderIPCMessage {
    XOrderCommandType command;
    wchar_t data[256];
};

// Helper function to send an IPC message from a hook to the injector
inline bool SendXOrderHookMessage(HWND hTargetWnd, XOrderCommandType cmd, const wchar_t* msg)
{
    if (!hTargetWnd) return false;

    XOrderIPCMessage ipcMsg;
    ipcMsg.command = cmd;
    wcscpy_s(ipcMsg.data, _countof(ipcMsg.data), msg);

    COPYDATASTRUCT cds;
    cds.dwData = XORDER_IPC_MESSAGE_ID;
    cds.cbData = sizeof(XOrderIPCMessage);
    cds.lpData = &ipcMsg;

    // Use SendMessageTimeout to avoid the hook hanging if the injector is busy
    LRESULT result;
    SendMessageTimeout(hTargetWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds, SMTO_ABORTIFHUNG | SMTO_NORMAL, 1000, (PDWORD_PTR)&result);

    return result == 1;
}

// Helper function to send an overlay message from the injector to a hook
inline bool SendXOrderInjectorMessage(HWND hTargetWnd, const wchar_t* msg)
{
    if (!hTargetWnd) return false;

    XOrderIPCMessage ipcMsg;
    ipcMsg.command = CMD_OVERLAY;
    wcscpy_s(ipcMsg.data, _countof(ipcMsg.data), msg);

    COPYDATASTRUCT cds;
    cds.dwData = XORDER_IPC_MESSAGE_ID;
    cds.cbData = sizeof(XOrderIPCMessage);
    cds.lpData = &ipcMsg;

    // Use SendMessageTimeout to avoid the injector hanging if the hook is busy
    LRESULT result;
    SendMessageTimeout(hTargetWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds, SMTO_ABORTIFHUNG | SMTO_NORMAL, 1000, (PDWORD_PTR)&result);

    return result == 1;
}
