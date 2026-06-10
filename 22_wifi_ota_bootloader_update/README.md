# WiFi OTA Bootloader Update with MD5 Verification

This project is the Bootloader-side part of a WiFi OTA upgrade demo for a GD32F30x MCU. The Bootloader starts from the beginning of internal Flash, reads the upgrade metadata written by the companion App, checks whether the user has confirmed the upgrade, copies the downloaded firmware from the Download area into the App area, verifies it with MD5, updates the stored firmware version, clears the upgrade metadata, and jumps to the App.

The companion App project is responsible for connecting to the OTA server through a WiFi module, reporting the current firmware version, checking for a new version, downloading the new firmware into the dedicated Flash download area, verifying the downloaded firmware, and setting the upgrade flag before resetting back to the Bootloader.

## Features

- Bootloader-to-App jump from the fixed Bootloader Flash region
- App vector table validation before jumping
- Upgrade metadata validation using magic code and CRC8
- User-confirmed OTA upgrade flow through a Flash flag
- Firmware copy from Download area to App area in 512-byte chunks
- MD5 verification after writing the firmware into the App area
- Persistent software version update after a successful upgrade
- Shared Flash layout with the companion App project

## Project Structure

```text
22_wifi_ota_bootloader_update/
+-- App/
|   +-- main.c          # Bootloader entry, driver init, update check, App jump
|   +-- store_app.c     # Version storage, OTA metadata validation, firmware copy
|   +-- store_app.h
+-- Driver/
|   +-- iap_drv.c       # Jump from Bootloader to App
|   +-- inflash_drv.c   # Internal Flash read/write/erase driver
|   +-- md5.c           # MD5 implementation
|   +-- usb2com_drv.c   # Debug UART driver
|   +-- ...
+-- GD32_lib/           # GD32 peripheral library
+-- Arm_kernel/         # Startup file
+-- ARM.uvprojx         # Keil uVision project
+-- Objects/ARM.sct     # Scatter file, Bootloader link address
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

The Bootloader does not connect to the WiFi module or download firmware by itself. It only consumes the firmware and metadata prepared by the App. Therefore, the Bootloader and App must keep the same Flash layout, `UpdateInfo_t` structure, MD5 string format and version storage rules.

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

- The Bootloader is linked at `0x08000000`.
- The App must be linked at `0x08007000`.
- The Bootloader and App must use the same Flash layout.
- `Download area` stores the new firmware downloaded by the App.
- `UpdateInfo area` stores firmware size, MD5, target version and update flag.
- `Parameter area` stores persistent system parameters such as the current software version.

## Key Modules

### `main.c`

Initializes drivers, loads system parameters, checks whether an upgrade is required, performs the upgrade when needed, clears the processed upgrade metadata, and jumps to the App:

```c
if (ReadUpdateInfo(&updateInfo) && CheckNeedUpdate(&updateInfo))
{
    if (UpdateAppFromDownloadArea(&updateInfo))
    {
        SetSoftwareVersionParam(updateInfo.fwVersion);
    }
    ClearUpdateInfo();
}

BootToApp();
```

### `store_app.c`

Provides persistent storage and Bootloader upgrade APIs:

- `InitSysParam()` loads stored system parameters from Flash.
- `GetSoftwareVersionParam()` returns the current software version.
- `SetSoftwareVersionParam()` writes the new software version after a successful upgrade.
- `ReadUpdateInfo()` reads and validates OTA metadata.
- `CheckNeedUpdate()` checks the user-confirmed upgrade flag and firmware size.
- `UpdateAppFromDownloadArea()` copies firmware from the Download area to the App area and verifies MD5.
- `ClearUpdateInfo()` clears OTA metadata after the Bootloader processes it.

### `iap_drv.c`

Implements the jump from Bootloader to App:

- Reads the App stack pointer and reset handler from the App vector table.
- Checks whether the App stack pointer is inside the valid SRAM range.
- Disables interrupts before switching context.
- Sets MSP to the App stack pointer.
- Relocates the vector table to the App Flash address.
- Calls the App reset handler.

## Data Validation

This project uses different validation mechanisms for different data sizes:

- `magicCode`: marks whether a Flash region contains valid structured data.
- `CRC8`: checks small Flash-stored structures such as `SysParam_t` and `UpdateInfo_t`.
- `needUpdateFlag`: indicates that the user has confirmed the upgrade in the App.
- `MD5`: checks the final firmware image written into the App area.

The App verifies the firmware after downloading it to the Download area. The Bootloader verifies the firmware again while copying it into the App area, so the final image that will be executed is checked before jumping.

## Build

This project is intended for Keil uVision / ARMCC.

1. Open `ARM.uvprojx` with Keil uVision.
2. Confirm the target MCU and GD32 library paths.
3. Confirm that `Objects/ARM.sct` links the Bootloader at `0x08000000` with a size of `0x00007000`.
4. Build the project.
5. Flash the Bootloader image to `0x08000000`.

The App project must be built separately and linked at `0x08007000`. During OTA, the App downloads the new firmware into the Download area and resets to this Bootloader to complete the replacement.

## Configuration Points

Before using the project on real hardware, update:

- Flash partition addresses in `Driver/inflash_drv.h`
- Bootloader link address and size in `Objects/ARM.sct`
- App link address in the companion App project's scatter file
- `UpdateInfo_t` and `SysParam_t` definitions shared with the App project
- SRAM range used by `BootToApp()` for App stack validation
- Firmware version update flow in `store_app.c`

## Limitations

This is a learning/demo OTA implementation. For production use, consider adding:

- Power-loss protection during App area erase and write
- Rollback or A/B firmware partitioning
- Firmware digital signature verification
- Stronger App image validation before jumping
- Retry and failure-state handling when an upgrade fails
- Watchdog integration during long Flash operations
- Protection against mismatched App and Bootloader metadata structures

## Security Notice

The current Bootloader verifies firmware integrity with MD5, which detects accidental corruption but does not prove firmware authenticity. For production OTA, add a digital signature mechanism and protect any product secrets, device keys or update credentials used by the companion App.

## License

No license is currently specified. Add a license file before publishing if you want others to use, modify or redistribute this project.
