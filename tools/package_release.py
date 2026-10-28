# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
import shutil
import struct
import tempfile
import zipfile
from pathlib import Path
from urllib.request import urlopen
from xml.etree import ElementTree
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
METADATA = json.loads((ROOT / "project.json").read_text(encoding="utf-8"))
NAME = METADATA["name"]
VERSION = METADATA["version"]
DLL_NAME = METADATA["id"] + ".dll"
PLUGIN_NAME = NAME + ".esp"
PACKAGE_NAME = NAME + "-" + VERSION


def digest(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def records(data: bytes, start: int = 0, end: int | None = None):
    end = len(data) if end is None else end
    while start < end:
        if start + 24 > end:
            raise ValueError("Truncated ESP record header")
        signature, size = struct.unpack_from("<4sI", data, start)
        if signature == b"GRUP":
            if size < 24 or start + size > end:
                raise ValueError("Invalid ESP group size")
            yield from records(data, start + 24, start + size)
            start += size
        else:
            if start + 24 + size > end:
                raise ValueError("Invalid ESP record size")
            form_id = struct.unpack_from("<I", data, start + 12)[0]
            yield signature, form_id, data[start + 24:start + 24 + size]
            start += 24 + size


def subrecords(payload: bytes):
    cursor = 0
    while cursor < len(payload):
        if cursor + 6 > len(payload):
            raise ValueError("Truncated ESP subrecord")
        signature, size = struct.unpack_from("<4sH", payload, cursor)
        cursor += 6
        if cursor + size > len(payload):
            raise ValueError("Invalid ESP subrecord size")
        yield signature, payload[cursor:cursor + size]
        cursor += size


def validate_plugin(path: Path) -> None:
    data = path.read_bytes()
    entries = list(records(data))
    if not entries or entries[0][0] != b"TES4":
        raise ValueError("The addon must be a TES4 plugin")
    header = list(subrecords(entries[0][2]))
    masters = [value.rstrip(b"\0").decode("cp1252") for kind, value in header if kind == b"MAST"]
    if "Home Cloud Storage.esp" not in masters:
        raise ValueError("Home Cloud Storage.esp must be a master")
    if (b"CNAM", METADATA["author"].encode() + b"\0") not in header:
        raise ValueError("ESP author does not match project.json")
    expected_reference = (masters.index("Home Cloud Storage.esp") << 24) | 0x802
    lists = [(form, dict(subrecords(payload))) for kind, form, payload in entries if kind == b"FLST"]
    expected_form = (len(masters) << 24) | 0x800
    if not any(form == expected_form and fields.get(b"EDID") == b"HCSE_ContainerList\0"
               and fields.get(b"LNAM") == struct.pack("<I", expected_reference) for form, fields in lists):
        raise ValueError("HCSE_ContainerList must contain the real shared container")


def render_fomod(destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    values = {"NAME": NAME, "VERSION": VERSION, "AUTHOR": METADATA["author"]}
    for template in sorted((ROOT / "packaging" / "fomod").glob("*.in")):
        text = template.read_text(encoding="utf-8")
        for key, value in values.items():
            text = text.replace("@" + key + "@", escape(value))
        (destination / template.stem).write_text(text, encoding="utf-8")


def build_staging(dll: Path, destination: Path | None = None) -> Path:
    destination = destination or ROOT / "build" / "staging"
    resolved = destination.resolve()
    if not resolved.is_relative_to((ROOT / "build").resolve()) or resolved.name != "staging":
        raise ValueError("Staging must be named staging and located inside build/")
    if resolved.exists():
        shutil.rmtree(resolved)
    shutil.copytree(ROOT / "data", resolved / "core")
    plugins = resolved / "core" / "SKSE" / "Plugins"
    plugins.mkdir(parents=True, exist_ok=True)
    shutil.copy2(dll, plugins / DLL_NAME)
    shutil.copytree(ROOT / "licenses", resolved / "licenses")
    own = resolved / "licenses" / METADATA["id"]
    own.mkdir(parents=True)
    for name in ("LICENSE", "EXCEPTIONS.md"):
        shutil.copy2(ROOT / name, own / name)
    render_fomod(resolved / "fomod")
    validate_staging(resolved)
    return resolved


def validate_staging(staging: Path) -> None:
    core = staging / "core"
    expected = {PLUGIN_NAME, "SKSE/Plugins/" + DLL_NAME,
                "SKSE/Plugins/DynamicStringDistributor/skymoji.esp/HCSE_OII_Storage.json"}
    actual = {path.relative_to(core).as_posix() for path in core.rglob("*") if path.is_file()}
    if actual != expected:
        raise ValueError("Unexpected runtime files: " + str(actual ^ expected))
    if (core / "SKSE/Plugins" / DLL_NAME).read_bytes()[:2] != b"MZ":
        raise ValueError("DLL is not a PE image")
    validate_plugin(core / PLUGIN_NAME)
    info = ElementTree.parse(staging / "fomod/info.xml").getroot()
    if info.findtext("Author") != METADATA["author"] or info.findtext("Version") != VERSION:
        raise ValueError("FOMOD metadata does not match project.json")
    config = ElementTree.parse(staging / "fomod/ModuleConfig.xml").getroot()
    groups = config.findall("installSteps/installStep/optionalFileGroups/group")
    plugins = config.findall(".//plugin")
    folders = config.findall(".//folder")
    if (len(groups) != 1 or groups[0].get("type") != "SelectAll" or len(plugins) != 1
            or plugins[0].get("name") != "Install mod"
            or plugins[0].find("typeDescriptor/type").get("name") != "Required"
            or len(folders) != 1 or folders[0].get("source") != "core"
            or folders[0].get("destination") != "" or config.findall(".//file")):
        raise ValueError("FOMOD must install only core via one mandatory checkbox")
    for name in ("LICENSE", "EXCEPTIONS.md"):
        if not (staging / "licenses" / METADATA["id"] / name).is_file():
            raise ValueError("Missing addon license notice")
    patch = core / "SKSE/Plugins/DynamicStringDistributor/skymoji.esp/HCSE_OII_Storage.json"
    entries = json.loads(patch.read_text())
    if entries != [{"form_id": "0x800|Home Cloud Storage.esp", "type": "PERK EPFD", "index": 0,
                    "string": " <font face='$Iconographia'>V</font> <br> Storage "}]:
        raise ValueError("Invalid OII storage patch")


@contextmanager
def temporary_output(destination: Path):
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix=destination.name + ".", suffix=".tmp", dir=destination.parent)
    os.close(descriptor)
    temporary = Path(name)
    try:
        yield temporary
    finally:
        temporary.unlink(missing_ok=True)


def write_zip(destination: Path, files) -> None:
    with temporary_output(destination) as temporary:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as output:
            for path, name in sorted(files, key=lambda item: item[1]):
                output.write(path, name)
        temporary.replace(destination)


def write_archives(archives) -> None:
    directory = archives[0][0].parent
    if any(path.parent != directory for path, _ in archives):
        raise ValueError("Release archives must share one output directory")
    directory.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".release-", dir=directory))
    if temporary.resolve().parent != directory.resolve():
        raise ValueError("Temporary release directory escaped its output directory")
    prepared = temporary / "new"
    previous = temporary / "previous"
    cleanup = True
    try:
        prepared.mkdir()
        previous.mkdir()
        for path, files in archives:
            write_zip(prepared / path.name, files)
        for path, _ in archives:
            if path.exists():
                shutil.copy2(path, previous / path.name)
        updated = []
        try:
            for path, _ in archives:
                (prepared / path.name).replace(path)
                updated.append(path)
        except BaseException as error:
            failures = []
            for path in reversed(updated):
                try:
                    backup = previous / path.name
                    if backup.exists():
                        backup.replace(path)
                    else:
                        path.unlink(missing_ok=True)
                except OSError as restore_error:
                    failures.append(str(restore_error))
            if failures:
                cleanup = False
                raise RuntimeError(f"Could not restore the previous release; recovery files remain in {temporary}: "
                                   + "; ".join(failures)) from error
            raise
    finally:
        if cleanup:
            shutil.rmtree(temporary)


