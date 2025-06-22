# XOrderHook  ⚠️ **Important**: Hook injection may not work with all games due to anti-cheat protections or incompatible engines.


# 🎮 XOrder - XInput Controller dynamically reorder

My need was to be able to force the XInput index on Windows games for my arcade cabinet controllers, which can randomly reverse, especially with single-player games that used the wrong arcade stick. With this hook, I can force the index in-game on the fly.

## 📌 Overview  
XOrder is a Windows-based system that dynamically reorganizes XInput controllers in games through hooking. This ensures that the first controller is assigned to index 0, and subsequent controllers are incremented accordingly.

## 🧩 Components

### 🧷 `XOrderHook.dll`
- **Purpose**: DLL injected into game processes  
- **Function**: Intercepts XInput calls and applies custom button mappings  
- **Architecture**: Supports both x86 and x64 games  

### 🚀 `XOrderInjector.exe`
- **Purpose**: Watchdog service that monitors and injects games  
- **Function**: Automatically detects target games and injects the hook DLL  
- **Features**:  
  - 🔍 Automatic game detection from configuration  
  - 🔄 Cross-architecture injection (x64 → x86)  
  - 🎮 Controller combo detection for adding games  
  - 💬 Visual overlay notifications  

### 🗒️ `ListerXOrder.exe`
- **Purpose**: Utility for listing running processes and XInput information  
- **Function**: Helps identify games and their XInput usage for configuration  

## 🎯 Automatic Game Detection
- 🧠 Monitors running processes against configured game list  
- 💻 Supports both x86 and x64 game architectures  
- 🔎 Automatic XInput version detection per game  

## 🎮 Controller Integration
- ⬆️ **START + UP (3 seconds)**: Add current focused game to watch list *(must be done within 90 seconds after injection starts)*
- ✅ **START (3 seconds)**: Activate controller on XInput index 0  
- ♻️ **START + BACK (3 seconds)**: Reset mapping to default  

## ⚙️ Configuration Management
- 📝 INI-based configuration (`XOrderConfig.ini`)  
- 🔁 Hot-reload when games are added via controller  
- 📌 Per-game XInput version forcing  
- 📂 SysWOW64/System32 path specification

## 🧾 Configuration Format

```ini
[Games]
game1.exe
game2.exe "xinput1_3"
game3.exe "SysWOW64\xinput1_4"
```

## ▶️ Usage
- 📂 Setup: Place all executables in the same directory with XOrderConfig.ini or use START + UP combo in-game
- 🏃 Run: Execute XOrderInjector.exe
- ➕ Add Games: Use START + UP combo while focused on a game window  *(must be done within 90 seconds after injection starts)*
- 🔢 Activate First Controller: Press START for 3 seconds to assign the controller to index 0. Others increment from 1 to 3.
- 🔄 Reset Mapping: Press START + BACK for 3 seconds to reset the mapping to default (confirmed by a strong vibration).

## 🧠 Technical Details
- 🧑‍💻 Languages: C++ with Windows API
- 💉 Injection Method: DLL injection via LoadLibrary or manual PE loading
- 🖥️ Architectures: Native x64 with x86 compatibility
- 🎮 XInput Support: All versions from XInput 1.1 to 1.4




