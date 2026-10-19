#!/usr/bin/env bash
set -euo pipefail
source /etc/os-release
apt-get update
apt-get install -y --no-install-recommends build-essential git ca-certificates curl \
  python3 python3-venv python3-pip dpkg-dev patchelf desktop-file-utils \
  libbluetooth-dev libdbus-1-dev libgl1-mesa-dev libxkbcommon-dev libfontconfig1-dev \
  xvfb xauth dbus-x11 python3-gi gir1.2-gtk-4.0 gir1.2-adw-1 \
  gir1.2-gtk-3.0 gir1.2-libxfce4ui-2.0
case "$ID:$VERSION_ID" in
  ubuntu:24.04)
    apt-get install -y breeze qtbase5-dev libkf5widgetsaddons-dev libkf5notifications-dev libkf5configwidgets-dev libxcb-cursor0
    python3 -m venv /opt/soundconnect-tools
    /opt/soundconnect-tools/bin/pip install 'cmake==3.31.6' 'aqtinstall==3.3.0'
    /opt/soundconnect-tools/bin/aqt install-qt linux desktop 6.8.3 linux_gcc_64 --archives qtbase icu -O /opt/qt
    ;;
  debian:13)
    apt-get install -y kde-style-breeze-data cmake qt6-base-dev libkf6widgetsaddons-dev libkf6statusnotifieritem-dev libkf6colorscheme-dev libkf6config-dev libkf6guiaddons-dev libkf6i18n-dev libkf6windowsystem-dev
    ;;
  *) echo 'Only Ubuntu 24.04 and Debian 13 are package targets' >&2; exit 2;;
esac
