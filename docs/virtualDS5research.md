# PS5 Virtual DualSense Controller — generated with claude 

**Status:** working end-to-end. Controller registers, assignment screen auto-dismisses, PC input flows to PS5 shell. Button bitmap empirically confirmed (see Button Bitmap section).

This document captures the complete flow for creating a virtual DualSense controller on a jailbroken PS5 that accepts input from a remote PC over TCP. 

## Quick Start (TL;DR)

```bash
git clone <this-repo>
cd ghostpad
echo "192.168.1.50" > ps5_ip.txt          # your PS5's LAN IP
python build_deploy_debug.py --skip-deploy --timeout 1     # build via WSL
python build_deploy_debug.py --skip-build --wait-ready --timeout 90  # deploy + monitor
python ghostpad_gui.py                     # open input GUI on the PC
```

Within ~20 seconds you should see in klog:
- `Injection OK pid=57 args=…`
- `patch_vda: PATCHED`
- `force_bind: sceMbusBindDeviceWithUserId(…, …) -> 0`
- `Connected: 192.168.x.x`     (your PC connecting from the GUI)

Click buttons in the GUI → they fire on the PS5 home screen.

---

## Summary of the Technique

1. Load payload via `elfldr` on port `9021`.
2. Payload self-elevates ucred (`kernel_set_ucred_authid`), calls `scePadInit`, then PT_ATTACHes `SceShellCore` (the pad daemon).
3. **Patch libScePad.sprx in SceShellCore**: redirect `scePadVirtualDeviceAddDevice`'s IPC-dispatch call through a code cave that zeros `eax` on return — bypasses the `0x803b0006` "assignment screen pending" sentinel that PS5's pad daemon normally returns when a virtual device is created.
4. Inject a small thread into SceShellCore that calls the (patched) `scePadVirtualDeviceAddDevice` → returns success — populates the libScePad **VDI write-handle table** for the new virtual device.
5. PS5 still raises the **assignment screen** (LoginMgr UI). Auto-dismiss via two MBus calls (resolved from `libSceMbus` inside SceShellUI) — replicates the exact sequence the system runs when a real DualSense user presses Cross:
   - `sceMbusDisconnectDevice(physicalDeviceId)`   — evict the currently-bound physical pad
   - `sceMbusBindDeviceWithUserId(virtualDeviceId, userId)` — bind the virtual pad
