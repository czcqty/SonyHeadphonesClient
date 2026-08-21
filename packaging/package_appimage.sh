#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
    echo "usage: $0 <build-dir> <output-dir> <version> <linuxdeploy-appimage>" >&2
    exit 2
fi

build_dir="$(cd "$1" && pwd)"
output_dir="$2"
version="$3"
linuxdeploy="$(cd "$(dirname "$4")" && pwd)/$(basename "$4")"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
appdir="$build_dir/AppDir"

case "$appdir" in
    "$build_dir"/*) ;;
    *) echo "refusing to clean unexpected AppDir: $appdir" >&2; exit 1 ;;
esac

cmake -E remove_directory "$appdir"
mkdir -p "$output_dir"
cmake --install "$build_dir" --config Release --prefix "$appdir/usr" --strip

export ARCH=x86_64
export VERSION="$version"
export APPIMAGE_EXTRACT_AND_RUN=1
"$linuxdeploy" \
    --appdir "$appdir" \
    --desktop-file "$repo_root/packaging/io.github.czcqty.SonyHeadphonesClient.desktop" \
    --icon-file "$repo_root/gui/assets/icons/app-icon.svg" \
    --output appimage

mapfile -t packages < <(find . -maxdepth 1 -type f -name 'SonyHeadphonesClient*.AppImage')
if [[ ${#packages[@]} -ne 1 ]]; then
    echo "expected exactly one AppImage, found ${#packages[@]}" >&2
    exit 1
fi

mv "${packages[0]}" \
    "$output_dir/SonyHeadphonesClient-${version}-x86_64.AppImage"
