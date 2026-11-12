#!/usr/bin/env bash
# Fall back to the distro plugin after a Plasma version upgrade.
if pacman -Q plasma-desktop plasma-workspace | awk '$2 !~ /^6\.8\.2-/ { exit 1 } END { if (NR != 2) exit 1 }'; then
    plugin_dir="$(dirname -- "${BASH_SOURCE[0]}")/plugins"
    export QT_PLUGIN_PATH="$plugin_dir${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
fi
exec /usr/bin/plasmashell --no-respawn