6. Payload then accepts pad-state TCP packets on port `6967` and forwards each via `scePadVirtualDeviceInsertData(write_handle, padData)` (called via `pt_call_with_copy` inside SceShellUI's context, since VDI write happens to a shared-memory ring buffer indexed by the libScePad write-handle).
7. Python GUI on PC connects to `6967` and streams a 16-byte packet at every input event.

The PS5 sees the virtual device as a fully-assigned DualSense at CIM slot 0.

---

## Required Environment

- **Target firmware**: tested on PS5 hosting `libScePad.sprx` with the layout described below (system version specific). Other firmware revs need the byte-pattern scan to match.
- **HEN**: any HEN that grants `ptrace` + `kernel_set_vmem_protection` + `mdbg_copyin`/`mdbg_copyout` (we used the standard `ps5-payload-sdk` runtime).
- **Tools**: WSL Ubuntu with `ps5-payload-sdk`; Python 3 on the host PC; PS5 reachable on the LAN.

---

## Key Architecture Decisions

| Decision | Reason |
|----------|--------|
| Inject into `SceShellCore` (not `SceShellUI` or `SceRemotePlay`) | SceShellCore IS the pad daemon. Running a thread there makes `scePadVirtualDeviceAddDevice` execute against the daemon's own libScePad without IPC validation — the dispatch call we have to bypass. |
| Patch libScePad's VDA dispatch instead of skipping it | The dispatch sub-fn actually creates the virtual device in mbus. We need it to run and then return 0 instead of `0x803b0006`. Skipping the call → no device. |
| Use a code cave at `+0x115` (libScePad's `cc` padding) | Avoids relocating instructions inside VDA. The cave does: `call original_dispatcher`, `pop rax` (discard pushed retaddr), `xor eax, eax`, `jmp` back to the canary epilogue. |
| Auto-dismiss via `sceMbusDisconnectDevice` + `sceMbusBindDeviceWithUserId` | Exactly the same pair the system calls when a real user presses Cross on the assignment screen. Both are in `libSceMbus`, available in SceShellUI's address space. |
| Two TCP ports (6967 GPAD input, 6970 GBND/HVDI control) | Separates data plane (60 Hz pad packets from PC GUI) from control plane (Python automation that watches klog, then issues bind + VDI test commands). |

---

## Build & Deploy

```bash
# build (WSL Ubuntu with /opt/ps5-payload-sdk)
python build_deploy_debug.py --skip-deploy --timeout 1

# deploy and tail klog (PS5 IP from ps5_ip.txt)
python build_deploy_debug.py --skip-build --wait-ready --timeout 90

# launch the PC-side input GUI
python ghostpad_gui.py
```

The Python `build_deploy_debug.py` driver:
- builds the payload via WSL
- sends the ELF to `9021` (payload loader)
- tails klog from `3232`
- watches for `DEVICE_ADDED` of the virtual device and **automatically** sends a GBND command to port `6970` to fire the eviction + bind sequence

---

## File Layout

```
ghostpad/
├── build_deploy_debug.py       # WSL build + deploy + klog monitor + GBND/HVDI automation
├── ghostpad_gui.py             # PC-side input GUI; sends 16-byte GPAD packets to PS5:6967
├── ghostpad_client.py          # Headless client for scripted single-button tests
├── ps5_probe.py                # Tiny TCP reachability checker
├── ps5_ip.txt                  # PS5 IP address
└── payloadExamples/ghostpadOGpartial/payload/
    ├── main.c                  # Payload entry, ucred elevation, TCP servers, GBND/HVDI handlers
    ├── shellui_pad.c           # SceShellCore injection + VDA patch + MBus calls + VDI
    ├── shellui_pad.h           # Shared ShellUiPadArgs struct + public API
    └── Makefile                # prospero-clang build rules
```

---

## Network Protocol

### Port `6967` — GPAD input stream (PC → PS5)

Fixed 16-byte packet `!4sIBBBBBB2s` (network byte order):

```
0..3   "GPAD" magic
4..7   uint32 buttons   (button bitmask — see Button Bitmap section)
8      uint8  lx        (left stick X, 0..255, 128 = center)
9      uint8  ly        (left stick Y, 0..255, 128 = center)
10     uint8  rx        (right stick X)
11     uint8  ry        (right stick Y)
12     uint8  l2        (L2 analog 0..255)
13     uint8  r2        (R2 analog 0..255)
14..15 2 bytes reserved (must be 0)
```

### Port `6970` — Control/diagnostic (Python automation → payload)

#### GBND — bind virtual device to user
```
0..3   "GBND" magic
4..7   uint32  userId         (0 = use payload's injectUserId)
8..15  uint64  virtualDeviceId
16..23 uint64  physicalDeviceId (0 = skip physical eviction)
```
Triggers `sceMbusDisconnectDevice(physicalDeviceId)` then `sceMbusBindDeviceWithUserId(virtualDeviceId, userId)`.

#### HVDI — diagnostic VDI test
```
0..3   "HVDI" magic
4..7   uint32 padHandle
```
Calls `scePadVirtualDeviceInsertData(padHandle, cross_data)` from both SceShellUI and SceShellCore contexts and logs return codes. Used during research to confirm VDI works after the VDA patch.

---

## libScePad VDA Patch (the critical hack)

`scePadVirtualDeviceAddDevice` in libScePad.sprx, decompiled prologue + dispatch:

```asm
00:  55 48 89 e5            ; push rbp ; mov rbp, rsp
04:  53 48 81 ec d8 00 00 00; push rbx ; sub rsp, 0xd8
0c:  48 8b 1d e5 18 01 00   ; mov rbx, [rip+__stack_chk_guard]
13:  48 8b 03 48 89 45 f0   ; canary save to [rbp-0x10]
1a:  b8 05 00 92 80         ; mov eax, 0x80920005 (NOT_INIT default)
1f:  83 3d d2 58 01 00 00   ; cmp dword [rip+init_flag], 0
26:  0f 84 cf 00 00 00      ; je epilogue (NOT_INIT)
... validation chain (sets eax to 0x80920001 INVALID_ARG default,
    je on null rcx/rsi to epilogue) ...
e8:  be 2a 48 18 c0         ; mov esi, 0xC018482A   (IPC msg ID)
ed:  31 c0                  ; xor eax, eax
ef:  e8 65 b3 00 00         ; call ipc_dispatch     ← THIS returns 0x803b0006
f4:  48 8b 0b               ; mov rcx, [rbx]        ← canary epilogue starts
f7:  48 3b 4d f0            ; cmp rcx, [rbp-0x10]
fb:  75 0a                  ; jne __stack_chk_fail
fd:  48 81 c4 d8 00 00 00   ; add rsp, 0xd8
104: 5b 5d c3               ; pop rbx ; pop rbp ; ret
... __stack_chk_fail call ...
113: 0f 0b                  ; ud2
115: cc cc cc cc cc cc cc cc cc cc cc ; 11 bytes of int3 padding ← CODE CAVE
```

### Patch strategy

1. **Code cave at +0x115** (10 bytes overwrite of int3 padding):
   ```
   e8 <rel32 → ipc_dispatch>   ; original IPC dispatcher
   58                          ; pop rax — discard the +0xfb retaddr the outer call pushed
   31 c0                       ; xor eax, eax — force success return
   eb <rel8 → +0xfb>           ; jmp short back into the canary epilogue
   ```

2. **Redirect call site at +0xef** (5 bytes overwrite):
   ```
   e8 <rel32 → cave +0x115>
   ```

### Why this works

- The IPC dispatcher still runs (device is registered in mbus, libScePad VDI table populated with the new write handle).
- `eax` is zeroed before the canary check, so VDA returns `0` to its caller.
- The injector's stub sees `vda_ret >= 0` and proceeds.
- Critically, the early validation errors (`0x80920005`, `0x80920001`) still flow through the original `+0xfb` epilogue with their error codes intact — only the post-IPC path gets the override.

The cave uses `pop rax` because the outer `call cave` pushes the post-call return address (`+0xfb`) before jumping to the cave. The cave's `call dispatcher` then pushes its own retaddr; when the dispatcher returns, the cave is back where it expects. We `pop rax` to discard the leftover `+0xfb` from the outer call, then `xor` and `jmp` back to it.

---

## MBus Auto-Dismiss

After VDA returns success and DEVICE_ADDED fires, the PS5 still shows the assignment screen (`PfLogin.UserSelectionScreen`). To dismiss it programmatically we replicate exactly what the LoginMgr does when a user presses Cross on a real new controller:

```c
PT_ATTACH(SceShellUI)
resolve sceMbusDisconnectDevice from libSceMbus
resolve sceMbusBindDeviceWithUserId from libSceMbus
pt_call sceMbusDisconnectDevice(physicalDeviceId)         // 0 on success
pt_call sceMbusBindDeviceWithUserId(virtualDeviceId, userId)  // 0 on success
PT_DETACH(SceShellUI)
```

`DEVICE_OWNER_CHANGED` fires, CIM updates, virtual device is now at slot 0 (or whatever slot the physical occupied).

---

## Reading Live Pad Data (hidDumper architecture, empirically confirmed)

For the inverse path — observing what the PS5 driver presents to a process after it normalizes a physical controller's HID report — Ghostpad ships a companion `hidDumper/` payload. Findings from live klog runs on 2026-05-23 fw:

### Process selection
PT_ATTACH-able processes that have `libScePad` loaded and respond to `scePadOpen`:

| Process | PT_ATTACH | `scePadGetHandle` | `scePadOpen(uid, 0, 0)` |
|---------|-----------|------------------|-------------------------|
| `SceShellUI`     | ✗ EINVAL (authid 0x4800...) | — | — |
| `SceShellCore`   | ✓ | `0x80920008` for every (uid,type,idx) | typically refuses |
| **`SceRemotePlay`** | ✓ | `0x80920008` for every (uid,type,idx) | **✓ returns positive handle (e.g. `0x03410300`) for type=0 idx=0** |
| `SceVideoCoreServer` | ✓ | n/a — no libScePad client | — |

So `SceRemotePlay` is the canonical attach target for reading pad data via injection. `scePadGetHandle` **does not work from any payload-attachable process** — only `scePadOpen` allocates a real handle.

### Call sequence
```c
sys_ptrace(PT_ATTACH, pid_SceRemotePlay, 0, 0);
waitpid(pid, NULL, 0);
get_lib(pid, "libScePad",    &libpad_h);
get_lib(pid, "libkernel_sys",&libkernel_h);
fn_open      = resolve_sym(pid, libpad_h,    "scePadOpen");
fn_readstate = resolve_sym(pid, libpad_h,    "scePadReadState");
fn_mmap      = resolve_sym(pid, libkernel_h, "mmap");

// scratch = first byte of libScePad fini section, made RWX + 0xCC for trap
pad_buf = pt_call(pid, fn_mmap, scratch, 0, 4096, PROT_RW, MAP_ANON|MAP_PRIVATE, -1, 0);
scePadSetProcessPrivilege(1);   // via pt_call
handle  = pt_call(pid, fn_open, scratch, userId, /*type*/0, /*idx*/0, 0,0,0);

// Poll loop
while (1) {
    pt_call(pid, fn_readstate, scratch, handle, pad_buf, 0,0,0,0);
    pt_io_read(pid, pad_buf, &local_padData, sizeof(local_padData));
    klog_printf( ... local_padData ... );
    usleep(200000); // 5 Hz keeps the system responsive
}
```

### Live klog excerpt (idle real DualSense, no buttons pressed)
```
[hidDumper] attached SceRemotePlay pid=60 authid=0x4800000000000019
[hidDumper] Open(uid=0x18c60ea1, type=0, idx=0) -> 0x3410300
[hidDumper] -- slot=0 h=0x3410300 ts=3034483552 --
[hidDumper] conn=1 btns=0x00000000 LS=124,122 RS=128,120 L2=  0 R2=  0
[hidDumper] gyro -0.003 0.973 0.170  accel 0.002 0.010 0.000
```
- Sticks not perfectly centered at 128 — that's hardware deadzone, exactly what an unfiltered game read would see.
- `gyro.y ≈ 0.97` matches 1g of gravity on the Y axis when the pad lies flat.
- `ts` is a monotonic 64-bit timestamp in microseconds.

### Why not `scePadGetHandle`?
`scePadGetHandle` returns `0x80920008 SCE_PAD_ERROR_NO_HANDLE` from every process we can PT_ATTACH. The handle table is keyed to the process that successfully called `scePadOpen` (the registered pad-client). `scePadOpen` from inside `SceRemotePlay` works because the pad service trusts it as a pad-using process. Other system processes that ARE registered (`SceShellUI`, the foreground game) cannot be PT_ATTACHed.

---

## VDI Input Path

Once the virtual device is bound, every 60 Hz GPAD packet that arrives at port `6967` is repacked into a `ScePadData` struct and inserted via:

```c
scePadVirtualDeviceInsertData(write_handle, &padData);
```

The `write_handle` is the value VDA returned in our patched flow. The call writes pad state into a shared-memory ring buffer that the PS5 input pipeline polls — the shell or game sees the bytes as if they came from a real DualSense.

The call is made via `pt_call_with_copy` while PT_ATTACHed to the target process (SceShellUI or SceShellCore). VDI is **table-indexed**, not pointer-dereferenced — it safely returns `0x803b0003 SCE_PAD_ERROR_INVALID_HANDLE` for any unknown handle without crashing, which made byte sweeps safe during research.

---

## Button Bitmap — PS5 Virtual DualSense (empirically confirmed)

```
typedef struct {
    uint32_t buttons;            // offset 0  — bitmask, see below
    uint8_t  leftStickX;         // offset 4  — 0..255, 128 = center
    uint8_t  leftStickY;         // offset 5  — 0..255, 128 = center
    uint8_t  rightStickX;        // offset 6
    uint8_t  rightStickY;        // offset 7
    uint8_t  analogL2;           // offset 8  — 0..255 (REQUIRED for L2 to register)
    uint8_t  analogR2;           // offset 9  — 0..255 (REQUIRED for R2 to register)
    uint16_t padding;            // offset 10
    float    quatX, quatY, quatZ, quatW;  // offset 12..27
    ...
    uint8_t  connected;          // offset 76 — set to 1 every frame
    ...
} ScePadData;
```

### Confirmed working bits in `padData.buttons`:

| Bit | Mask | Button | Status |
|-----|------|--------|--------|
| 1   | 0x00000002 | L3 | ✓ confirmed |
| 2   | 0x00000004 | R3 | ✓ confirmed |
| 3   | 0x00000008 | Options | ✓ confirmed |
| 4   | 0x00000010 | Up | ✓ confirmed |
| 5   | 0x00000020 | Right | ✓ confirmed |
| 6   | 0x00000040 | Down | ✓ confirmed |
| 7   | 0x00000080 | Left | ✓ confirmed |
| 8   | 0x00000100 | L2 (digital) | needs analog L2 byte > 0 to register on PS5 |
| 9   | 0x00000200 | R2 (digital) | needs analog R2 byte > 0 to register on PS5 |
| 10  | 0x00000400 | L1 | ✓ confirmed |
| 11  | 0x00000800 | R1 | ✓ confirmed |
| 12  | 0x00001000 | Triangle | ✓ confirmed |
| 13  | 0x00002000 | Circle | ✓ confirmed |
| 14  | 0x00004000 | Cross | ✓ confirmed |
| 15  | 0x00008000 | Square | ✓ confirmed |
| 16  | 0x00010000 | **PS button** | ✓ confirmed (PS5-specific; NOT "Create"/"Share" as PS4 SDK header suggests) |
| 17  | 0x00020000 | reserved/invalid | sending this bit produced an unintended Cross press in PS5 shell |
| 20  | 0x00100000 | Touchpad | ✓ confirmed |

### Bits not yet mapped:
- 0x00040000, 0x00080000, 0x00200000+ — likely Create/Share, Mute, etc., to be confirmed
- 0x80000000 — "Intercepted" flag (per PS4 SDK), not used for virtual input

### L2 / R2 specifics:
- The digital bits (0x100, 0x200) alone are **not sufficient** — PS5 looks at the analog bytes at offset 8 (L2) and offset 9 (R2). Setting both the bit AND analog=255 gives the most consistent press.
- For pressure-sensitive games, ramp analog from 0..255 instead of binary.

### Verified Ghostpad GUI mappings (final):
| GUI label | Bitmask | Notes |
|-----------|---------|-------|
| L3 / R3 | 0x2 / 0x4 | stick click |
| Options | 0x8 | |
| D-pad Up/Right/Down/Left | 0x10 / 0x20 / 0x40 / 0x80 | |
| L2 / R2 | 0x100 / 0x200 + analog=255 | GUI also auto-sets the analog byte when held |
| L1 / R1 | 0x400 / 0x800 | |
| Triangle / Circle / Cross / Square | 0x1000 / 0x2000 / 0x4000 / 0x8000 | |
| PS | 0x10000 | bit 16, NOT "Create" |
| Touchpad | 0x100000 | tap; touchpad coordinates not implemented yet |

### Removed (do not use):
- bit 0x00020000 — the original GUI mapped this to "PS" based on assumed PS4 enum order, but the PS5 virtual device interprets it as Cross (or simply garbage). The Ghostpad GUI no longer exposes this bit.

---

## Success Criteria (current status)

| Step | Status | Notes |
|------|--------|-------|
| 1. Payload loads on PS5 | ✓ | klog shows `Ghostpad v1.0 Starting` |
| 2. SceShellCore injection succeeds | ✓ | `attached to SceShellCore … pthread_create -> 0` |
| 3. VDA patch applied | ✓ | `patch_vda: PATCHED` |
| 4. VDA returns success (write handle obtained) | ✓ | `stub ready=1` |
| 5. DEVICE_ADDED for virtual device | ✓ | `processMbusEvent() event=DEVICE_ADDED deviceId=0xXXXXX deviceType=22 capabilityBattery:0` |
| 6. Auto-evict physical + bind virtual | ✓ | `sceMbusDisconnectDevice` + `sceMbusBindDeviceWithUserId` both return 0; DEVICE_OWNER_CHANGED fires |
| 7. CIM shows virtual at slot 0 | ✓ | `[CIM] [0] deviceId userId 1 22 padHandle 0x00000000 0` |
| 8. VDI delivers Cross test packet | ✓ | First Cross dismisses any residual UI; shell-side input fires |
| 9. PC GUI streams pad state at 60 Hz | ✓ | GPAD packets land in main accept loop |
| 10. Stick + button bitmap fully correct | ⏳ | being mapped empirically |

---

## Known Limitations / Future Work

- **Per-firmware byte patterns**: the VDA patch scans for `e8 ?? ?? ?? ?? 48 8b 0b 48 3b 4d f0 75 ??` (call + canary epilogue). Other libScePad builds will have different offsets and might compile differently — the scan must adapt.
- **Restart needed if patch destabilizes**: a previous experimental patch crashed SceShellCore. A clean PS5 reboot restores libScePad. The current patch is non-fatal (verified across multiple deploys in the same session).
- **mdbg_copyin to SceShellCore heap fails (errno=1)**: that's why the architecture uses `pt_call_with_copy` for VDI instead of writing the packet through `mdbg_copyin` and having the stub poll a sequence number.
- **Button bitmap finalization**: the PS5 DualSense layout for `PS` and `Create`/`Share` is not yet matched empirically — currently in active mapping.

---

## Acknowledgements / References

- `payloadExamples/chronicLoader` — for ELF build and deploy patterns
- `shadps4-emu/shadPS4` — pad.h reference for standard PS4 ScePad enums
- etaHEN — for ptrace-based injector patterns (note: etaHEN itself does **not** implement virtual controllers; only standard scePad client API)
- `ps5-payload-sdk` toolchain — Makefile templates, `prospero-clang`, kernel API headers
