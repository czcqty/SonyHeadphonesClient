#!/usr/bin/env python3
"""Fail when a qsTr source is missing or unfinished in the Chinese catalog."""

from __future__ import annotations

import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    qml = (root / "gui" / "qml" / "I18n.qml").read_text(encoding="utf-8")
    source_tokens = re.findall(r'qsTr\("((?:\\.|[^"\\])*)"', qml)
    qml_sources = {json.loads(f'"{token}"') for token in source_tokens}
    defined_keys = set(re.findall(r'case "([^"]+)":', qml))

    qml_files = list((root / "gui" / "qml").glob("*.qml"))
    used_keys = set()
    for path in qml_files:
        contents = path.read_text(encoding="utf-8")
        used_keys.update(re.findall(r'I18n\.text\("([^"]+)"\)', contents))
    undefined_keys = sorted(used_keys - defined_keys)
    if undefined_keys:
        raise SystemExit(f"Undefined I18n keys: {undefined_keys}")

    catalog = ET.parse(
        root / "gui" / "translations" / "SonyHeadphonesClient_zh_CN.ts")
    translations = {}
    for message in catalog.findall(".//message"):
        source = message.findtext("source")
        translation = message.find("translation")
        if source is None or translation is None:
            continue
        unfinished = translation.get("type") == "unfinished"
        text = "".join(translation.itertext()).strip()
        translations[source] = (text, unfinished)

    missing = sorted(qml_sources - translations.keys())
    invalid = sorted(
        source for source in qml_sources
        if source in translations
        and (not translations[source][0] or translations[source][1]))
    if missing or invalid:
        raise SystemExit(
            f"Missing translations: {missing}; empty/unfinished translations: {invalid}")

    forbidden = ("Headphones Client", "耳机客户端")
    gui_files = qml_files
    gui_files += list((root / "gui" / "quick").glob("*.cpp"))
    for path in gui_files:
        contents = path.read_text(encoding="utf-8")
        for name in forbidden:
            if name in contents:
                raise SystemExit(f"Deprecated display name {name!r} in {path}")


if __name__ == "__main__":
    main()
