# Plasma Spotify Hover

A patch for KDE Plasma Desktop 6.8.2 that adds a timeline and a heart to the existing Spotify taskbar hover preview. The timeline uses the MPRIS player Plasma already matches to the task. Click or drag to seek, or use the arrow keys when the slider has focus. The heart saves the displayed song to your Spotify Liked Songs and reports the result in the popup. Clicking it again is safe.

## Build and enable on CachyOS

The plugin builds against Plasma 6.8.2 and the matching installed development libraries. Install `extra-cmake-modules` if missing. The build also needs Git, GNU patch, CMake, Ninja, a C++ compiler, Qt 6 and the KDE libraries listed in [CMakeLists.txt](CMakeLists.txt).

```sh
./build.sh
./install.sh enable
systemctl --user restart plasma-plasmashell.service
```

A local build of the plugin was placed in the build directory on this machine, so you can start at `./install.sh enable`. The install script targets a Plasma session managed by the standard `plasma-plasmashell.service` user unit. Its drop-in launches Plasma with the user plugin path ahead of the distro path. The launcher falls back to the distro plugin when the installed Plasma version changes. The restart reloads the panel and desktop shell. With Spotify playing, hover over its taskbar entry. Keep **Show media and volume controls in tooltip** enabled in Task Manager settings.

## Connect the heart to Spotify

Seeking works independently of Spotify account setup.

1. Open the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and create or reuse a Development Mode app with Web API access. The app owner needs Spotify Premium. Register `http://127.0.0.1:8765/callback` as a redirect URI.
2. Run `python3 spotify-library.py auth YOUR_CLIENT_ID`, open the URL it prints, and approve library access. Use the account whose Liked Songs should receive the tracks. The helper uses PKCE and stores the refresh token in a user-only file under `${XDG_CONFIG_HOME:-~/.config}/plasma-spotify-hover/`.
3. Hover over Spotify and click the heart. This targets the native Spotify Linux client and the song identified by its MPRIS metadata. Local files, ads, and episodes may lack a usable track URI.

## Disable

```sh
./install.sh disable
systemctl --user restart plasma-plasmashell.service
```

This removes the Plasma service override. To disconnect the account too, remove the saved `spotify.json` from the config directory or revoke the app in your Spotify account settings.

## Implementation notes

- Plasma 6.8.2 packages the QML tooltip in a compiled Task Manager plugin shared by Task Manager and Icons-Only Task Manager. The patch applies to the [matching KDE source](https://github.com/KDE/plasma-desktop/tree/v6.8.2/applets/taskmanager).
- Seeking follows KDE’s [Media Controller implementation](https://github.com/KDE/plasma-workspace/blob/v6.8.2/applets/mediacontroller/ExpandedRepresentation.qml) and the [MPRIS interface](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html).
- The GNOME [Spotify Controller](https://github.com/NarkAgni/spotify-controller/blob/640d3f6a5fe26e78bb75a58c64538ec1804ca7e0/core/playlist.js) was reviewed for comparison. Its heart stores favorites locally. This project implements the account save using Spotify’s [Save Items to Library](https://developer.spotify.com/documentation/web-api/reference/save-library-items) endpoint.
