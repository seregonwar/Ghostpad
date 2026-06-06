<p align="center">
  <img width="180" alt="Ghostpad icon" src="https://github.com/user-attachments/assets/cfdbc55a-60f9-498d-b166-8613a19ab3a0" />
</p>

<h1 align="center">Ghostpad</h1>

<p align="center">
  <strong>PS4/PS5 Virtual Controller Bridge</strong>
</p>

<p align="center">
  Remote-control a jailbroken PS4 or PS5 from a desktop app, web browser, or BLE controller through an ESP32-WROOM-32U bridge.
</p>

<p align="center">
  <a href="#overview">Overview</a> ·
  <a href="#features">Features</a> ·
  <a href="#architecture">Architecture</a> ·
  <a href="#quick-start">Quick Start</a> ·
  <a href="#protocol-reference">Protocol Reference</a> ·
  <a href="#credits">Credits</a>
</p>

---

https://github.com/user-attachments/assets/30c7f384-4fa8-411c-aa07-84c59eda6037

## Overview

Ghostpad is a **payload + bridge + client** system that creates a virtual DualSense-compatible controller on a jailbroken PS4 or PS5 and forwards controller input over the local network.

The console shell and games see the injected device as a real controller. Input can come from:

- the native **Ghostpad Desktop GUI**;
- the embedded **ESP32 web controller**;
- a physical **BLE gamepad** such as DualSense or DualShock 4;
- keyboard, gamepad, or browser-based controls, depending on the client.

Ghostpad is designed for local-network control, diagnostics, development, and controller redirection workflows.

> **Compatibility note**  
> Ghostpad targets jailbroken PS4/PS5 environments. Firmware-specific byte patterns may require validation or updates through `tools/vda_probe`.

---

## Features

### Console payload

- Creates a virtual DualSense through the console virtual device API.
- Receives fixed-size GPAD input packets over TCP.
- Inserts controller reports into the virtual device stream.
- Supports runtime VDA patching for self and `SceShellCore`.
- Provides optional `/dev/klog` capture and TCP streaming.
- Supports MBus-based virtual/physical device binding.

### ESP32 bridge

- Runs on ESP32-WROOM-32U using ESP-IDF.
- Provides WiFi STA mode with AP fallback.
- Hosts an embedded web controller UI.
- Scans the subnet for PS4/PS5 services.
- Bridges GPAD input to the console over TCP.
- Scans, classifies, and connects to BLE HID controllers.
- Supports TinyUSB HID output for direct USB HID use cases.
- Streams klog data to dashboard clients through WebSocket.

### Desktop GUI

- Native C++17 cross-platform client (GLFW + OpenGL + Dear ImGui).
- Virtual controller UI with real-time gamepad visualizer.
- Keyboard and native gamepad input mappings.
- Advanced macro engine (recording, playback, and customization).
- Integrated console manager with network scanning and auto-deploy workflow.
- Console system actions (reboot, shutdown, rest mode, and disc eject).
- Dedicated diagnostic and testing views (Beeper control, project/profile stores).
- High performance, low latency, and lightweight resource footprint.

---

## Architecture

```text
                                      TCP 6967
                               GPAD controller input
  ┌─────────────────┐      ───────────────────────────►      ┌─────────────────────┐
  │  Desktop App    │                                       │  PS4 / PS5 Console   │
  │  C++ / ImGui    │      ◄──────── TCP 3434 ────────────  │  ghostpad.elf        │
  └─────────────────┘             klog stream               └──────────┬──────────┘
                                                                        │
                                                                        │
                                                                        ▼
                                                              Virtual DualSense
                                                              via VDA / VDI

  ┌─────────────────┐
  │  Browser UI     │
  │  Web Controller │
  └────────┬────────┘
           │ WiFi
           ▼
  ┌─────────────────┐         TCP 6967          ┌─────────────────────┐
  │  ESP32 Bridge   │ ────────────────────────► │  PS4 / PS5 Console  │
  │  Web + BLE HID  │                           │  ghostpad.elf       │
  └────────┬────────┘                           └─────────────────────┘
           │
           │ BLE
           ▼
  ┌─────────────────┐
  │  BLE Gamepad    │
  │  DS5 / DS4 etc. │
  └─────────────────┘


```

### Components

