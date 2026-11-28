#!/usr/bin/env python3
"""Validate an explicitly supplied Ruffle release and generate a resource manifest."""
import argparse
import re
from pathlib import Path
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--list", action="store_true", dest="list_assets")
parser.add_argument("--manifest", action="store_true", dest="manifest")
parser.add_argument("directory")
args = parser.parse_args()
if args.list_assets == args.manifest:
    parser.error("select --list or --manifest")
directory = Path(args.directory)
if not directory.is_absolute() or not directory.is_dir():
    parser.error("expected an absolute release directory")
assets = []
for path in sorted(directory.iterdir()):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+\.(?:js|wasm)", path.name):
        continue
    if path.is_symlink() or not path.is_file():
        parser.error(f"asset is not a regular file: {path.name}")
    assets.append(path.name)
if "ruffle.js" not in assets or not any(n.endswith(".wasm") for n in assets):
    parser.error("release must contain ruffle.js and WebAssembly assets")
if not any(n.startswith("core.ruffle.") and n.endswith(".js") for n in assets):
    parser.error("release must contain the generated core JavaScript chunks")
if args.list_assets:
    print("\n".join(assets))
else:
    root = ET.Element("gresources")
    group = ET.SubElement(root, "gresource", prefix="/ruffle-web")
    for name in assets:
        ET.SubElement(group, "file").text = name
    print(ET.tostring(root, encoding="unicode"))
