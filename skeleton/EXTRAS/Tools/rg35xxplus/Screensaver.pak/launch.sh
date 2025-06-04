#!/bin/bash

DIR="$(dirname "$0")"
cd "$DIR"

show.elf "$DIR/1.png"

# Make sure this is the correct device
DEVICE="/dev/input/event1"

# Message list
messages=("1.png" "2.png" "3.png" "4.png" "5.png" "6.png" "7.png" "8.png" "9.png" "10.png" "11.png" "12.png")
index=0

while true; do
    echo "${messages[$index]}"
    show.elf "$DIR/${messages[$index]}"
    index=$(( (index + 1) % ${#messages[@]} ))

    # This reads exactly one 24-byte event from the input device (non-blocking with timeout)
    if timeout 5 dd if="$DEVICE" bs=24 count=1 status=none 2>/dev/null | grep . >/dev/null; then
        echo "Button press detected. Exiting."
        break
    fi
done