| Component | Path | Purpose |
|---|---:|---|
| Console payload | `payload/` | ELF payload for PS4/PS5. Creates the virtual controller, accepts GPAD input, handles VDA/VDI and optional klog streaming. |
| ESP32 bridge | `esp32-ghostpad/` | Firmware for WiFi, web UI, BLE HID host, USB HID, subnet scan, API, and WebSocket streaming. |
| Desktop GUI | `Ghostpad-native/` | Native C++17 (GLFW/ImGui) cross-platform client with advanced controller, macro, deployment, and console-management features. |
| VDA probe | `tools/vda_probe/` | Diagnostic payload for fingerprinting VDA byte patterns and MBus symbols. |
| Research notes | `virtualDS5research.md` | Technical write-up for the virtual DualSense research and implementation. |

---

## Quick Start

### 1. Build the console payload

#### PS4

```bash
cd payload

make PS4_PAYLOAD_SDK=/path/to/ps4-payload-sdk
```

#### PS5

```bash
cd payload

make PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
```

Deploy the generated `ghostpad.elf` to the console through the ELF loader, typically on port `9021`.

#### Payload build flags

| Flag | Default | Description |
|---|---:|---|
| `GHOSTPAD_ENABLE_INTERNAL_KLOG` | `1` | Enables the in-payload `/dev/klog` reader and TCP bridge. |
| `GHOSTPAD_KLOG_PORT` | `3434` | TCP port used for klog streaming. |
| `GHOSTPAD_ENABLE_RUNTIME_VDA_PATCH` | `1` | Enables runtime patching of libScePad VDA paths in self and `SceShellCore`. |
| `GHOSTPAD_ENABLE_KLOG_AUTOBIND` | `0` | Enables automatic virtual-device binding from klog `DEVICE_ADDED` events. |

---

### 2. Build and flash the ESP32 bridge

```bash
cd esp32-ghostpad

cp sdkconfig.private.example sdkconfig.private.defaults
# Edit sdkconfig.private.defaults with your WiFi SSID and password.

source ~/esp/esp-idf/export.sh

idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash
```

After boot, the ESP32 bridge is available at:

```text
http://ghostpad.local
```

If mDNS is unavailable, use the device IP shown by your router or serial monitor.

---

### 3. Build the native Desktop GUI

#### Building on macOS / Linux:

```bash
cd Ghostpad-native
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On macOS, this builds the application bundle (`ghostpad-native.app`). On Linux/macOS, you can run the executable from the build directory:

```bash
./build/ghostpad-native
```

#### Building on Windows:

Using Command Prompt or PowerShell, run:

```cmd
cd Ghostpad-native
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This produces `ghostpad-native.exe` in `build/` (or `build/Release/`).

First-time desktop workflow:

1. Start the ELF loader on the console.
2. Run the native Ghostpad GUI.
3. Add the console IP in the **Consoles** or **Settings** screen.
4. Click **Deploy** to load the payload.
5. Click **Connect**.

---

## Usage Modes

| Mode | Input source | Transport | Output target | Best for |
|---|---|---|---|---|
| Desktop GUI → Console | Keyboard, gamepad, virtual controller | TCP `6967` | Virtual DualSense | Full-featured cross-platform control |
| Browser → ESP32 → Console | Web UI | WebSocket + TCP `6967` | Virtual DualSense | Lightweight LAN control |
| BLE gamepad → ESP32 → Console | DualSense, DualShock 4, compatible BLE HID devices | BLE + TCP `6967` | Virtual DualSense | Wireless controller relay |
| Browser → ESP32 → USB | Web UI | WebSocket + USB HID | PS5 USB HID | Direct wired control experiments |
| Payload → Client | Console klog | TCP `3434` / WebSocket | GUI dashboard | Diagnostics and binding analysis |

---

## ESP32 Bridge

### Web UI

The embedded dashboard includes:

- **Device Browser** — scans the subnet and detects open Ghostpad-related ports.
- **Virtual Controller** — browser-based controller with keyboard and touch support.
- **Klog Viewer** — live klog stream through WebSocket.
- **BLE Scanner** — detects and classifies nearby controllers.
- **Settings** — WiFi, payload deployment, and firmware information.

### BLE HID host

The bridge can scan and classify common controllers, including:

- DualSense `0x054C:0x0CE6`;
- DualShock 4 `0x054C:0x09CC` / `0x054C:0x05C4`;
- Xbox, Nintendo, and 8BitDo-compatible HID devices.

BLE support includes:

- 30-second active scan window;
- classification scoring by model, appearance, and HID flags;
- DualSense report `0x01` decoding;
- DualShock 4 report `0x11` decoding;
- klog backlog replay for late-connecting dashboard clients.

