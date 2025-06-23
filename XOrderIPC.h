#pragma once

// Unique identifier for our WM_COPYDATA message, using ASCII for 'XO' ('XOrder')
#define XORDER_IPC_MESSAGE_ID 0x584F 

#define MAX_OVERLAY_MSG_LENGTH 256

// Data structure for the message payload
struct XOrderOverlayMsgData {
    wchar_t message[MAX_OVERLAY_MSG_LENGTH];
    DWORD duration;
    bool isSuccess; // Re-using the flag from the existing ShowMessage function
};
