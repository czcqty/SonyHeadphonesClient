#!/usr/bin/env python3
"""Validate the immutable cross-platform product and package identity."""

from __future__ import annotations

from pathlib import Path


PRODUCT = "SonyHeadphonesClient"
PACKAGE_ID = "io.github.czcqty.SonyHeadphonesClient"


def require(path: Path, *fragments: str) -> str:
    contents = path.read_text(encoding="utf-8")
    missing = [fragment for fragment in fragments if fragment not in contents]
    if missing:
        raise SystemExit(f"{path} is missing required metadata: {missing}")
    return contents


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    top_cmake = require(
        root / "CMakeLists.txt",
        'CPACK_PACKAGE_NAME "SonyHeadphonesClient"',
        'CPACK_PACKAGE_VENDOR "czcqty"',
        'CPACK_NSIS_DISPLAY_NAME "SonyHeadphonesClient"',
        '"SonyHeadphonesClient" "SonyHeadphonesClient"',
    )
    gui_cmake = require(
        root / "gui" / "CMakeLists.txt",
        f'MACOSX_BUNDLE_GUI_IDENTIFIER "{PACKAGE_ID}"',
        'MACOSX_BUNDLE_BUNDLE_NAME "SonyHeadphonesClient"',
        'OUTPUT_NAME "SonyHeadphonesClient"',
    )
    desktop = require(
        root / "packaging" / f"{PACKAGE_ID}.desktop",
        "Name=SonyHeadphonesClient",
        "Exec=SonyHeadphonesClient",
        f"Icon={PACKAGE_ID}",
    )
    workflow = require(
        root / ".github" / "workflows" / "release.yml",
        "SonyHeadphonesClient-${{ needs.validate.outputs.version }}-windows-x64-setup.exe",
        "SonyHeadphonesClient-${{ needs.validate.outputs.version }}-macos-universal.dmg",
        "SonyHeadphonesClient-${{ needs.validate.outputs.version }}-x86_64.AppImage",
    )
    main_cpp = require(
        root / "gui" / "quick" / "main.cpp",
        'setApplicationName(QStringLiteral("SonyHeadphonesClient"))',
        'setApplicationDisplayName(QStringLiteral("SonyHeadphonesClient"))',
        'setOrganizationName(QStringLiteral("czcqty"))',
    )

if __name__ == "__main__":
    main()
