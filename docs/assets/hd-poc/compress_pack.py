"""Create a separate, lossless PNG distribution pack from a reviewed HD pack."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
from PIL import Image


def convert(source, destination, workers=4, version=None):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if destination.exists():
        raise FileExistsError(f"Refusing to replace an existing pack: {destination}")
    manifest = json.loads((source / "manifest.json").read_text())
    records = manifest["atlases"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=destination.name + ".partial-", dir=destination.parent))
    try:
        jobs = {}
        for record in records:
            key = record["key"]
            if len(key) != 16 or any(c not in "0123456789abcdef" for c in key):
                raise ValueError(f"Invalid atlas key: {key!r}")
            filename = record.get("file", key + ".bmp")
            if Path(filename).name != filename:
                raise ValueError(f"Invalid atlas filename: {filename!r}")
            job = (filename, record.get("atlas_sha256"))
            if key in jobs and jobs[key] != job:
                raise ValueError(f"Conflicting atlas records: {key}")
            jobs[key] = job

        def image_job(item):
            key, (filename, expected) = item
            path = source / filename
            original = path.read_bytes()
            digest = hashlib.sha256(original).hexdigest()
            if expected and expected != digest:
                raise ValueError(f"Source checksum mismatch: {filename}")
            with Image.open(path) as image:
                image.load()
                pixels = image.convert("RGBA").tobytes()
                output = staging / (key + ".png")
                image.save(output, format="PNG", compress_level=6)
            with Image.open(output) as decoded:
                if decoded.convert("RGBA").tobytes() != pixels:
                    raise ValueError(f"Pixel mismatch: {key}")
            return key, dict(file=output.name, atlas_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                             source_atlas_sha256=digest, pixel_sha256=hashlib.sha256(pixels).hexdigest(),
                             source_bytes=len(original), compressed_bytes=output.stat().st_size)

        converted = {}
        with ThreadPoolExecutor(max_workers=workers) as executor:
            for key, result in executor.map(image_job, jobs.items()):
                converted[key] = result
                if len(converted) % 100 == 0:
                    print(f"{len(converted)}/{len(jobs)}", flush=True)
        # Title artwork and widescreen indices/materials are versioned and
        # checked with the pack, even though they are not original mesh entries.
        reserved = {"manifest.json", *(r["file"].casefold() for r in converted.values())}
        for extra in manifest.get("extras", []):
            filename = extra["file"]
            if not filename or filename in (".", "..") or Path(filename).name != filename or any(c in filename for c in "/\\:") or filename.casefold() in reserved:
                raise ValueError(f"Invalid or conflicting extra filename: {filename!r}")
            reserved.add(filename.casefold())
            original = (source / filename).read_bytes()
            digest = hashlib.sha256(original).hexdigest()
            if extra.get("sha256") != digest:
                raise ValueError(f"Source checksum mismatch: {filename}")
            (staging / filename).write_bytes(original)
        manifest["atlases"] = [{**record, **converted[record["key"]]} for record in records]
        manifest["image_format"] = "png"
        if version is not None:
            manifest["version"] = version
        manifest["compression"] = dict(lossless=True, files=len(converted),
            source_bytes=sum(r["source_bytes"] for r in converted.values()),
            compressed_bytes=sum(r["compressed_bytes"] for r in converted.values()))
        (staging / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        staging.rename(destination)
        return manifest["compression"]
    except BaseException:
        shutil.rmtree(staging, ignore_errors=True)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--version", help="Set the distribution pack version")
    args = parser.parse_args()
    if args.workers < 1:
        parser.error("--workers must be positive")
    print(json.dumps(convert(args.source, args.destination, args.workers, args.version), indent=2))
