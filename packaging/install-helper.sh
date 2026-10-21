#!/usr/bin/env bash
set -euo pipefail

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$EUID" -ne 0 ]]; then
    echo "Run this installer with sudo. Do not run the GUI as root." >&2
    exit 1
fi
for file in usbip-manager-helper org.usbip.manager.policy; do
    if [[ ! -f "$here/$file" ]]; then
        echo "Missing $here/$file. Use the complete dist/helper directory." >&2
        exit 1
    fi
done
install -d -o root -g root -m 755 /usr/local/libexec /usr/share/polkit-1/actions
install -o root -g root -m 755 "$here/usbip-manager-helper" /usr/local/libexec/usbip-manager-helper
install -o root -g root -m 644 "$here/org.usbip.manager.policy" \
    /usr/share/polkit-1/actions/org.usbip.manager.policy
printf 'USB/IP helper installed. Launch the AppImage as your normal user.\n'
