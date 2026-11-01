"""Offline PNG pack conversion regression; requires Pillow."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image

spec = importlib.util.spec_from_file_location("compress_pack", Path(__file__).resolve().parents[1] / "docs/assets/hd-poc/compress_pack.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PackConversion(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.destination = self.root / "converted"
        records = []
        for index, mode in enumerate(("RGB", "RGBA")):
            image = Image.new(mode, (68, 136))
            image.putdata([(i % 256, (i * 7) % 256, (i * 13) % 256) + ((i % 256,) if mode == "RGBA" else ()) for i in range(68 * 136)])
            key = f"{index + 1:016x}"
            path = self.source / (key + (".bmp" if mode == "RGB" else ".png"))
            image.save(path)
            records.append(dict(key=key, file=path.name, atlas_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), room="M001", tiles=[[0, 0]]))
        self.manifest = dict(atlases=records, custom="preserve metadata")
        self.write_manifest()

    def write_manifest(self):
        (self.source / "manifest.json").write_text(json.dumps(self.manifest))

    def test_lossless_and_source_preserved(self):
        before = {p.name: p.read_bytes() for p in self.source.iterdir()}
        result = module.convert(self.source, self.destination, version="0.1")
        after = json.loads((self.destination / "manifest.json").read_text())
        self.assertEqual(after["custom"], "preserve metadata")
        self.assertEqual(after["version"], "0.1")
        self.assertEqual(result["files"], 2)
        for record in after["atlases"]:
            source_record = next(r for r in self.manifest["atlases"] if r["key"] == record["key"])
            with Image.open(self.source / source_record["file"]) as original, Image.open(self.destination / record["file"]) as png:
                self.assertEqual(original.convert("RGBA").tobytes(), png.convert("RGBA").tobytes())
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.source.iterdir()})

    def test_bad_hash_never_activates_partial_pack(self):
        self.manifest["atlases"][0]["atlas_sha256"] = "0" * 64
        self.write_manifest()
        with self.assertRaises(ValueError): module.convert(self.source, self.destination)
        self.assertFalse(self.destination.exists())
        self.assertFalse(list(self.root.glob("converted.partial-*")))

    def test_title_and_widescreen_extras_are_preserved(self):
        self.manifest["extras"] = []
        for filename in ("title.png", "0123456789abcdef-wide.bin"):
            content = b"checked pack extra " + filename.encode()
            (self.source / filename).write_bytes(content)
            self.manifest["extras"].append(dict(file=filename, sha256=hashlib.sha256(content).hexdigest()))
        self.write_manifest()
        module.convert(self.source, self.destination)
        for extra in self.manifest["extras"]:
            self.assertEqual((self.source / extra["file"]).read_bytes(), (self.destination / extra["file"]).read_bytes())

    def test_extra_checksum_and_path_fail_atomically(self):
        for filename in ("title.png", "../outside.bin", "manifest.json", "C:title.png", "0000000000000001.PNG"):
            self.manifest["extras"] = [dict(file=filename, sha256="0"*64)]
            if filename == "title.png": (self.source / filename).write_bytes(b"damaged title")
            self.write_manifest()
            with self.assertRaises(ValueError): module.convert(self.source, self.destination)
            self.assertFalse(self.destination.exists())
            self.assertFalse(list(self.root.glob("converted.partial-*")))

    def test_existing_pack_is_not_overwritten(self):
        self.destination.mkdir()
        marker = self.destination / "working-version"
        marker.write_text("keep")
        with self.assertRaises(FileExistsError): module.convert(self.source, self.destination)
        self.assertEqual(marker.read_text(), "keep")

    def test_unsafe_manifest_path_is_rejected(self):
        self.manifest["atlases"][0]["file"] = "../outside.bmp"
        self.write_manifest()
        with self.assertRaises(ValueError): module.convert(self.source, self.destination)
        self.assertFalse(self.destination.exists())


if __name__ == "__main__": unittest.main()
