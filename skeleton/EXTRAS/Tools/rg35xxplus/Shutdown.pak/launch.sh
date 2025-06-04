#!/bin/sh
DIR="$(dirname "$0")"
cd "$DIR"

show.elf "$DIR/shutdown.png"

rm -f /tmp/minui_exec && sync
shutdown -h now