"""Statische Regressionstests fuer die Registry der Game-Patches ohne Spiel-EXE.

Prueft doppelte Patch-Listen, Check-/Apply-Verweise, Site-Arrays und die zentrale
Signaturtabelle. Die Eindeutigkeit der Maschinenbytes in einer echten Spiel-EXE
prueft weiterhin tools/patch_signatures.py (nicht diese reine Quelltextpruefung).
"""
from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PATCH_LIST = re.compile(r"\bconst\s+(?:Patches::)?Patch\s+List\s*\[\s*\]\s*=\s*\{")
ENTRY = re.compile(
    r'\{\s*"([^"]+)"\s*,\s*&?(\w+)\s*,\s*&?(\w+)'
    r'\s*(?:,\s*(\w+Sites))?\s*\}\s*,?'
)
SITES = re.compile(r"\b(?:constexpr|const)\s+uintptr_t\s+(\w+Sites)\s*\[\s*\]\s*=\s*\{([^}]*)\}", re.S)
ADDRESS = re.compile(r"\bconstexpr\s+uintptr_t\s+(\w+)\s*=\s*(0x[0-9a-fA-F]+)\s*;")
SIGNATURE = re.compile(r"^\{\s*(0x[0-9a-fA-F]+)\s*,\s*(\d+)\s*,", re.M)
EXPORT = re.compile(r"\bstd::span\s*<\s*const\s+Patch\s*>\s+\w+Patches\s*\(\s*\)")


def signature_references(content: str) -> tuple[set[int], list[str]]:
    errors: list[str] = []
    rows = [(int(ref, 16), int(build)) for ref, build in SIGNATURE.findall(content)]
    if not rows:
        return set(), ["PatchSignatures.inc enthaelt keine Signaturen"]
    if rows != sorted(rows):
        errors.append("PatchSignatures.inc: unsortierte (RVA, Build)-Zeilen")
    if len(rows) != len(set(rows)):
        errors.append("PatchSignatures.inc: doppelte (RVA, Build)-Zeilen")
    return {ref for ref, _ in rows}, errors


def check_patch(path: str, content: str, signatures: set[int]) -> list[str]:
    errors: list[str] = []
    registries = list(PATCH_LIST.finditer(content))
    if not registries:
        if EXPORT.search(content):
            errors.append(f"{path}: Exportfunktion ohne Patch List[]")
        return errors
    if len(registries) != 1:
        return [f"{path}: Patch List[] ist mehrfach definiert ({len(registries)})"]
    start = registries[0].end()
    end = content.find("};", start)
    if end < 0:
        return [f"{path}: Patch List[] ist nicht abgeschlossen"]
    block = content[start:end]
    entries = list(ENTRY.finditer(block))
    expected = len(re.findall(r'(?m)^\s*\{\s*"', block))
    if not entries or len(entries) != expected:
        errors.append(f"{path}: Patch List[] enthaelt ungueltige oder fehlende Eintraege")
    declared_sites = {}
    for name, value in SITES.findall(content):
        if name in declared_sites:
            errors.append(f"{path}: Site-Array mehrfach definiert: {name}")
        declared_sites[name] = [token.strip() for token in value.split(",") if token.strip()]
    addresses = {name: int(addr, 16) for name, addr in ADDRESS.findall(content)}
    used_sites = set()
    labels = set()
    for match in entries:
        label, check, apply, site_array = match.groups()
        if label in labels:
            errors.append(f"{path}: Patchname doppelt: {label}")
        labels.add(label)
        for callback in (check, apply):
            if not re.search(r"\b" + re.escape(callback) + r"\s*\(", content):
                errors.append(f"{path}: Callback fehlt: {callback}")
        if not site_array:
            continue  # Vtable-Hooks ohne Code-Site sind erlaubt.
        used_sites.add(site_array)
        if site_array not in declared_sites:
            errors.append(f"{path}: Site-Array fehlt: {site_array}")
            continue
        for symbol in declared_sites[site_array]:
            if re.fullmatch(r"0x[0-9a-fA-F]+", symbol):
                address = int(symbol, 16)
            elif symbol in addresses:
                address = addresses[symbol]
            else:
                errors.append(f"{path}: Site-Adresse unbekannt: {symbol}")
                continue
            if address not in signatures:
                errors.append(f"{path}: RVA 0x{address:X} ({symbol}) ohne Signatur")
    for unused in declared_sites.keys() - used_sites:
        errors.append(f"{path}: Site-Array nicht verwendet: {unused}")
    return errors


def check_sources(sources: dict[str, str], signatures_content: str) -> list[str]:
    refs, errors = signature_references(signatures_content)
    for name, content in sorted(sources.items()):
        errors.extend(check_patch(name, content, refs))
    return errors


def check_repository(root: Path = ROOT) -> list[str]:
    src = root / "src" / "Game"
    return check_sources(
        {path.name: path.read_text(encoding="utf-8") for path in src.glob("*Patches.cpp")},
        (src / "PatchSignatures.inc").read_text(encoding="utf-8"),
    )


if __name__ == "__main__":
    issues = check_repository()
    if issues:
        raise SystemExit("\n".join(issues))
    print("Patch-Registry und Signaturverweise: OK")
