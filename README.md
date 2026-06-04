<img width="580" height="601" alt="icon" src="https://github.com/user-attachments/assets/cfdbc55a-60f9-498d-b166-8613a19ab3a0" />

# Ghostpad — PS4/PS5 Virtual Controller Bridge

https://github.com/user-attachments/assets/30c7f384-4fa8-411c-aa07-84c59eda6037

> Remote-control a jailbroken PS4 or PS5 from any device: PC app, web browser, or real BLE controller — via an ESP32-P4 bridge that forwards HID input over TCP to a virtual DualSense running on the console.

Ghostpad is a **payload + bridge + GUI** system that creates a **virtual DualSense** on a jailbroken PS4/PS5 and forwards input over LAN. The shell and games see it as a real controller.

## Architecture

```
                          TCP 6967 (GPAD input)
  ┌──────────┐  ──────────────────────────────────►  ┌─────────────────┐
  │  PC GUI   │                                       │  PS4/PS5        │
  │  (Python) │  ◄─────── klog TCP 3434 ────────────  │  ghostpad.elf   │
  └──────────┘                                       └─────────────────┘
                                                              │
  ┌──────────────┐                                    scePadVirtualDevice
  │  Browser     │──WiFi──┐                           InsertData (VDI)
  │  (Web UI)    │        │                                   │
  └──────────────┘        │                            virtual DualSense
                          ▼                            @ slot 0
  ┌──────────────┐  ┌─────────────────────┐
  │  BLE Gamepad │──│  ESP32-P4 Bridge    │──USB HID──► PS5
  │  (DualSense) │  │  (this repo)        │
  └──────────────┘  └─────────────────────┘
```

### Components

| Component | Directory | Description |
|-----------|-----------|-------------|
| **PS4/PS5 Payload** | `payload/` | ELF that runs on jailbroken console, creates virtual DualSense via VDA, listens on TCP 6967 |
| **ESP32 Bridge** | `esp32-ghostpad/` | ESP32-P4 firmware: WiFi, web UI, BLE HID host, USB HID, subnet scan, klog bridge |
| **VDA Probe** | `tools/vda_probe/` | Diagnostic tool to fingerprint libScePad VDA byte-pattern and enumerate MBus symbols |
| **PC GUI** | root | Python/Qt desktop app for keyboard/mouse/gamepad input (Windows/Linux) |

### How it works

The PS4/PS5 `scePadVirtualDeviceAddDevice` (VDA) normally requires a human user to confirm the virtual device through the shell UI. Ghostpad bypasses this with a **code-cave patch** of `libScePad.sprx` inside `SceShellCore`:

1. Find VDA's IPC dispatch call, redirect through a cave in the function's tail-padding
2. The cave calls the original dispatcher, pops the leftover return address, returns 0
3. The device is now real; the assignment screen is dismissed via MBus bind:
   ```
   sceMbusDisconnectDevice(physical) → sceMbusBindDeviceWithUserId(virtual, user)
   ```
4. Every 16-byte GPAD packet received over TCP is forwarded into `scePadVirtualDeviceInsertData`

Full technical write-up: **[virtualDS5research.md](virtualDS5research.md)**.

---

## Quick Start

### Payload (PS4/PS5)

```bash
cd payload
# PS4
make PS4_PAYLOAD_SDK=/path/to/ps4-payload-sdk
# PS5  
make PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
```

Deploy `ghostpad.elf` to the console (port 9021). Build flags:

| Flag | Default | Purpose |
|------|---------|---------|
| `GHOSTPAD_ENABLE_INTERNAL_KLOG` | 1 | In-payload /dev/klog reader + TCP bridge |
| `GHOSTPAD_KLOG_PORT` | 3434 | TCP port for klog streaming |
| `GHOSTPAD_ENABLE_RUNTIME_VDA_PATCH` | 1 | Patch libScePad VDA in self and SceShellCore |
| `GHOSTPAD_ENABLE_KLOG_AUTOBIND` | 0 | Auto-bind virtual device from klog DEVICE_ADDED events |

