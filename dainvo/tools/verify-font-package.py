#!/usr/bin/env python3
"""Verify the pinned redistributable fonts and licenses in a macOS app bundle."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "fonts"
    manifest = source / "manifest.json"
    bundled = args.app / "Contents/Resources/fonts"
    assert (bundled / "licenses/manifest.json").read_bytes() == manifest.read_bytes()
    entries = json.loads(manifest.read_text())
    for entry in entries:
        relative = Path(entry["file"])
        path = (bundled / "truetype" / relative.name if relative.suffix == ".ttf"
                else bundled / "licenses" / relative)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], path
    assert not list((bundled / "truetype").glob("Aptos*.ttf")), "Reference fonts must not ship"
    assert not list((bundled / "truetype").glob("Intos-Semibold*.ttf")), "Review styles must not ship"
    count = sum(entry["file"].endswith(".ttf") for entry in entries)
    print(f"Verified {count} pinned fonts, both OFL licenses, and provenance manifest in {args.app}")


if __name__ == "__main__":
    main()
