# WiFi OTA App Download with MD5 Verification

This project is the application-side part of a WiFi OTA upgrade demo for a GD32F30x MCU. The App connects to an OTA server through a WiFi module, reports the current firmware version, checks for a new version, downloads the new firmware into a dedicated Flash download area, verifies it with MD5, and writes upgrade metadata for the Bootloader.

The companion Bootloader project is responsible for reading the upgrade metadata, copying the downloaded firmware into the App area, verifying it again, updating the stored version, clearing the upgrade flag, and jumping to the new App.

## Features

- WiFi module communication through AT commands over UART
- HTTP-based OTA version check and firmware download
- Firmware download in 512-byte chunks using HTTP `Range`
- MD5 verification after firmware download
- Persistent upgrade metadata stored in internal Flash
- User-confirmed upgrade flow using a key long press
- App-to-Bootloader handoff through Flash flags
- Parameter storage for current software version

## Project Structure

```text
22_wifi_ota_app_download_md5/
©À©¤©¤ App/
©¦   ©À©¤©¤ main.c          # App entry, driver init, WiFi task, key-confirmed upgrade
©¦   ©À©¤©¤ wifi_app.c      # WiFi/HTTP OTA state machine
©¦   ©À©¤©¤ wifi_app.h
©¦   ©À©¤©¤ store_app.c     # Version and OTA metadata storage
©¦   ©¸©¤©¤ store_app.h
©À©¤©¤ Driver/
©¦   ©À©¤©¤ wifi_drv.c      # WiFi UART/DMA driver
©¦   ©À©¤©¤ inflash_drv.c   # Internal Flash read/write/erase driver
©¦   ©À©¤©¤ iap_drv.c       # Reset back to Bootloader
©¦   ©À©¤©¤ md5.c           # MD5 implementation
©¦   ©¸©¤©¤ ...
©À©¤©¤ GD32_lib/           # GD32 peripheral library
©À©¤©¤ Arm_kernel/         # Startup file
©À©¤©¤ ARM.uvprojx         # Keil uVision project
©¸©¤©¤ Objects/ARM.sct     # Scatter file, App link address
```

## OTA Architecture

The system uses a two-stage OTA architecture:

```text
App:
  report version
  check update
  download firmware to Download area
  verify firmware MD5
  write UpdateInfo with flag = 0
  wait for user confirmation
  set update flag = 1
  reset to Bootloader

Bootloader:
  read UpdateInfo
  check magic, CRC, flag and firmware size
  copy firmware from Download area to App area
  verify App area MD5
  update stored firmware version
  clear UpdateInfo
  jump to App
```

The App does not overwrite itself while running. It only downloads the new firmware into a separate Flash region. The Bootloader performs the actual App replacement after reset.

## Flash Layout

The Flash layout is defined in `Driver/inflash_drv.h`.

```text
0x08000000  Bootloader area  28 KB
0x08007000  App area         240 KB
0x08043000  Download area    240 KB
0x0807F000  UpdateInfo area  2 KB
0x0807F800  Parameter area   2 KB
0x08080000  Flash end
```

Important notes:

- The App is linked at `0x08007000`.
- The Bootloader and App must use the same Flash layout.
- `Download area` stores the newly downloaded firmware.
- `UpdateInfo area` stores firmware size, MD5, target version and update flag.
- `Parameter area` stores persistent system parameters such as the current software version.

## Key Modules

### `wifi_app.c`

Implements the WiFi OTA state machine:

- Initializes the WiFi module with AT commands
- Connects to a WiFi hotspot
- Connects to the OTA server
- Reports the current firmware version
- Checks whether a new version is available
- Parses OTA metadata from the server response
- Downloads firmware by chunks
- Writes firmware data to the Flash download area
- Verifies the downloaded firmware MD5
- Calls `WriteUpdateInfo()` after a successful download

### `store_app.c`

Provides persistent storage APIs:

- `InitSysParam()` loads stored system parameters from Flash.
- `GetSoftwareVersionParam()` returns the current software version.
- `SetSoftwareVersionParam()` writes a new software version to Flash.
- `WriteUpdateInfo()` writes OTA metadata after a successful download.
- `ReadUpdateInfo()` reads and validates OTA metadata.
- `SetUpdateFlag()` sets the user-confirmed upgrade flag.
- `ClearUpdateInfo()` clears OTA metadata after upgrade.

### `main.c`

Initializes drivers, runs the WiFi OTA task, and handles the key long-press confirmation:

```c
WifiNetworkTask();

if (keyVal == KEY1_LONG_PRESS)
{
    if (SetUpdateFlag())
    {
        ResetToBoot();
    }
}
```

## Data Validation

This project uses different validation mechanisms for different data sizes:

- `magicCode`: marks whether a Flash region contains valid structured data.
- `CRC8`: checks small Flash-stored structures such as `SysParam_t` and `UpdateInfo_t`.
- `MD5`: checks the downloaded firmware image.

The App verifies the downloaded firmware once. The Bootloader should verify the firmware again after copying it to the App area.

## Build

This project is intended for Keil uVision / ARMCC.

1. Open `ARM.uvprojx` with Keil uVision.
2. Confirm the target MCU and GD32 library paths.
3. Confirm that `Objects/ARM.sct` links the App at `0x08007000`.
4. Build the project.
5. Flash the App image to the App area.

The Bootloader project must be built and flashed separately at `0x08000000`.

## Configuration Points

Before using the project on real hardware, update:

- WiFi SSID and password in `App/wifi_app.c`
- OTA server host, product ID, device name and authorization string
- Flash partition addresses in `Driver/inflash_drv.h`
- App link address in `Objects/ARM.sct`
- Firmware version string in the stored parameter flow

## Limitations

This is a learning/demo OTA implementation. For production use, consider adding:

- Power-loss protection during upgrade
- Rollback or A/B firmware partitioning
- Firmware digital signature verification
- Safer string length checks
- More robust HTTP response parsing
- Resume download support
- Watchdog integration during long Flash operations
- Secure storage of WiFi and cloud credentials

## Security Notice

The current source code contains demo WiFi and cloud authorization strings. Replace them before publishing or deploying this project. Avoid committing real WiFi passwords, cloud tokens, product secrets or device keys to a public repository.

## License

No license is currently specified. Add a license file before publishing if you want others to use, modify or redistribute this project.