### ESP32 Bridge

```bash
cd esp32-ghostpad
# Copy and fill in your WiFi credentials
cp sdkconfig.private.example sdkconfig.private.defaults
# Edit sdkconfig.private.defaults with your SSID and password

source ~/esp/esp-idf/export.sh
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash
```

The ESP32 connects to your WiFi and serves a web UI at `http://ghostpad.local` (mDNS) or via its IP.

---

## ESP32 Bridge Features

### Web UI
- **Device Browser** — auto-discovers PS4/PS5 on the subnet, shows open ports (6967 GPAD, 3434 Klog, 9021 ELF loader)
- **Virtual Controller** — on-screen gamepad with keyboard bindings and touch support
- **Klog Viewer** — live streaming of console kernel log over WebSocket
- **BLE Scanner** — discover nearby DualSense/DS4 controllers with classification (model, score, appearance, HID flags)
- **Settings** — WiFi config, payload deploy, firmware info

### BLE HID Host
- Scans for DualSense (0x054C:0x0CE6), DualShock 4 (0x054C:0x09CC/0x05C4), Xbox, Nintendo, 8BitDo controllers
- 30-second active scan window with classification scoring
- Decodes DualSense report 0x01 and DualShock 4 report 0x11
- Stores backlog of klog data for late-connecting dashboard clients

### Connection Modes
| Mode | Input Source | Output | Use Case |
|------|-------------|--------|----------|
| **WiFi → PS4/PS5** | Web UI / Browser | TCP 6967 to console | Remote play over LAN |
| **BLE → PS4/PS5** | DualSense/DS4 via BLE | TCP 6967 to console | Wireless controller relay |
| **USB HID → PS5** | Web UI | TinyUSB HID to PS5 USB | Direct wired control |

### Networking
- WiFi STA (connect to existing network) with mDNS
- WiFi AP fallback (`Ghostpad-ESP32` / `ghostpad123`)
- Subnet scanner with port probing (6967, 3434, 9021, 9090)
- Klog TCP bridge (port 3434) with multi-client broadcast
- Private WiFi credentials via `sdkconfig.private.defaults` (gitignored)

---

## Wire Protocol

### `6967/tcp` — GPAD input stream

Fixed 16-byte packet:

| Offset | Type | Field |
|--------|------|-------|
| 0..3 | char[4] | `"GPAD"` magic |
| 4..7 | uint32 BE | Button bitmask |
| 8 | uint8 | Left stick X (0..255, 128 = center) |
| 9 | uint8 | Left stick Y |
| 10 | uint8 | Right stick X |
| 11 | uint8 | Right stick Y |
| 12 | uint8 | L2 analog (0..255) |
| 13 | uint8 | R2 analog |
| 14..15 | uint16 | Reserved |

### `6970/tcp` — Control / Diagnostic

| Magic | Payload | Effect |
|-------|---------|--------|
| `GBND` | uint32 userId + uint64 virtualDeviceId + uint64 physicalDeviceId | MBus disconnect + bind |
| `HVDI` | uint32 padHandle | Diagnostic VDI Cross press |
| `TYPE` | uint32 type (0=DS4, 3=DualSense) | Reconfigure VDA type |
| `DISC` | (empty) | Disconnect virtual device |

### WebSocket `/ws` — Web UI controller stream

Browser sends JSON frames at ~30 Hz:
```json
{"buttons": 0, "lx": 128, "ly": 128, "rx": 128, "ry": 128, "l2": 0, "r2": 0,
 "target_ip": "192.168.1.x", "seq": 1, "ts": 1717430000000}
```

ESP32 responds with status ACKs (every 30th frame):
```json
{"event": "state", "connected": true, "tx": 1500, "err": 0}
```

### HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | System status (WiFi, controller, scan results) |
| `/api/scan` | GET | Trigger subnet scan for consoles |
| `/api/ble/scan` | GET | BLE scan for controllers |
| `/api/ble/status` | GET | BLE connection state |
| `/api/ble/connect` | POST | Connect to BLE controller `{"addr":"xx:xx:xx:xx:xx:xx"}` |
| `/api/ble/disconnect` | POST | Disconnect BLE controller |
| `/api/controller/pulse` | GET | Diagnostic Cross press test `?ip=192.168.1.x` |
| `/api/settings/deploy` | POST | Deploy ELF payload to console |
| `/ws` | WS | WebSocket for controller streaming |
| `/ws/klog` | WS | WebSocket for klog streaming |

---

## VDA Probe (`tools/vda_probe`)

Diagnostic payload for reverse-engineering the VDA patch on PS4/PS5:

- Enumerates libScePad exports to find the VDA dispatch function
- Sweeps MBus symbols (20 functions) to map the binding API
- Reports instruction bytes for pattern-matching in `shellui_pad.c`

```bash
cd tools/vda_probe
make PS4_PAYLOAD_SDK=/path/to/sdk
# Deploy vda_probe.elf, check klog for results
```

---

## Keyboard Bindings (PC GUI)

| Key | Button | Key | Button |
|-----|--------|-----|--------|
| W A S D | D-pad | I J K L | Triangle / Square / Cross / Circle |
| U / O | L1 / R1 | Q / E | L2 / R2 |
| Enter | Options | Space | PS |
| BackSpace | Share/Create | T | Touchpad |

---

## Building

### Payload (PS4)

