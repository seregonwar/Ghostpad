<img width="580" height="601" alt="icon" src="https://github.com/user-attachments/assets/cfdbc55a-60f9-498d-b166-8613a19ab3a0" />


# Ghostpad — PS5 Virtual DualSense over TCP



https://github.com/user-attachments/assets/30c7f384-4fa8-411c-aa07-84c59eda6037



> Remote-control a jailbroken PS5 from any PC, using keyboard, mouse, or a real controller — without needing PSN, Remote Play, or any official Sony software.
>
> Ghostpad-app is a Windows desktop app for controlling a jailbroken PS5 over your local network. It sends virtual DualSense input to the PS5 and includes macro recording, keyboard and mouse controls, gamepad passthrough, and optional beeper and LED tools.

Ghostpad is a payload + PC GUI that creates a **virtual DualSense** on a jailbroken PS5 and forwards keyboard, on screen, or scripted input to it over LAN. The shell and games see it as a real controller

```
+----------------+         TCP 6967 (input)        +-------------------+
|    PC GUI      |  ────────────────────────────►  |  PS5 (jailbroken) |
| ghostpad_gui   |  ◄─────────── klog ───────────  |  payload elf      |
+----------------+         TCP 3232 (debug)        +-------------------+
                                                          │
                                                          ▼
                                            virtual DualSense @ slot 0
                                            (no assignment screen)
```

<img width="1865" height="1463" alt="image" src="https://github.com/user-attachments/assets/a86de49b-49dc-4b8a-aae5-f8e6fe0759a4" />


## First time Setup

1. Start the ELF loader on your PS5.
2. Open Ghostpad.
3. Go to **Settings**.
4. Add or select your PS5 using its local IP address.
5. Confirm that the resolved payload path points to `ghostpad.elf`.
6. Click **Deploy Payload**.
7. Click **Connect**.

The app can deploy the payload automatically on future connections when **Auto-deploy payload when connecting** is enabled.

## Input Redirection

Open **Input Redirection** to control the PS5 in real time.

Available input methods:

- Use the on-screen controller.
- Connect an XInput-compatible controller.
- Use keyboard bindings.
- Enable mouse look for stick movement.
- Use the auto-clicker for repeated button presses.

Default keyboard bindings:

| Keys | Action |
|---|---|
| `W`, `A`, `S`, `D` | D-pad |
| `I`, `J`, `K`, `L` | Face buttons |
| `U`, `O` | `L1`, `R1` |
| `Q`, `E` | `L2`, `R2` |
| `Enter` | Options |
| `Backspace` | Create |
| `Space` | PS button |

Bindings can be changed from the **Input Redirection** screen.

## Macros

Open **Projects** to create and manage macro collections.

To record a macro:

1. Create or open a project.
2. Create or select a command.
3. Click the red record button.
4. Use the controller, keyboard, mouse, or on-screen controls.
5. Click stop when finished.
6. Click **Save**.

Use **Play** to run a macro once or **Repeat** to loop it. Recorded macros include button presses, stick movement, and analog trigger pressure.

Projects can be imported and exported as JSON files. Individual macros can also be exported as JSON or as a Python replay script.

## Saved Consoles

Open **Consoles** to add, edit, scan for, or remove PS5 consoles. Saved consoles can be selected from the other screens.

## Optional Tools

### Beeper and LED

The **Beeper & LED** screen controls the PS5 beeper and LED brightness. This requires firmware `8.0+` and the separate `beeper_server.elf` payload.

### System State

The **System State** screen can send reboot, shutdown, rest mode, and disc eject commands. It requires the separate `SystemStateManager.elf` payload.

## How it works

PS5's `scePadVirtualDeviceAddDevice` always returns `0x803B0006` ("assignment screen pending") when called from a payload — the system normally won't accept a virtual device until a human user confirms it through the shell UI. Ghostpad bypasses this with a **10-byte code-cave patch** of `libScePad.sprx` inside `SceShellCore`:

1. Find VDA's IPC dispatch call (5-byte `e8 …` immediately before the canary epilogue).
2. Redirect it through a cave in the function's tail-padding (`cc cc cc …`).
3. The cave calls the original dispatcher, `pop`s the leftover return address, `xor eax,eax`, and `jmp`s back into the canary epilogue.

VDA now returns 0, the libScePad write-handle table gets populated, and the device is real. The assignment screen still pops; we dismiss it programmatically by calling the same MBus pair the OS calls when a real user accepts a controller:

```
sceMbusDisconnectDevice(physical_device_id)
sceMbusBindDeviceWithUserId(virtual_device_id, foreground_user_id)
```

From here, every 16-byte GPAD packet that the PC sends over TCP `6967` is forwarded into `scePadVirtualDeviceInsertData(write_handle, &padData)` and the shell sees it as a real controller event.

Full technical write-up: **[`virtualDS5research.md`](virtualDS5research.md)**.

## Quick start

You need:
- A jailbroken PS5 with `elfldr` on port `9021`, `klog` on port `3232`, and `ps5-payload-sdk` runtime (HEN that grants `ptrace` + `kernel_set_vmem_protection`)
- WSL Ubuntu (or any Linux) with `/opt/ps5-payload-sdk` installed
- Python 3 on the PC

