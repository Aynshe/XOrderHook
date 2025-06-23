# XOrderHook  ⚠️ *Important*: Hook injection may not work with all games due to anti-cheat protections or incompatible engines.


# 🎮 XOrder - XInput Controller dynamically reorder

My need was to be able to force the XInput index in Windows games for my arcade cabinet controllers, which can sometimes randomly swap—especially in single-player games that pick the wrong arcade stick. With this hook, I can force the index dynamically in-game.

Sometimes I use my arcade cabinet from another screen with additional controllers connected, and the issue of controllers being assigned to the wrong index becomes quite annoying. This solution allows me to avoid unplugging arcade sticks from USB or relying on a smart home workaround.
I encounter the same issue when connecting via Moonlight using the Sunshine server.

The main goal was to be able to reassign a controller to player 1 on the fly, especially in games that don’t allow freely choosing from all connected devices.

🎯 I’d also like to make the tool compatible with some games that seem to use DirectInput (DInput), but since I’m not a developer, I’ve had a hard time figuring it out.
As a fallback, the DevReorder tool can handle this kind of setup, but it doesn’t allow dynamic reassignment In-Game.

ℹ️ XInput version detection may sometimes be inaccurate. In such cases, you can force the XInput version manually for a specific game by editing the XOrderConfig.ini file.


## 📌 Overview  
XOrder is a Windows-based system that dynamically reorganizes XInput controllers in games through hooking. This ensures that the first controller is assigned to index 0, and subsequent controllers are incremented accordingly.

## 🧩 Components

### 🧷 `XOrderHook.dll`
- **Purpose**: DLL injected into game processes  
- **Function**: Intercepts XInput calls
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
- **Purpose**: Utility for listing XInput Controller (only for information)

## 🎯 Automatic Game Detection
- 🧠 Monitors running processes against configured game list  
- 💻 Supports both x86 and x64 game architectures  
- 🔎 Automatic XInput version detection per game  

## 🎮 Controller Integration
- ⬆️ **START + UP (3 seconds)**: Add current focused game to watch list *(must be done within 90 seconds after injection starts)*
- ✅ **START (3 seconds)**: Activate controller on XInput index 0  
- ♻️ **START + DOWN (3 seconds)**: Reset mapping to default  

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
- 🔄 Reset Mapping: Press START + DOWN for 3 seconds to reset the mapping to default (confirmed by a strong vibration).

## 🧠 Technical Details
- 🧑‍💻 Languages: C++ with Windows API
- 💉 Injection Method: DLL injection via LoadLibrary or manual PE loading
- 🖥️ Architectures: Native x64 with x86 compatibility
- 🎮 XInput Support: All versions from XInput 1.1 to 1.4




