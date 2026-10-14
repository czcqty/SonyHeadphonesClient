#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
out="$root/build/macos/Sound Connect.app/Contents"
mkdir -p "$out/MacOS" "$out/Resources"
MACOSX_DEPLOYMENT_TARGET=12.0 swiftc -swift-version 5 -O -framework AppKit "$root/gui/macos/main.swift" -o "$out/MacOS/SoundConnect"
cp "$root/gui/native/catalog.json" "$out/Resources/catalog.json"
cp "$root/gui/macos/Info.plist" "$out/Info.plist"
printf 'Built: %s\n' "$out/MacOS/SoundConnect"
