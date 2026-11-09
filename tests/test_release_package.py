"""Release-Paket: positive und manipulierte ZIPs, komplett ohne Spiel und echte DLLs."""
import hashlib
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.verify_release_package import MANIFEST, REQUIRED_FILES, verify_package, is_x64_pe, validate_archive_name


def stub_pe() -> bytes:
    data = bytearray(256)
    data[:2] = b"MZ"
    data[0x3C:0x40] = (128).to_bytes(4, "little")
    data[128:132] = b"PE\x00\x00"
    data[132:134] = b"\x64\x86"
    return bytes(data)


class ReleasePackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "DDCoop-test.zip"
        self.payloads = {name: (stub_pe() if name.endswith(".dll") else (name + "\n").encode())
                         for name in REQUIRED_FILES}

    def make_zip(self, *, payloads=None, manifests=None, extra=None):
        payloads = self.payloads if payloads is None else payloads
        if manifests is None:
            manifests = [f"{hashlib.sha256(blob).hexdigest()}  {name}"
                         for name, blob in sorted(self.payloads.items())]
        with zipfile.ZipFile(self.path, "w") as archive:
            for name, blob in payloads.items():
                archive.writestr("DDCoop-test/" + name, blob)
            archive.writestr("DDCoop-test/" + MANIFEST, "\n".join(manifests) + "\n")
            for name, blob in extra or []:
                archive.writestr(name, blob)
        return self.path

    def test_good_package(self):
        self.assertEqual(set(verify_package(self.make_zip())), REQUIRED_FILES)

    def test_missing_file(self):
        changed = dict(self.payloads)
        changed.pop("Install.cmd")
        with self.assertRaisesRegex(ValueError, "Dateiliste"):
            verify_package(self.make_zip(payloads=changed))

    def test_changed_file_detected(self):
        changed = dict(self.payloads)
        changed["LIESMICH.txt"] = b"tampered"
        with self.assertRaisesRegex(ValueError, "SHA256"):
            verify_package(self.make_zip(payloads=changed))

    def test_forbidden_game_files(self):
        with self.assertRaisesRegex(ValueError, "Unerlaubte Datei"):
            verify_package(self.make_zip(extra=[("DDCoop-test/GameContent.pak", b"game")]))

    def test_traversal_and_backslash(self):
        # ZIP-Schreiber unter Windows normalisieren Backslashes. Darum die rohe
        # Eingabe separat pruefen und den ZIP-Rundtrip auf Ablehnung absichern.
        for name in ("DDCoop-test/../other.txt", "DDCoop-test/other\\evil.dll"):
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, "Pfad"):
                    validate_archive_name(name, "DDCoop-test/")
                with self.assertRaisesRegex(ValueError, "Pfad|Unerlaubte Datei"):
                    verify_package(self.make_zip(extra=[(name, b"x")]))

    def test_duplicate_file(self):
        with self.assertRaisesRegex(ValueError, "Doppelte"):
            verify_package(self.make_zip(extra=[("DDCoop-test/Install.cmd", b"x")]))

    def test_bad_manifest(self):
        with self.assertRaisesRegex(ValueError, "SHA256SUMS"):
            verify_package(self.make_zip(manifests=["not a hash"]))

    def test_invalid_pe_header(self):
        bad = dict(self.payloads)
        bad["DDCoop/DDCoop.dll"] = b"not a DLL"
        with self.assertRaisesRegex(ValueError, "x64-Windows-DLL"):
            verify_package(self.make_zip(payloads=bad))

    def test_pe_validator(self):
        self.assertTrue(is_x64_pe(stub_pe()))
        self.assertFalse(is_x64_pe(b"MZ" + bytes(70)))
        bad = bytearray(stub_pe())
        bad[132:134] = b"\x4c\x01"  # x86
        self.assertFalse(is_x64_pe(bytes(bad)))


if __name__ == "__main__":
    unittest.main()