Requirements:
- [ps4-payload-sdk](https://github.com/ps4-payload-sdk) with LLVM toolchain
- `LLVM_CONFIG` pointing to a compatible `llvm-config`

```bash
cd payload
LLVM_CONFIG=/opt/homebrew/opt/llvm/bin/llvm-config \
  PS4_PAYLOAD_SDK=../external/ps4-payload-sdk \
  make
```

Output: `payload/ghostpad.elf`

### Payload (PS5)

```bash
cd payload
PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk make
```

### ESP32 Bridge

Requirements:
- [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/)
- Waveshare ESP32-P4-WIFI6 (with ESP32-C6 co-processor)

```bash
cd esp32-ghostpad
cp sdkconfig.private.example sdkconfig.private.defaults
# edit sdkconfig.private.defaults
source ~/esp/esp-idf/export.sh
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash
```

---

## Project Structure

```
payload/
├── main.c              # PS4/PS5 payload: VDA, klog, TCP server, GBND
├── shellui_pad.c/h     # SceShellCore injection, VDA patching, MBus helpers
└── Makefile

esp32-ghostpad/
├── main/
│   ├── main.c          # ESP32 entry point: WiFi, web server, USB HID
│   ├── wifi_ap.c/h     # WiFi STA/AP with mDNS
│   ├── web_server.c/h  # HTTP server, WebSocket, API endpoints
│   ├── web_gui.h       # Embedded HTML/JS controller UI
│   ├── ble_hid_host.c/h # BLE HID host: scan, connect, decode reports
│   ├── hid_gamepad.c/h # TinyUSB DualSense HID emulation
│   ├── klog_reader.c/h # /dev/klog TCP reader (legacy, for PS5 payload)
│   └── CMakeLists.txt
├── sdkconfig.defaults           # Public ESP-IDF config
├── sdkconfig.private.example    # Template for private WiFi credentials
├── sdkconfig.private.defaults   # Your WiFi credentials (gitignored)
└── CMakeLists.txt

tools/
└── vda_probe/
    └── vda_probe.c     # VDA pattern / MBus symbol diagnostic

git-patch/               # Patches applied during development
```

---

## Known Limitations

- **Firmware-specific byte patterns.** The VDA patch scans for specific instruction sequences in libScePad. Other firmware builds need the scan updated via `vda_probe`.
- **Single virtual device.** One virtual controller at a time.
- **No haptics / rumble feedback** from PS4/PS5 → client.
- **BLE-only on ESP32.** The ESP32-C6 co-processor is BLE-only; no Bluetooth Classic (BR/EDR).
- **DualSense BLE pairing** requires manual PS+Create button combo and has a narrow ~30s advertising window.

---

## Ghostpad Desktop App (Windows)

The **Ghostpad-app** (`Ghostpad-app/`) is an Electron + React desktop application for Windows that provides the most complete control experience:

- **Input Redirection** — on-screen virtual controller, XInput gamepad passthrough, keyboard bindings, mouse look
- **Macro Recording** — record and replay sequences of button presses, stick movements, and trigger pressure
- **Console Manager** — add, edit, and auto-scan PS4/PS5 consoles on LAN
- **System State** — reboot, shutdown, rest mode, and disc eject commands
- **Beeper & LED** — control PS5 beeper/LED (requires `beeper_server.elf`)
- **Auto-Deploy** — automatically deploy the payload ELF when connecting

### First-Time Setup (Desktop App)
1. Start the ELF loader on your PS4/PS5 (port 9021)
2. Open Ghostpad-app
3. Go to **Settings →** add your console IP
4. Click **Deploy Payload** (sends `ghostpad.elf`)
5. Click **Connect**

### Building the Desktop App
```bash
cd Ghostpad-app
npm install
npm run build:react
npm run start          # development
npm run compile        # package as Windows .exe
```

### Default Keyboard Bindings

| Key | Button | Key | Button |
|-----|--------|-----|--------|
| W A S D | D-pad | I J K L | Triangle / Square / Cross / Circle |
| U / O | L1 / R1 | Q / E | L2 / R2 |
| Enter | Options | Backspace | Share/Create |
| Space | PS button | T | Touchpad |

## Credits

### StonedModder
- Ghostpad-app (`Ghostpad-app/`): Electron + React desktop GUI for Windows with virtual controller, XInput passthrough, keyboard/mouse input, macro recording/playback, console manager, auto-deploy, system state controls, beeper/LED, and video capture overlay
- Original PS5 payload: VDA research, `scePadVirtualDeviceAddDevice` code-cave patch, `sceMbusBindDeviceWithUserId` / `sceMbusDisconnectDevice` MBus binding flow, credential elevation, process injection into SceShellCore
- hidDumper companion tool for reverse-engineering unknown controllers
- `virtualDS5research.md` technical write-up

### SeregonWar
- ESP32-P4 bridge firmware (`esp32-ghostpad/`): WiFi STA/AP with mDNS, embedded web UI, BLE HID host (NimBLE scan, connect, DualSense/DS4 report decoding), USB HID (TinyUSB), subnet scanner with port probing, klog TCP bridge with multi-client broadcast and backlog replay, WebSocket controller streaming, HTTP REST API, controller pulse diagnostics
- PS4 payload port (`__ORBIS__` target): SceShellCore injection (`shellui_pad.c`), runtime VDA patching with byte-pattern verification, libSceMbus dlopen/dlsym, VDA type handling (DualSense default, skip duplicate), PS4-specific authid/credential flow
- Payload-side klog architecture: always-on capture thread with client pool broadcasting, backlog ring buffer, thread-safe VDA candidate tracking with sequence numbers, GBND prebind loop, `ghostpad_try_klog_candidate_bind` direct MBus bind path
- VDA probe (`tools/vda_probe/`): diagnostic tool that fingerprints libScePad VDA byte-patterns and sweeps 20 MBus symbols
- klog parser improvements: multi-key DeviceId extraction, `GetUnassignedDeviceInfo` detection, VDA type:4/subType:2 RemotePlay recognition
- Native CPP GUI


## Acknowledgements

- `ps5-payload-sdk` / `ps4-payload-sdk` (John Törnblom et al.) — toolchain, kernel API helpers
- ESP-IDF — ESP32-P4 framework
- NimBLE — BLE host stack
- TinyUSB — USB HID device stack
- cJSON — JSON parsing/generation
- The PS4/PS5 dev community

## License

Payload C sources: GPL-3.0-or-later. ESP32 firmware: GPL-3.0-or-later. PC GUI scripts: MIT.
