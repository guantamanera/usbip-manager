# USB/IP Manager
Share USB devices from linux to any client over USB/IP


A Qt 6 Widgets desktop application for the Linux USB/IP server. Shows USB devices,
refreshes their sharing status, warns before sharing HID devices, and provides
start/stop and share/unshare controls.

## Build on CachyOS / Arch Linux

Dependencies: `qt6-base`, `cmake`, `gcc`, `make`, `polkit`, and `usbip`.
An active graphical Polkit authentication agent is needed for administrative actions.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --parallel
ctest --test-dir build --output-on-failure
sudo cmake --install build
./build/usbip-manager
```

After installation, USB/IP Manager also appears in the desktop application menu.
Keep the GUI unprivileged: **do not run the application with sudo**.

The installed, root-owned helper is authorized by Polkit for each operation.
It only accepts start/stop for `usbipd.service` and bind/unbind for validated USB
bus IDs. No shell commands are constructed. The GUI uses `pkexec` directly rather
than adding a persistent D-Bus service for this initial version.

The server's USB host module is loaded on demand. The packaged `usbipd.service`
is used without modifying its unit, firewall rules, or boot enablement.
Sharing is not automatically restored after reboot or device reconnection.

## Use

1. Start the server, review the network warning, and authorize the operation.
2. Select a non-hub device, click Share, and review the local-disconnection warning.
3. On a compatible client, attach with `sudo usbip attach -r SERVER_IP -b BUS_ID`.
4. Unshare the device to revoke its export and disconnect an attached client.

USB/IP does not provide built-in authentication or encryption. Restrict TCP port
3240 to trusted clients using your firewall or a VPN. Starting the server uses the
distribution's default listening configuration; this app does not open firewall ports.
Stopping the listener alone may not disconnect already attached clients.

Sharing an input device can remove local keyboard/mouse access. Hubs are excluded.
Bus IDs describe connection topology, not persistent device identity.

## AppImage

```sh
bash packaging/build-appimage.sh
sudo ./dist/helper/install-helper.sh
./dist/USBIP_Manager-x86_64.AppImage
```

The packaging script downloads official linuxdeploy and Qt-plugin AppImages
from their GitHub releases. It bundles Qt with the GUI and performs an offscreen
launch of the resulting AppImage. Packaging tools use extract-and-run mode so
FUSE is not required to build. If FUSE is unavailable when launching the final
AppImage, use `--appimage-extract-and-run`.

Distribute the AppImage together with the `dist/helper` folder. Installing the
root-owned helper and Polkit policy once is required for administrative controls;
bundling these into a user-writable AppImage would not establish system trust.
The host still needs USB/IP, systemd, Polkit with an authentication agent, and
Qt 6 Core for the separately installed helper.

This AppImage is built on CachyOS and targets this machine. It is not guaranteed
to run on distributions with older glibc, libstdc++, or graphics stacks. For broad
distribution, build on the oldest supported distribution and test each target.

## Checks

`ctest` covers bus ID validation, sysfs discovery, HID/hub detection, shared-device
status, invalid helper commands, and an offscreen GUI launch. It does not change
device bindings or start the server. Real administrative actions require an
interactive desktop session and an explicitly selected expendable USB device.
