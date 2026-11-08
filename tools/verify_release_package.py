"""Prueft ein DDCoop-Release-ZIP offline (Inhalt, PE-Dateien, SHA256SUMS, ZIP-Pfade).

Keine Steam-/Spieldateien, Internetanfragen oder installierte Mod erforderlich.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import stat
import zipfile

REQUIRED_FILES = frozenset({
    "version.dll",
    "DDCoop/DDCoop.dll",
    "Install.cmd",
    "Uninstall.cmd",
    "Install-DDCoop.ps1",
    "LIESMICH.txt",
    "LICENSE.txt",
    "third_party/minhook-LICENSE.txt",
})
MANIFEST = "SHA256SUMS.txt"
MAX_FILE_SIZE = 200 * 1024 * 1024
HASH_LINE = re.compile(r"([0-9a-f]{64})  ([A-Za-z0-9_./-]+)\Z")


def is_x64_pe(payload: bytes) -> bool:
    """DOS- und COFF-Header pruefen, ohne fremde DLLs zu laden."""
    if len(payload) < 64 or payload[:2] != b"MZ":
        return False
    location = int.from_bytes(payload[0x3C:0x40], "little")
    return 64 <= location <= len(payload) - 6 and (
        payload[location:location + 4] == b"PE\x00\x00"
        and payload[location + 4:location + 6] == b"\x64\x86"
    )


def validate_archive_name(name: str, prefix: str) -> str:
    """Rohpfad vor der Pfad-/Dateilistenpruefung ablehnen; ZIP-Schreiber normalisieren teils Backslashes."""
    if "\\" in name or not name.startswith(prefix) or name.startswith("/") or "//" in name:
        raise ValueError(f"Ungueltiger ZIP-Pfad: {name!r}")
    suffix = name[len(prefix):]
    parts = suffix.rstrip("/").split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ValueError(f"Unsicherer ZIP-Pfad: {name!r}")
    return suffix


def verify_package(filename: str | Path) -> dict[str, str]:
    path = Path(filename)
    if not re.fullmatch(r"DDCoop-[A-Za-z0-9._-]+", path.stem):
        raise ValueError("Paketname muss DDCoop-<Version>.zip sein")
    if path.suffix.lower() != ".zip":
        raise ValueError("Paket ist keine ZIP-Datei")
    prefix = path.stem + "/"
    allowed = REQUIRED_FILES | {MANIFEST}
    valid_directories = {prefix, prefix + "DDCoop/", prefix + "third_party/"}
    files: dict[str, bytes] = {}
    with zipfile.ZipFile(path) as archive:
        for entry in archive.infolist():
            name = entry.filename
            suffix = validate_archive_name(name, prefix)
            if stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError(f"Symlink im Paket: {name!r}")
            if entry.is_dir():
                if name not in valid_directories:
                    raise ValueError(f"Unerwartetes Verzeichnis: {name!r}")
                continue
            if suffix not in allowed:
                raise ValueError(f"Unerlaubte Datei im Paket: {suffix}")
            if suffix in files:
                raise ValueError(f"Doppelte ZIP-Datei: {suffix}")
            if entry.file_size == 0 or entry.file_size > MAX_FILE_SIZE:
                raise ValueError(f"Leere oder zu grosse Datei: {suffix}")
            files[suffix] = archive.read(entry)
    if files.keys() != allowed:
        raise ValueError(f"Dateiliste stimmt nicht: fehlend={sorted(allowed - files.keys())}, extra={sorted(files.keys() - allowed)}")
    for dll in ("version.dll", "DDCoop/DDCoop.dll"):
        if not is_x64_pe(files[dll]):
            raise ValueError(f"Keine x64-Windows-DLL: {dll}")
    try:
        lines = files[MANIFEST].decode("utf-8-sig").splitlines()
    except UnicodeDecodeError as exc:
        raise ValueError("SHA256SUMS.txt ist kein UTF-8") from exc
    if len(lines) != len(REQUIRED_FILES):
        raise ValueError("SHA256SUMS.txt hat die falsche Anzahl Eintraege")
    expected: dict[str, str] = {}
    for line in lines:
        match = HASH_LINE.fullmatch(line)
        if not match:
            raise ValueError(f"Ungueltige SHA256SUMS-Zeile: {line!r}")
        digest, name = match.groups()
        if name in expected:
            raise ValueError(f"Doppelter SHA256-Eintrag: {name}")
        expected[name] = digest
    if expected.keys() != REQUIRED_FILES:
        raise ValueError("SHA256SUMS.txt enthaelt falsche oder fehlende Dateinamen")
    for name, claimed in expected.items():
        actual = hashlib.sha256(files[name]).hexdigest()
        if claimed != actual:
            raise ValueError(f"SHA256 stimmt nicht fuer {name}")
    return expected


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="DDCoop-<Version>.zip")
    args = parser.parse_args()
    try:
        result = verify_package(args.archive)
    except (ValueError, OSError, zipfile.BadZipFile) as exc:
        parser.exit(1, f"Paketpruefung fehlgeschlagen: {exc}\n")
    print(f"Paket geprueft: {args.archive} ({len(result)} Dateien, zwei x64-DLLs, SHA256 OK)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
