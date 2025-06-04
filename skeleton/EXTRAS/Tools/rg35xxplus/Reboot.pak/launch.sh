#!/bin/sh
#!/bin/bash
DIR="$(dirname "$0")"
cd "$DIR"

show.elf "$DIR/reboot.png"

rm -f /tmp/minui_exec && sync
shutdown -r now