def download_source(destination: Path, url: str, checksum: str) -> None:
    print("Downloading dependency source: " + destination.name, flush=True)
    with temporary_output(destination) as temporary:
        with urlopen(url, timeout=60) as response, temporary.open("wb") as output:
            shutil.copyfileobj(response, output)
        if digest(temporary) != checksum:
            raise ValueError("Mismatched dependency source: " + destination.name)
        temporary.replace(destination)


def source_files():
    names = ["CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json", "project.json",
             ".clang-format", ".gitattributes", ".gitignore", "README.md", "LICENSE", "EXCEPTIONS.md"]
    result = [(ROOT / name, name) for name in names]
    for folder in ("src", "cmake", "data", "packaging", "tools", "tests", "licenses", ".github"):
        for path in (ROOT / folder).rglob("*"):
            relative = path.relative_to(ROOT)
            if path.is_file() and not {"__pycache__", "build"}.intersection(relative.parts) and path.suffix != ".pyc":
                result.append((path, relative.as_posix()))
    return result


def dependency_sources(directory: Path, download: bool = False):
    manifest = json.loads((ROOT / "packaging/dependency-sources.json").read_text())
    result = []
    for entry in manifest:
        path = directory / entry["file"]
        if not path.exists() and download:
            download_source(path, entry["url"], entry["sha256"])
        if not path.is_file() or digest(path) != entry["sha256"]:
            raise ValueError("Missing or mismatched dependency source: " + str(path))
        result.append((path, "dependencies/sources/" + path.name))
    return result


def main():
    parser = argparse.ArgumentParser(description="Package the mod and matching source with dependency notices.")
    parser.add_argument("--dll", type=Path, default=ROOT / "build/release" / DLL_NAME)
    bundled_sources = ROOT / "dependencies/sources"
    default_sources = bundled_sources if bundled_sources.is_dir() else Path(os.environ.get("VCPKG_DOWNLOADS", ROOT / "build/vcpkg-downloads"))
    parser.add_argument("--dependency-sources", type=Path, default=default_sources)
    parser.add_argument("--download-sources", action="store_true", help="Fetch missing pinned dependency sources")
    args = parser.parse_args()
    dependencies = dependency_sources(args.dependency_sources, args.download_sources)
    staging = build_staging(args.dll)
    archives = [
        (ROOT / "dist" / (PACKAGE_NAME + ".zip"),
         [(path, path.relative_to(staging).as_posix()) for path in staging.rglob("*") if path.is_file()]),
        (ROOT / "dist" / (PACKAGE_NAME + "-Sources.zip"), source_files() + dependencies),
    ]
    write_archives(archives)
    for path, _ in archives:
        print(f"Created: {path} ({path.stat().st_size} bytes)\nSHA256: {digest(path)}")


if __name__ == "__main__":
    main()
