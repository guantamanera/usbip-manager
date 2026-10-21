#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
tools="$root/build/appimage-tools"
dist="$root/dist"
mkdir -p "$tools" "$dist"
cp "$root/LAUNCH.txt" "$dist/INSTALL-AND-LAUNCH.txt"
appdir="$(mktemp -d "$root/build/AppDir.XXXXXX")"

for name in linuxdeploy linuxdeploy-plugin-qt; do
    file="$tools/$name-x86_64.AppImage"
    if [[ ! -x "$file" ]]; then
        curl --fail --location --retry 3 \
            "https://github.com/linuxdeploy/$name/releases/download/continuous/$name-x86_64.AppImage" \
            --output "$file.download"
        chmod +x "$file.download"
        mv -- "$file.download" "$file"
    fi
done

cmake -S "$root" -B "$root/build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build "$root/build" --parallel 4
ctest --test-dir "$root/build" --output-on-failure

mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" \
    "$appdir/usr/share/icons/hicolor/scalable/apps" "$dist/helper"
cp "$root/build/usbip-manager" "$appdir/usr/bin/"
cp "$root/org.usbip.manager.desktop" "$appdir/usr/share/applications/"
cp "$root/packaging/usbip-manager.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/"

# The administrative helper must live outside the user-writable AppImage.
cp "$root/build/usbip-manager-helper" "$dist/helper/"
cp "$root/build/org.usbip.manager.policy" "$dist/helper/"
cp "$root/packaging/install-helper.sh" "$dist/helper/"
chmod +x "$dist/helper/install-helper.sh"

export APPIMAGE_EXTRACT_AND_RUN=1
# The bundled strip predates CachyOS's RELR ELF sections.
export NO_STRIP=1
export QMAKE=/usr/bin/qmake6
export EXTRA_PLATFORM_PLUGINS="libqwayland.so;libqoffscreen.so"
export OUTPUT="$dist/USBIP_Manager-x86_64.AppImage"
cd "$root"
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --desktop-file "$appdir/usr/share/applications/org.usbip.manager.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/usbip-manager.svg"
# KDE image codecs are unrelated to this utility and can have optional dependencies.
"$tools/linuxdeploy-plugin-qt-x86_64.AppImage" --appdir "$appdir" --exclude-library 'kimg_*'
# Qt also searches next to the AppRun entry point when APPIMAGE is set.
cp "$root/packaging/qt.conf" "$appdir/qt.conf"
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --output appimage

QT_QPA_PLATFORM=offscreen "$OUTPUT" --appimage-extract-and-run --smoke-test
printf '\nAppImage: %s\nOne-time helper installation: sudo %s/helper/install-helper.sh\n' "$OUTPUT" "$dist"