### Networking

- WiFi station mode.
- AP fallback: `Ghostpad-ESP32` / `ghostpad123`.
- mDNS hostname: `ghostpad.local`.
- Subnet scanner with probes for ports `6967`, `3434`, `9021`, and `9090`.
- Multi-client klog broadcast.
- Private WiFi credentials stored in `sdkconfig.private.defaults` and excluded from Git.

---

## Protocol Reference

### TCP `6967` — GPAD input stream

Each input report is a fixed 16-byte packet.

| Offset | Type | Field |
|---:|---|---|
| `0..3` | `char[4]` | Magic value: `GPAD` |
| `4..7` | `uint32 BE` | Button bitmask |
| `8` | `uint8` | Left stick X, `0..255`, center `128` |
| `9` | `uint8` | Left stick Y |
| `10` | `uint8` | Right stick X |
| `11` | `uint8` | Right stick Y |
| `12` | `uint8` | L2 analog value, `0..255` |
| `13` | `uint8` | R2 analog value, `0..255` |
| `14..15` | `uint16` | Reserved |

---

### TCP `6970` — Control and diagnostics

| Magic | Payload | Effect |
|---|---|---|
| `GBND` | `uint32 userId` + `uint64 virtualDeviceId` + `uint64 physicalDeviceId` | Disconnects the physical device and binds the virtual device to the user. |
| `HVDI` | `uint32 padHandle` | Sends a diagnostic VDI Cross press. |
| `TYPE` | `uint32 type` where `0 = DS4`, `3 = DualSense` | Reconfigures the virtual-device type. |
| `DISC` | Empty | Disconnects the virtual device. |

---

### WebSocket `/ws` — Controller stream

Browser clients send JSON frames at approximately 30 Hz:

```json
{
  "buttons": 0,
  "lx": 128,
  "ly": 128,
  "rx": 128,
  "ry": 128,
  "l2": 0,
  "r2": 0,
  "target_ip": "192.168.1.x",
  "seq": 1,
  "ts": 1717430000000
}
```

The ESP32 bridge returns periodic status frames:

```json
{
  "event": "state",
  "connected": true,
  "tx": 1500,
  "err": 0
}
```

---

### HTTP API

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | `GET` | Returns system status, WiFi state, controller state, and scan results. |
| `/api/scan` | `GET` | Starts a subnet scan for consoles. |
| `/api/ble/scan` | `GET` | Starts a BLE scan for controllers. |
| `/api/ble/status` | `GET` | Returns BLE connection state. |
| `/api/ble/connect` | `POST` | Connects to a BLE controller using `{"addr":"xx:xx:xx:xx:xx:xx"}`. |
| `/api/ble/disconnect` | `POST` | Disconnects the active BLE controller. |
| `/api/controller/pulse` | `GET` | Sends a diagnostic Cross press test to `?ip=192.168.1.x`. |
| `/api/settings/deploy` | `POST` | Deploys the ELF payload to the selected console. |
| `/ws` | `WS` | WebSocket stream for controller input. |
| `/ws/klog` | `WS` | WebSocket stream for klog output. |

---

## Building from Source

### Requirements

#### Console payload

- `ps4-payload-sdk` or `ps5-payload-sdk`;
- LLVM toolchain;
- `LLVM_CONFIG` pointing to a compatible `llvm-config`, when required by your SDK setup.

#### ESP32 bridge

- ESP-IDF `v5.3+`;
- ESP32-WROOM-32U board;
- USB serial adapter or native USB flashing path supported by your board.

#### Desktop GUI

- CMake 3.20+;
- C++17 compiler (GCC, Clang, or MSVC);
- Python 3 (required for automated asset embedding);
- GLFW/OpenGL development libraries (on Linux/Ubuntu, install `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`).

---

### PS4 payload example

```bash
cd payload

LLVM_CONFIG=/opt/homebrew/opt/llvm/bin/llvm-config \
PS4_PAYLOAD_SDK=../external/ps4-payload-sdk \
make
```

Output:

```text
payload/ghostpad.elf
```

---

### PS5 payload example

```bash
cd payload

PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk make
```

Output:

```text
payload/ghostpad.elf
```

---

### ESP32 bridge example

```bash
cd esp32-ghostpad

cp sdkconfig.private.example sdkconfig.private.defaults
# Edit sdkconfig.private.defaults.

source ~/esp/esp-idf/export.sh

idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash
```

---

