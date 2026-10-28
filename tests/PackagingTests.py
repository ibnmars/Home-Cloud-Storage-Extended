# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
import importlib.util
import hashlib
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("package_release", ROOT / "tools/package_release.py")
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackagingTests(unittest.TestCase):
    def test_addon_formlist(self):
        package.validate_plugin(ROOT / "data" / package.PLUGIN_NAME)

    def test_installer_keeps_licenses_outside_data(self):
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "build") as temporary:
            directory = Path(temporary)
            dll = directory / "fixture.dll"
            dll.write_bytes(b"MZ" + bytes(64))
            staging = package.build_staging(dll, directory / "staging")
            package.validate_staging(staging)
            config = ElementTree.parse(staging / "fomod/ModuleConfig.xml").getroot()
            self.assertEqual([folder.get("source") for folder in config.findall(".//folder")], ["core"])
            self.assertTrue((staging / "licenses" / package.METADATA["id"] / "LICENSE").is_file())
            self.assertFalse((staging / "core/licenses").exists())
            self.assertEqual((staging / "core" / package.PLUGIN_NAME).read_bytes(),
                             (ROOT / "data" / package.PLUGIN_NAME).read_bytes())

    def test_rejects_unsafe_staging(self):
        with self.assertRaises(ValueError):
            package.build_staging(ROOT / "missing.dll", ROOT / "src")
        with self.assertRaises(ValueError):
            package.build_staging(ROOT / "missing.dll", ROOT / "build")
        with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "build") as temporary:
            directory = Path(temporary) / "release"
            directory.mkdir()
            sentinel = directory / "keep.txt"
            sentinel.write_text("preserved")
            with self.assertRaises(ValueError):
                package.build_staging(ROOT / "missing.dll", directory)
            self.assertEqual(sentinel.read_text(), "preserved")

    def test_failed_zip_preserves_previous_archive(self):
        with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "build") as temporary:
            archive = Path(temporary) / "release.zip"
            archive.write_bytes(b"previous release")
            with self.assertRaises(FileNotFoundError):
                package.write_zip(archive, [(Path(temporary) / "missing", "missing")])
            self.assertEqual(archive.read_bytes(), b"previous release")
            self.assertEqual(list(Path(temporary).iterdir()), [archive])

    def test_failed_second_archive_keeps_previous_pair(self):
        with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "build") as temporary:
            directory = Path(temporary)
            runtime = directory / "runtime.zip"
            sources = directory / "sources.zip"
            runtime.write_bytes(b"previous runtime")
            sources.write_bytes(b"previous sources")
            fixture = directory / "fixture.txt"
            fixture.write_text("new content")
            with self.assertRaises(FileNotFoundError):
                package.write_archives([(runtime, [(fixture, "fixture.txt")]),
                                        (sources, [(directory / "missing", "missing")])])
            self.assertEqual(runtime.read_bytes(), b"previous runtime")
            self.assertEqual(sources.read_bytes(), b"previous sources")
            self.assertFalse(list(directory.glob(".release-*")))

    def test_failed_publish_restores_previous_pair(self):
        for existing in (True, False):
            with (self.subTest(existing=existing),
                  tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "build") as temporary):
                directory = Path(temporary)
                runtime = directory / "runtime.zip"
                sources = directory / "sources.zip"
                if existing:
                    runtime.write_bytes(b"previous runtime")
                    sources.write_bytes(b"previous sources")
                fixture = directory / "fixture.txt"
                fixture.write_text("new content")
                original = Path.replace

                def fail_second_publish(path, destination):
                    if destination == sources:
                        raise OSError("simulated publish failure")
                    return original(path, destination)

                with patch.object(Path, "replace", fail_second_publish):
                    with self.assertRaises(OSError):
                        package.write_archives([(runtime, [(fixture, "fixture.txt")]),
                                                (sources, [(fixture, "fixture.txt")])])
                if existing:
                    self.assertEqual(runtime.read_bytes(), b"previous runtime")
                    self.assertEqual(sources.read_bytes(), b"previous sources")
                else:
                    self.assertFalse(runtime.exists())
                    self.assertFalse(sources.exists())
                self.assertFalse(list(directory.glob(".release-*")))

    def test_failed_restore_preserves_recovery_files(self):
        with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "build") as temporary:
            directory = Path(temporary)
            runtime = directory / "runtime.zip"
            sources = directory / "sources.zip"
            runtime.write_bytes(b"previous runtime")
            sources.write_bytes(b"previous sources")
            fixture = directory / "fixture.txt"
            fixture.write_text("new content")
            original = Path.replace
            runtime_attempts = 0

            def fail_publish_and_restore(path, destination):
                nonlocal runtime_attempts
                if destination == sources:
                    raise OSError("simulated publish failure")
                if destination == runtime:
                    runtime_attempts += 1
                    if runtime_attempts == 2:
                        raise OSError("simulated restore failure")
                return original(path, destination)

            with patch.object(Path, "replace", fail_publish_and_restore):
                with self.assertRaises(RuntimeError):
                    package.write_archives([(runtime, [(fixture, "fixture.txt")]),
                                            (sources, [(fixture, "fixture.txt")])])
            recovery = list(directory.glob(".release-*"))
            self.assertEqual(len(recovery), 1)
            self.assertEqual((recovery[0] / "previous/runtime.zip").read_bytes(), b"previous runtime")
            self.assertEqual((recovery[0] / "previous/sources.zip").read_bytes(), b"previous sources")

    def test_dependency_download_checks_digest_before_replacing(self):
        with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "build") as temporary:
            archive = Path(temporary) / "dependency.tar.gz"
            payload = b"pinned dependency source"
            checksum = hashlib.sha256(payload).hexdigest()
            with patch.object(package, "urlopen", return_value=io.BytesIO(payload)):
                package.download_source(archive, "https://example.invalid/source", checksum)
            self.assertEqual(archive.read_bytes(), payload)
            with patch.object(package, "urlopen", return_value=io.BytesIO(b"wrong source")):
                with self.assertRaises(ValueError):
                    package.download_source(archive, "https://example.invalid/source", checksum)
            self.assertEqual(archive.read_bytes(), payload)
            self.assertEqual(list(Path(temporary).iterdir()), [archive])

    def test_source_archive_is_self_contained(self):
        files = dict((name, path) for path, name in package.source_files())
        for name in ("project.json", "CMakeLists.txt", ".gitattributes", "data/" + package.PLUGIN_NAME,
                     "tools/package_release.py", "packaging/fomod/ModuleConfig.xml.in",
                     "src/CraftingSession.cpp", "tests/CraftingTests.cpp",
                     "licenses/scie/LICENSE.txt", "licenses/commonlibsse-ng/COPYING.txt"):
            self.assertIn(name, files)
        for name in files:
            self.assertFalse(name.startswith(("build/", "build-tools/", "dist/", "dependencies/")))
            self.assertNotIn(Path(name).suffix, (".dll", ".pdb", ".pch", ".obj"))


if __name__ == "__main__":
    unittest.main()
