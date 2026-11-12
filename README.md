# Plasma Spotify Hover

A patch for KDE Plasma Desktop 6.8.2 that adds a timeline and a heart to the existing Spotify taskbar hover preview. The timeline uses the MPRIS player Plasma already matches to the task. Click or drag to seek, or use the arrow keys when the slider has focus. A filled heart means the song is in your Spotify Liked Songs. Click the outline to save it or the filled heart to remove it.

## Install on CachyOS

From this project directory, run:

```sh
./install.sh
```

The installer uses the local build if present and builds the plugin if needed. It opens your [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) for first-time setup. Create or select an app with Web API access, add the loopback redirect URI printed in the terminal, and paste the app’s Client ID into the installer. Spotify requires Premium for Development Mode app owners.

The installer then opens Spotify’s consent page. Approve library access in your browser. After authorization succeeds, the installer puts the helper and plugin in your user data directory, enables the override, and restarts Plasma. The panel and desktop may briefly disappear during the restart. The installer reuses your existing Client ID and asks for library read access once if you are upgrading from the save-only version. To connect a different account or repair expired authorization, run `./install.sh reconnect`.

Play a song and hover over the Spotify taskbar entry. Keep **Show media and volume controls in tooltip** enabled in Task Manager settings. Seeking works locally. The heart targets tracks in the native Spotify Linux client. Local files, ads, and episodes may lack a usable track URI.

The installer targets Plasma 6.8.2 and the standard `plasma-plasmashell.service` user unit. The launcher falls back to the distro plugin when the installed Plasma version changes. Building from source requires Git, GNU patch, CMake, Ninja, a C++ compiler, `extra-cmake-modules`, Qt 6, and the KDE libraries listed in [CMakeLists.txt](CMakeLists.txt).

## Disable

```sh
./install.sh disable
```

This removes the Plasma service override and reloads the shell. The Spotify refresh token lives in a user-only file called `spotify.json` under `${XDG_CONFIG_HOME:-~/.config}/plasma-spotify-hover/`. Remove it or revoke access in Spotify account settings to disconnect the account.

## Implementation notes

- Plasma 6.8.2 packages the QML tooltip in a compiled Task Manager plugin shared by Task Manager and Icons-Only Task Manager. The patch applies to the [matching KDE source](https://github.com/KDE/plasma-desktop/tree/v6.8.2/applets/taskmanager).
- Seeking follows KDE’s [Media Controller implementation](https://github.com/KDE/plasma-workspace/blob/v6.8.2/applets/mediacontroller/ExpandedRepresentation.qml) and the [MPRIS interface](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html).
- The [Spotify library status API](https://developer.spotify.com/documentation/web-api/reference/check-library-contains) supplies the heart state. The [remove endpoint](https://developer.spotify.com/documentation/web-api/reference/remove-library-items) handles unliking. The GNOME [Spotify Controller](https://github.com/NarkAgni/spotify-controller/blob/640d3f6a5fe26e78bb75a58c64538ec1804ca7e0/core/playlist.js) was reviewed for comparison. Its heart stores favorites locally. This project implements the account save using Spotify’s [Save Items to Library](https://developer.spotify.com/documentation/web-api/reference/save-library-items) endpoint.

## License

The project’s original code is licensed under the [GNU Affero General Public License v3.0](LICENSE). The KDE files modified by the patch retain their upstream GPL and LGPL notices. Their license texts are included in this repository.
