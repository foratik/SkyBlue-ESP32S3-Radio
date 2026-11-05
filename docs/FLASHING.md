# Firmware, backup and installation

SkyBlue was tested on one ATS Mini-style ESP32-S3 receiver with 16 MiB flash, 8 MiB OPI PSRAM and a 320 × 170 parallel display. Match your PCB wiring to the source configuration before using these binaries. Replace `PORT` with your device's serial port. Commands use esptool 5.x syntax.

## Save your own full backup

```sh
esptool --chip esp32s3 --port PORT --baud 921600 read-flash 0 ALL before-skyblue.bin
esptool --chip esp32s3 --port PORT verify-flash 0 before-skyblue.bin
```

Keep this file locally. A flash dump can include device settings and credentials; the owner's full dump is not part of this repository.

## Update a device already running this project

The tested v1.1.x updates only replace the application at `0x10000`, using the compatible bootloader and partition table already present:

```sh
esptool --chip esp32s3 --port PORT --baud 921600 write-flash --flash-mode keep --flash-freq keep --flash-size keep 0x10000 releases/refigh-v1.1.2/refigh-app.bin
```

This application-only update retains the Refigh preference, existing NVS settings and partition table.

## Initial installation on matching hardware

Build first. For initial provisioning, use the complete matching build outputs:

```sh
esptool --chip esp32s3 --port PORT --baud 921600 write-flash --flash-mode keep --flash-freq keep --flash-size keep 0x0 build/ats-mini.ino.bootloader.bin 0x8000 build/ats-mini.ino.partitions.bin 0xe000 build/boot_app0.bin 0x10000 build/ats-mini.ino.bin
```

This writes bootloader, partition table, OTA selection data and application. It is a different operation from an application-only update and should follow your own backup.

The checked-in partition layout occupies the first 8 MiB of the tested 16 MiB flash. Its two application slots are `0x330000` bytes each; the remaining physical flash is unassigned by that table.

## Verify a firmware file

Each snapshot has `SHA256SUMS.txt`. From its directory, for example on macOS:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

## Restore your full backup

```sh
esptool --chip esp32s3 --port PORT --baud 921600 write-flash --flash-mode keep --flash-freq keep --flash-size keep 0x0 before-skyblue.bin
```

This restores the saved flash contents, including settings at backup time. These installation commands do not burn eFuses.

## Refigh calibration

The built-in RF signature was measured for the original device, antenna and location. A different setup can produce a different signature. Refigh can be disabled from `Settings → Refigh`; uncertain estimates skip the clip.
