# Ghostpad Bridge - ESP32-WROOM-32U

Turns the ESP32-WROOM-32U into a bridge for PS5 remote control.

## Architecture

```
Phone (Browser) ---WiFi---> ESP32-WROOM-32U ---USB HID---> PS5
                                    |
                          Ghostpad Payload (modified)
                          reads /dev/klog directly
```

- **ESP32-WROOM-32U**: Creates a WiFi AP, serves a web interface with virtual joysticks, translates input into DualSense HID reports via USB TinyUSB
- **PS5**: The modified Ghostpad payload reads /dev/klog directly (without a separate klog TCP server) using logic extracted from `klog_reader.c`

## Requirements

- [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- ESP32-WROOM-32U board
- USB-C <=> USB-A cable for connecting to the PS5

## Setup

```bash
# Clone ESP-IDF (if not already present)
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
source export.sh

# Build firmware
cd /path/to/esp32-ghostpad
idf.py set-target esp32
idf.py menuconfig  # optional configuration
idf.py build

# Flash
idf.py -p /dev/ttyACM0 flash monitor
```

## Connection

1. The board creates the WiFi AP: **Ghostpad-ESP32** / **ghostpad123**
2. Connect your phone to the AP
3. Open http://192.168.4.1 in the browser
4. Connect ESP32 to PS5 via USB
5. Use the web interface to control the PS5

### Connecting PS4/PS5 to the ESP32 network

If you connect the console directly to the **Ghostpad-ESP32** AP, leave the proxy set to **Do Not Use** in the PlayStation network setup.

The ESP32 advertises `192.168.4.1` as the gateway/local DNS to make the private network stable, but it does not provide Internet access and is not an HTTP proxy server. Entering `192.168.4.1` in the proxy field makes the console appear in the WiFi client list, but the PlayStation connectivity test fails and may send you back to the proxy screen.

Recommended configuration:

1. IP: automatic
2. DHCP Host Name: do not specify
3. DNS: automatic
4. MTU: automatic
5. Proxy: **Do Not Use**

The Internet test may fail if the ESP32 has no upstream, but the local network remains valid for Ghostpad.

## Modifying the Ghostpad Payload (PS5)

To integrate direct `/dev/klog` reading into the Ghostpad payload:

1. Copy `klog_reader.h` and `klog_reader.c` into the `payload/` directory
2. Add `klog_reader.c` to the `Makefile` (`SRC` line)
3. Start a thread with `klog_reader_start("/dev/klog", 3232)`
4. The payload now serves logs internally without needing `klogsrv.elf`

## File structure

```
main/
├── CMakeLists.txt
├── main.c            # Entry point: init WiFi, web, USB HID
├── wifi_ap.c/h       # WiFi Access Point configuration
├── web_server.c/h    # HTTP server + WebSocket for GUI
├── web_gui.h         # Controller interface (HTML/JS embedded)
├── hid_gamepad.c/h   # DualSense USB HID emulation via TinyUSB
├── klog_reader.c/h   # /dev/klog reading logic (for PS5)
CMakeLists.txt
sdkconfig
```
