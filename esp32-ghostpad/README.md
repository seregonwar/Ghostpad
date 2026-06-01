# Ghostpad Bridge - ESP32-P4-WIFI6

Trasforma il Waveshare ESP32-P4-WIFI6 in un bridge per il controllo remoto della PS5.

## Architettura

```
Telefono (Browser) ---WiFi---> ESP32-P4 ---USB HID---> PS5
                                    |
                          Ghostpad Payload (modificato)
                          legge /dev/klog direttamente
```

- **ESP32-P4**: Crea un AP WiFi, serve un'interfaccia web con joystick virtuali, traduce l'input in report HID DualSense via USB TinyUSB
- **PS5**: Il Ghostpad payload modificato legge /dev/klog direttamente (senza klog server TCP separato) usando la logica estratta da `klog_reader.c`

## Requisiti

- [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/)
- Waveshare ESP32-P4-WIFI6 board
- USB-C <=> USB-A per collegamento alla PS5

## Setup

```bash
# Clona ESP-IDF (se non già presente)
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32p4
source export.sh

# Build del firmware
cd /path/to/esp32-ghostpad
idf.py set-target esp32p4
idf.py menuconfig  # configurazioni opzionali
idf.py build

# Flash
idf.py -p /dev/ttyACM0 flash monitor
```

## Connessione

1. La scheda crea l'AP WiFi: **Ghostpad-ESP32** / **ghostpad123**
2. Collega il telefono all'AP
3. Apri http://192.168.4.1 nel browser
4. Collega ESP32 alla PS5 via USB
5. Usa l'interfaccia web per controllare la PS5

### Connessione PS4/PS5 alla rete ESP32

Se colleghi direttamente la console all'AP **Ghostpad-ESP32**, nella procedura rete della PlayStation lascia il proxy su **Non usare**.

L'ESP32 pubblica `192.168.4.1` come gateway/DNS locale per rendere stabile la rete privata, ma non fornisce accesso Internet e non e' un server proxy HTTP. Inserire `192.168.4.1` nel campo proxy fa comparire la console nella lista dei client WiFi, ma il test della PlayStation fallisce e puo' riportarti alla schermata del proxy.

Configurazione consigliata:

1. IP: automatico
2. DHCP Host Name: non specificare
3. DNS: automatico
4. MTU: automatico
5. Proxy: **Non usare**

Il test Internet puo' risultare non riuscito se l'ESP32 non ha upstream, ma la rete locale resta valida per Ghostpad.

## Modifica Ghostpad Payload (PS5)

Per integrare la lettura diretta di `/dev/klog` nel payload Ghostpad:

1. Copia `klog_reader.h` e `klog_reader.c` nella directory `payload/`
2. Aggiungi `klog_reader.c` nel `Makefile` (riga `SRC`)
3. Avvia un thread con `klog_reader_start("/dev/klog", 3232)`
4. Il payload ora serve i log internamente senza necessità di `klogsrv.elf`

## Struttura file

```
main/
├── CMakeLists.txt
├── main.c            # Entry point: init WiFi, web, USB HID
├── wifi_ap.c/h       # Configurazione WiFi Access Point
├── web_server.c/h    # HTTP server + WebSocket per GUI
├── web_gui.h         # Interfaccia controller (HTML/JS embedded)
├── hid_gamepad.c/h   # Emulazione USB HID DualSense via TinyUSB
├── klog_reader.c/h   # Logica lettura /dev/klog (per PS5)
CMakeLists.txt
sdkconfig
```
