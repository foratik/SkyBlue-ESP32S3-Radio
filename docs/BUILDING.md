# Build SkyBlue

The tested configuration uses Arduino CLI 1.5.1, Espressif Arduino Core 3.3.11 and the vendored libraries in this repository.

## Install the core

With Arduino CLI available:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

## Compile

From the repository root:

```sh
bash build.sh
```

`build.sh` finds Arduino CLI on PATH, or in the standard macOS Arduino IDE installation. Set `REFIGH_ARDUINO_CLI` to override its location. Outputs go to the ignored `build/` directory.

The build uses the `lilygo_t_display_s3` profile, OPI PSRAM and the local `ats-mini/partitions.csv`. The display library uses Setup206. This matches the tested ATS Mini-style receiver; it is not a universal pinout for all ESP32-S3 radios.

## Rebuild display assets

The generated headers are already checked in, so Python, fonts and rendering tools are not required for a normal firmware build.

For media changes, install Python dependencies and provide ffmpeg and `rsvg-convert` from librsvg:

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-media.txt
```

The media-generation versions were tested with Python 3.9.

### Refigh video and captions

Provide `Vazirmatn-Regular.ttf` and `Vazirmatn-Bold.ttf` in a font directory:

```sh
.venv/bin/python make_assets.py media/refigh.mp4 --font-dir FONT_DIRECTORY
```

Edit the caption text/layout in `make_assets.py`. This regenerates `ats-mini/RefighAssets.h`; previews go to the ignored `preview/` directory. Font fingerprints are recorded in `media/fonts-sha256.json`.

### Boot calligraphy and face

```sh
.venv/bin/python make_boot_assets.py
```

This reads `media/Besmellah.svg`, derives a stroke-following reveal, and generates colored face poses in `ats-mini/BootScreenAssets.h`. The original artwork remains unchanged. Previews go to `preview/boot/`.

Compile again after regenerating either header.

## Host-side checks

```sh
c++ -std=c++11 -Wall -Wextra -Werror test_decision.cpp -o test-decision
./test-decision
```

These check antenna-decision logic and calibrated-signature handling. They do not replace a display/receiver test on physical hardware.
