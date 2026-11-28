#!/usr/bin/env python3
"""Exercise the packaging helper without downloading or executing Ruffle."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
import xml.etree.ElementTree as ET

HELPER = Path(sys.argv.pop(1)).resolve()


class AssetManifestTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        for name in ("ruffle.js", "core.ruffle.abc.js", "abc.wasm", "README.md"):
            (self.directory / name).write_text("fixture", encoding="utf-8")

    def run_helper(self, mode):
        return subprocess.run(
            [sys.executable, str(HELPER), mode, str(self.directory)],
            check=False, capture_output=True, text=True
        )

    def test_manifest(self):
        result = self.run_helper("--manifest")
        self.assertEqual(result.returncode, 0, result.stderr)
        root = ET.fromstring(result.stdout)
        group = root.find("gresource")
        self.assertEqual(group.attrib["prefix"], "/ruffle-web")
        self.assertEqual(
            [node.text for node in group.findall("file")],
            ["abc.wasm", "core.ruffle.abc.js", "ruffle.js"]
        )
        result = self.run_helper("--list")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(),
                         ["abc.wasm", "core.ruffle.abc.js", "ruffle.js"])

    def test_missing_wasm(self):
        (self.directory / "abc.wasm").unlink()
        self.assertNotEqual(self.run_helper("--manifest").returncode, 0)

    def test_symlink_rejected(self):
        (self.directory / "linked.wasm").symlink_to("abc.wasm")
        self.assertNotEqual(self.run_helper("--manifest").returncode, 0)


if __name__ == "__main__":
    unittest.main()
