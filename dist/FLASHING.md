# Flashing

Dieses Release enthaelt pro ESP zwei relevante Artefaktarten:

- `*_firmware.elf`: Debug-/Analyse-Artefakt aus dem Build
- `*_flash.bin`: zusammengefuehrte Flash-Datei fuer direktes Schreiben auf den ESP

Empfohlene Flash-Offets der Einzeldateien:

- Host ESP32-S3:
  - `0x0000` `bootloader.bin`
  - `0x8000` `partitions.bin`
  - `0xE000` `boot_app0.bin`
  - `0x10000` `firmware.bin`

- Client ESP32:
  - `0x1000` `bootloader.bin`
  - `0x8000` `partitions.bin`
  - `0xE000` `boot_app0.bin`
  - `0x10000` `firmware.bin`

Alternativ direkt die jeweilige `*_flash.bin` verwenden.
