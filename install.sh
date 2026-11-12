#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

data_dir="${XDG_DATA_HOME:-$HOME/.local/share}/plasma-spotify-hover"
drop_in="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/plasma-plasmashell.service.d/90-spotify-hover.conf"

case "${1:-}" in
    enable)
        plugin="$PWD/build/cmake/plugins/plasma/applets/org.kde.plasma.taskmanager.so"
        if [[ ! -f "$plugin" ]]; then
            echo "Build the plugin with ./build.sh first." >&2
            exit 1
        fi
        if ! pacman -Q plasma-desktop plasma-workspace | awk '$2 !~ /^6\.8\.2-/ { exit 1 } END { if (NR != 2) exit 1 }'; then
            echo "This build requires Plasma Desktop and Workspace 6.8.2." >&2
            exit 1
        fi
        if [[ "$data_dir" == *['"%\$']* || "$data_dir" == *$'\n'* ]]; then
            echo "Unsupported characters in the plugin install path." >&2
            exit 1
        fi
        install -Dm755 "$plugin" "$data_dir/plugins/plasma/applets/org.kde.plasma.taskmanager.so"
        install -Dm644 spotify-library.py "$data_dir/spotify-library.py"
        install -Dm755 launch-plasma.sh "$data_dir/launch-plasma.sh"
        mkdir -p -- "$(dirname -- "$drop_in")"
        printf '[Service]\nExecStart=\nExecStart="%s/launch-plasma.sh"\n' "$data_dir" > "$drop_in"
        ;;
    disable)
        rm -f -- "$drop_in"
        ;;
    *)
        echo "Usage: $0 enable|disable" >&2
        exit 2
        ;;
esac
systemctl --user daemon-reload
echo "Apply the change with: systemctl --user restart plasma-plasmashell.service"