```bash
git clone https://github.com/<you>/ghostpad
cd ghostpad
echo "192.168.1.50" > ps5_ip.txt        # your PS5's LAN IP

# build payload (WSL) + deploy to PS5 + tail klog
python build_deploy_debug.py --wait-ready --timeout 90

# in another terminal: open the PC input GUI
python ghostpad_gui.py
```

Within ~20 seconds you should see in klog:
```
patch_vda: PATCHED
force_bind: sceMbusBindDeviceWithUserId(...) -> 0
Connected: 192.168.x.x
```

Click anything in the GUI — it fires on the PS5 home screen.

### Keyboard mapping (default)

| Key | Button | Key | Button |
|-----|--------|-----|--------|
| W A S D | D-pad | I J K L | Triangle/Square/Cross/Circle |
| U / O   | L1 / R1 | Q / E | L2 / R2 |
| Enter   | Options | Space | PS |
| BackSpace | Share/Create | T | Touchpad |

### Autoclicker

`Autoclicker` button in the top bar opens a per-button table:

| Column | Meaning |
|--------|---------|
| **On** | Toggle this clicker |
| **Period ms** | Total cycle = press+release |
| **Hold ms**   | Press duration each cycle |
| **Burst**     | Total clicks then auto-stop (0 = forever) |
| **Count**     | Clicks fired so far |

Use it for per-game max-rate tests and round-trip latency benchmarking (low-period burst → screen-capture → diff timestamps).


## Wire protocol

### `6967/tcp` — GPAD input stream (PC → PS5)
Fixed 16-byte `!4sIBBBBBB2s` packet:

| Offset | Type | Field |
|--------|------|-------|
| 0..3   | char[4] | `"GPAD"` magic |
| 4..7   | uint32 BE | button bitmask |
| 8      | uint8 | LX (0..255, 128 = center) |
| 9      | uint8 | LY |
| 10     | uint8 | RX |
| 11     | uint8 | RY |
| 12     | uint8 | L2 analog (0..255) |
| 13     | uint8 | R2 analog |
| 14..15 | uint16 | reserved (0) |

### `6970/tcp` — control / diagnostic

| Magic | Payload | Effect |
|-------|---------|--------|
| `GBND` | uint32 userId + uint64 virtualDeviceId + uint64 physicalDeviceId | Run MBus disconnect + bind sequence |
| `HVDI` | uint32 padHandle | Diagnostic VDI write of a Cross press, log return code |
| `TYPE` | uint32 type (0=DS4, 3=DS5) | Reconfigure subsequent VDA calls |
| `DISC` | (empty) | Disconnect virtual device |

## hidDumper (companion tool)

Standalone payload for reverse-engineering controllers Ghostpad doesn't yet support:

1. Plug the unknown controller into the PS5.
2. `cd hidDumper && make`
3. `python ../build_deploy_debug.py --skip-build --elf hidDumper/hiddumper.elf --wait-ready`
4. Tail klog, press every button on the real controller, note which bits in `ScePadData.buttons` toggle.

Output (live, change-driven + 1s heartbeat):
```
[hidDumper] -- slot=0 h=0x3410300 ts=3034483552 --
[hidDumper] conn=1 btns=0x00004000 LS=124,122 RS=128,120 L2=  0 R2=  0
[hidDumper] gyro -0.003 0.973 0.170  accel 0.002 0.010 0.000
[hidDumper] hex +00 00 40 00 00 7c 7a 80 78 ...
```

Architecture: PT_ATTACH `SceRemotePlay`, `pt_call scePadOpen(userId, 0, 0)` to grab a real read handle inside that process, then `pt_call scePadReadState` in a 5 Hz loop with `pt_io_read` to copy the struct back to klog. Why SceRemotePlay? It's the only payload-attachable process that returns a positive `scePadOpen` handle — `SceShellUI` is authid-protected, `SceShellCore` and others always return `0x80920008` for both `Open` and `GetHandle`. Details in `virtualDS5research.md` → "Reading Live Pad Data".

## Known limitations

- **Firmware-specific byte patterns.** The VDA patch scans for `e8 ?? ?? ?? ?? 48 8b 0b 48 3b 4d f0 75 ??`. Other libScePad builds will need the scan updated.
- **Single virtual device.** One virtual DualSense at a time (PS5 typically binds it to slot 0).
- **No touchpad coordinates.** Touchpad button click works; XY motion isn't wired through yet.
- **No haptics / rumble feedback** from PS5 → PC.
- **mdbg_copyin into SceShellCore fails** (errno=1) — that's why VDI uses `pt_call_with_copy` instead of a shared-memory update path.


## Acknowledgements

- `ps5-payload-sdk` (John Törnblom et al.) — toolchain, `prospero-clang`, kernel API helpers
- `chronicLoader` — ELF build/deploy patterns
- etaHEN — ptrace-injector patterns (etaHEN itself does not implement virtual controllers)
- `shadps4-emu/shadPS4` — pad.h reference for the ScePad enum
- The PS5 dev community on the various scene wikis — for the firmware-version map and the pointer to which `0x803B0006` actually meant

## License

GPL-3.0-or-later for the payload C sources. PC-side Python scripts MIT.
