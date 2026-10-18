#!/usr/bin/env bash
# Run in the selected native branch, inside the matching distribution.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
desktop=${1:?Usage: build_linux.sh kde|gnome|xfce [build-directory]}
case "$desktop" in kde|gnome|xfce) ;; *) exit 2;; esac
build=${2:-"$root/build/native-linux"}
source /etc/os-release
case "$ID:$VERSION_ID" in
  ubuntu:24.04) distro=ubuntu24.04; : "${SOUNDCONNECT_QT_ROOT:?Set SOUNDCONNECT_QT_ROOT to the private Qt 6.8.3 SDK}";;
  debian:13) distro=debian13;;
  *) echo "Unsupported package build distribution: $ID $VERSION_ID" >&2; exit 2;;
esac
mkdir -p "$build/reports"
qt_args=()
if [[ "$distro" == ubuntu24.04 ]]; then qt_args+=("-DCMAKE_PREFIX_PATH=$SOUNDCONNECT_QT_ROOT"); fi
cmake -S "$root" -B "$build/backend" -DCMAKE_BUILD_TYPE=Release \
  -DMDR_ENABLE_CODEGEN=OFF -DMDR_BUILD_QT_FRONTEND=OFF \
  -DMDR_DEPLOY_BACKEND_RUNTIME=OFF -DBUILD_TESTING=ON "${qt_args[@]}"
cmake --build "$build/backend" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$build/backend" --output-on-failure --output-junit "$build/reports/backend.xml"
if [[ "$desktop" == kde ]]; then
  cmake -S "$root/gui/kde" -B "$build/kde" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$build/kde" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
fi
python3 "$root/packaging/native/package_linux.py" --desktop "$desktop" --distro "$distro" \
  --build "$build" --output "$build/packages" ${SOUNDCONNECT_QT_ROOT:+--qt-root "$SOUNDCONNECT_QT_ROOT"}
