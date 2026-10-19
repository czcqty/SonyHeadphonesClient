#!/usr/bin/env bash
# Run only in a disposable distro container/rootfs. Installs and removes packages.
set -euo pipefail
packages=$(realpath "${1:?Usage: verify_linux.sh package-directory kde|gnome|xfce}")
desktop=${2:?Select desktop}
case "$desktop" in kde|gnome|xfce) ;; *) exit 2;; esac
reports="$packages/../reports"
mkdir -p "$reports"
apt-get install -y --no-install-recommends "$packages"/*.deb
# Reinstall exercises upgrade paths while retaining a user-owned preferences file.
testroot=$(mktemp -d)
trap 'rm -rf -- "$testroot"' EXIT
export XDG_CONFIG_HOME="$testroot/config" XDG_DATA_HOME="$testroot/data"
export XDG_CACHE_HOME="$testroot/cache" XDG_RUNTIME_DIR="$testroot/runtime"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME/czcqty"
chmod 700 "$XDG_RUNTIME_DIR"
printf '[appearance]\nlocale=zh_CN\nthemeMode=dark\n' > "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf"
cp "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf" "$testroot/preferences-before"
apt-get install -y --reinstall "$packages"/*.deb
cd "$testroot"
export PATH=/usr/bin:/bin GSK_RENDERER=cairo G_DEBUG=fatal-criticals
unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QML2_IMPORT_PATH LD_LIBRARY_PATH
for locale in en zh_CN; do
  xvfb-run -a dbus-run-session -- "soundconnect-$desktop" --locale "$locale" --smoke-test \
    2>&1 | tee "$reports/$desktop-$locale.log"
done
cmp "$testroot/preferences-before" "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf"
ldd /usr/libexec/soundconnect/SoundConnectBackend | tee "$reports/backend-dependencies.txt"
! grep -E 'not found|libQt6(Gui|Widgets|Quick|Qml)' "$reports/backend-dependencies.txt"
dpkg-query -W 'soundconnect-*' > "$reports/installed-packages.txt"
apt-get remove -y "soundconnect-$desktop" soundconnect-common soundconnect-backend
test ! -e /usr/libexec/soundconnect/SoundConnectBackend
test ! -e "/usr/bin/soundconnect-$desktop"
cmp "$testroot/preferences-before" "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf"
echo 'PASS: installed entry, native Linux backend, upgrade, preview isolation and uninstall'
