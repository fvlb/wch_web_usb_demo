#!/usr/bin/env python3
"""Save an official WCH EVT subtree as a revision-pinned, CRC-checked ZIP."""
import argparse
import hashlib
import json
import pathlib
import shutil
import tempfile
import urllib.request
import zipfile


def archive(repo, revision, output):
    if repo not in ("ch585", "ch32v307", "ch32v20x"):
        raise ValueError("Only the listed official openwch repositories are supported")
    if len(revision) != 40 or any(c not in "0123456789abcdef" for c in revision):
        raise ValueError("Use a full hexadecimal commit hash")
    destination = output / f"{repo.upper()}_EVT_{revision[:12]}.zip"
    if destination.exists():
        raise FileExistsError(destination)
    url = f"https://codeload.github.com/openwch/{repo}/zip/{revision}"
    prefix = f"{repo}-{revision}/EVT/"
    files = 0
    # Temporary downloads are cleaned up even when networking or ZIP validation fails.
    with tempfile.TemporaryDirectory() as tmp:
        download = pathlib.Path(tmp) / "upstream.zip"
        request = urllib.request.Request(url, headers={"User-Agent": "wch-usb-demo-EVT-archive"})
        with urllib.request.urlopen(request, timeout=120) as response, download.open("wb") as target:
            shutil.copyfileobj(response, target)
        with zipfile.ZipFile(download) as source, zipfile.ZipFile(
            destination, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6,
        ) as target:
            try:
                for entry in source.infolist():
                    if not entry.filename.startswith(prefix):
                        continue
                    name = "EVT/" + entry.filename[len(prefix):]
                    info = zipfile.ZipInfo(name, entry.date_time)
                    info.external_attr = entry.external_attr
                    info.compress_type = zipfile.ZIP_DEFLATED
                    with source.open(entry) as reader, target.open(info, "w") as writer:
                        shutil.copyfileobj(reader, writer)
                    files += not entry.is_dir()
                if not files:
                    raise ValueError("The downloaded revision has no EVT files")
                metadata = {
                    "repository": f"https://github.com/openwch/{repo}",
                    "revision": revision,
                    "subtree": "EVT/",
                    "source_archive": url,
                    "files": files,
                }
                target.writestr("UPSTREAM.json", json.dumps(metadata, indent=2) + "\n")
            except BaseException:
                destination.unlink(missing_ok=True)
                raise
    with zipfile.ZipFile(destination) as saved:
        bad = saved.testzip()
        if bad:
            raise ValueError(f"ZIP CRC failure: {bad}")
    with destination.open("rb") as saved:
        digest = hashlib.file_digest(saved, "sha256").hexdigest()
    destination.with_suffix(".zip.sha256").write_text(f"{digest}  {destination.name}\n")
    print(json.dumps({"path": str(destination), "files": files,
                      "bytes": destination.stat().st_size, "sha256": digest}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repo", choices=("ch585", "ch32v307", "ch32v20x"))
    parser.add_argument("revision", help="Full upstream Git commit hash")
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if not args.output.is_dir():
        parser.error("--output must be an existing directory")
    archive(args.repo, args.revision, args.output)


if __name__ == "__main__":
    main()
