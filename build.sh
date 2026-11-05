#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
REFIGH_CLI="${REFIGH_ARDUINO_CLI:-}"
if [[ -z "$REFIGH_CLI" ]]; then
  if command -v arduino-cli >/dev/null 2>&1; then
    REFIGH_CLI="$(command -v arduino-cli)"
  elif [[ -x '/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli' ]]; then
    REFIGH_CLI='/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli'
  else
    echo 'Install arduino-cli and Espressif Arduino core 3.3.11 first.' >&2
    exit 1
  fi
fi
"$REFIGH_CLI" compile \
  --fqbn 'esp32:esp32:lilygo_t_display_s3:LoopCore=1,EventsCore=1,USBMode=hwcdc,CDCOnBoot=cdc' \
  --libraries "$PWD/libraries" \
  --build-path "$PWD/build" \
  --build-property 'compiler.cpp.extra_flags=-DDEBUG=0' \
  "$PWD/ats-mini"