### Desktop GUI example

```bash
cd Ghostpad-native
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## VDA Probe

`tools/vda_probe` is a diagnostic payload used to validate firmware-specific VDA assumptions.

It can:

- enumerate libScePad exports;
- identify the VDA dispatch function;
- sweep MBus symbols used by the binding flow;
- report instruction bytes for pattern matching in `shellui_pad.c`.

Build:

```bash
cd tools/vda_probe

make PS4_PAYLOAD_SDK=/path/to/sdk
```

Deploy `vda_probe.elf` and inspect the console klog output.

---

## Keyboard Bindings

| Key | Action | Key | Action |
|---|---|---|---|
| `W` / `A` / `S` / `D` | D-pad | `I` / `J` / `K` / `L` | Triangle / Square / Cross / Circle |
| `U` / `O` | L1 / R1 | `Q` / `E` | L2 / R2 |
| `Enter` | Options | `Backspace` | Share / Create |
| `Space` | PS button | `T` | Touchpad |

---

## Project Structure

```text
payload/
├── main.c
├── shellui_pad.c
├── shellui_pad.h
└── Makefile

esp32-ghostpad/
├── main/
│   ├── main.c
│   ├── wifi_ap.c
│   ├── wifi_ap.h
│   ├── web_server.c
│   ├── web_server.h
│   ├── web_gui.h
│   ├── ble_hid_host.c
│   ├── ble_hid_host.h
│   ├── hid_gamepad.c
│   ├── hid_gamepad.h
│   ├── klog_reader.c
│   ├── klog_reader.h
│   └── CMakeLists.txt
├── sdkconfig.defaults
├── sdkconfig.private.example
├── sdkconfig.private.defaults
└── CMakeLists.txt

Ghostpad-native/
└── C++17 + ImGui + GLFW cross-platform desktop client

tools/
└── vda_probe/
    └── vda_probe.c

git-patch/
└── Development patches

virtualDS5research.md
```

---

## Known Limitations

- **Firmware-specific VDA patterns** — unsupported firmware builds may require new scan patterns generated with `vda_probe`.
- **Single virtual device** — Ghostpad currently manages one virtual controller at a time.
- **No haptics or rumble feedback** — feedback from PS4/PS5 to the client is not currently forwarded.
- **BLE-only controller relay on ESP32** — Bluetooth Classic / BR/EDR is not supported.
- **DualSense BLE pairing window** — pairing requires the manual PS + Create sequence and a short advertising window.

---

## Credits

### StonedModder

- Original Ghostpad desktop app: Electron + React GUI (now deprecated).
- Original PS5 payload and VDA research.
- `scePadVirtualDeviceAddDevice` code-cave patch.
- `sceMbusBindDeviceWithUserId` / `sceMbusDisconnectDevice` MBus binding flow.
- Credential elevation and `SceShellCore` process injection.
- `hidDumper` companion tool.
- `virtualDS5research.md` technical write-up.

### SeregonWar

- ESP32-WROOM-32U bridge firmware.
- WiFi STA/AP with mDNS.
- Embedded web UI.
- BLE HID host using NimBLE scan/connect/classification.
- DualSense and DualShock 4 report decoding.
- TinyUSB HID support.
- Subnet scanner and port probing.
- Klog TCP bridge with multi-client broadcast and backlog replay.
- WebSocket controller streaming and HTTP REST API.
- Controller pulse diagnostics.
- PS4 payload port under `__ORBIS__`.
- Runtime VDA patching with byte-pattern verification.
- libSceMbus `dlopen` / `dlsym` integration.
- Payload-side klog architecture.
- GBND prebind loop and direct MBus bind path.
- VDA probe diagnostic tool.
- Klog parser improvements.
- Native C++ GUI (GLFW + OpenGL + Dear ImGui cross-platform client).

---

## Acknowledgements

- `ps5-payload-sdk` and `ps4-payload-sdk`, including work by John Törnblom and contributors.
- ESP-IDF.
- NimBLE.
- TinyUSB.
- cJSON.
- The PS4/PS5 development community.

---

## License

| Area | License |
|---|---|
| Payload C sources | GPL-3.0-or-later |
| ESP32 firmware | GPL-3.0-or-later |
| Native Desktop GUI | GPL-3.0-or-later |

---

## Disclaimer

Ghostpad is intended for development, research, and local-network experimentation on hardware you own or are authorized to work with. Use it responsibly and in accordance with applicable laws, platform terms, and local regulations.
