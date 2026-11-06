#!/usr/bin/env bash
# Run only in a disposable distro container/rootfs. Installs and removes packages.
set -euo pipefail
packages=$(realpath "${1:?Usage: verify_linux.sh package-directory kde|gnome|xfce [previous-packages]}")
desktop=${2:?Select desktop}
case "$desktop" in kde|gnome|xfce) ;; *) exit 2;; esac
reports="$packages/../reports"
mkdir -p "$reports"
testroot=$(mktemp -d)
trap 'rm -rf -- "$testroot"' EXIT
export XDG_CONFIG_HOME="$testroot/config" XDG_DATA_HOME="$testroot/data"
export XDG_CACHE_HOME="$testroot/cache" XDG_RUNTIME_DIR="$testroot/runtime"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME/czcqty"
chmod 700 "$XDG_RUNTIME_DIR"
printf '[appearance]\nlocale=zh_CN\nthemeMode=dark\n' > "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf"
cp "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf" "$testroot/preferences-before"

# Select this adapter even if the directory also contains packages for the other desktops.
archives=("$packages"/soundconnect-backend_*.deb "$packages"/soundconnect-common_*.deb "$packages"/soundconnect-"$desktop"_*.deb)
if (( ${#archives[@]} != 3 )); then echo "Expected one revision of each package" >&2; exit 2; fi
if [[ -n ${3:-} ]]; then
  previous=$(realpath "$3")
  apt-get install -y --no-install-recommends "$previous"/soundconnect-{backend,common}*.deb "$previous"/soundconnect-"$desktop"_*.deb
  dpkg --compare-versions "$(dpkg-query -W -f='${Version}' "soundconnect-$desktop")" lt "$(dpkg-deb -f "${archives[0]}" Version)"
fi
verify_payload() {
  for archive in "${archives[@]}"; do
    dpkg-deb -x "$archive" "$testroot/payload"
  done
  while IFS= read -r -d '' path; do cmp "$path" "${path#"$testroot/payload"}"; done < <(find "$testroot/payload" -type f -print0)
}
apt-get install -y --no-install-recommends "${archives[@]}"
verify_payload
apt-get install -y --reinstall "${archives[@]}"
verify_payload
cd "$testroot"
export PATH=/usr/bin:/bin GSK_RENDERER=cairo G_DEBUG=fatal-criticals GDK_BACKEND=x11 QT_QPA_PLATFORM=xcb
unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QML2_IMPORT_PATH LD_LIBRARY_PATH
for locale in en zh_CN; do
  for suite in smoke parity; do
    pointer_test=0
    if [[ "$desktop" == gnome && "$suite" == parity ]]; then pointer_test=1; fi
    SOUNDCONNECT_TEST_X11_POINTER=$pointer_test timeout 300 xvfb-run -a dbus-run-session -- "soundconnect-$desktop" --locale "$locale" "--$suite-test" \
      2>&1 | tee "$reports/$desktop-$locale-$suite.log"
  done
done
cmp "$testroot/preferences-before" "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf"
ldd /usr/libexec/soundconnect/SoundConnectBackend | tee "$reports/backend-dependencies.txt"
if grep -E 'not found|libQt6(Gui|Widgets|Quick|Qml)' "$reports/backend-dependencies.txt"; then exit 1; fi
dpkg-query -W 'soundconnect-*' > "$reports/installed-packages.txt"
apt-get remove -y "soundconnect-$desktop" soundconnect-common soundconnect-backend
while IFS= read -r -d '' path; do test ! -e "${path#"$testroot/payload"}"; done < <(find "$testroot/payload" -type f -print0)
cmp "$testroot/preferences-before" "$XDG_CONFIG_HOME/czcqty/SonyHeadphonesClient.conf"
echo 'PASS: installed payload, native Linux backend, both languages, preview isolation and uninstall'
