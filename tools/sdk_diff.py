"""Vergleicht zwei lokale Dumper-7-SDK-Ausgaben für alles, was die Mod nutzt.

    py tools/sdk_diff.py --old <sdk-alt> --new <sdk-neu> [--repo .] [--all] [--json]

<sdk-alt>/<sdk-neu> ist der Ordner von Dumper-7 (mit CppSDK/SDK), CppSDK oder SDK selbst. Gelesen werden nur
Basic.hpp, *_classes.hpp und *_structs.hpp. Die Ausgabe nennt Klassen-, Feld- und Funktionsnamen mit Offsets, nie SDK-Inhalte
darüber hinaus; nichts davon wird ins Repo geschrieben.

Was die Mod nutzt, kommt aus dem Repo:
- src/Game/SdkChecks.cpp: offsetof(SDK::Klasse, Feld)
- src/ und docs/OFFSETS.md: Verweise der Form Klasse.Feld oder Klasse.Funktion (etwa UGameplayAbility.bIsActive)
- src/: FindObject("Funktion", "Klasse"), Function("Funktion", "Klasse"), FieldSpec{"/Script/Paket", "Klasse", "Feld", ...}
- src/: Zeichenketten, die genau einen Klassennamen tragen (etwa "BP_DD_Character_C")
Ein Verweis zählt nur, wenn mindestens eines der beiden SDKs die Klasse kennt.

Exitcode 0: nichts verschoben oder verschwunden, 1: mindestens eine Abweichung, 2: Aufruffehler.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

CLASS_HEAD = re.compile(r"^(?:class|struct)\s+(?:alignas\(\w+\)\s+)?(?P<name>[A-Za-z_]\w*)(?:\s+final)?\s*(?::\s*(?P<bases>[^{]+))?\s*$")
SIZE_COMMENT = re.compile(r"^//\s*0x[0-9A-Fa-f]+\s*\(0x(?P<size>[0-9A-Fa-f]+)\s*-\s*0x[0-9A-Fa-f]+\)")
MEMBER = re.compile(
    r"^\s+(?P<type>[^;/(]+?)\s+(?P<name>[A-Za-z_]\w*)(?:\[[^\]]*\])?(?:\s*:\s*\d+)?;\s*"
    r"//\s*0x(?P<off>[0-9A-Fa-f]+)\(0x(?P<size>[0-9A-Fa-f]+)\)(?P<rest>.*)$"
)
BIT_INDEX = re.compile(r"BitIndex:\s*0x(?P<bit>[0-9A-Fa-f]+)")
FUNCTION = re.compile(r"^\s+(?:static\s+)?(?:[\w:<>*&,\s]+?)\s+\*?(?P<name>[A-Za-z_]\w*)\((?P<params>[^)]*)\)(?:\s*const)?;\s*$")
SKIP_FUNCTIONS = {"StaticClass", "GetDefaultObj", "StaticName"}
PREFIXES = ("U", "A", "F", "E", "I")


@dataclass
class Member:
    offset: int
    size: int
    bit: int | None = None

    def text(self) -> str:
        bit = f" Bit {self.bit}" if self.bit is not None else ""
        return f"0x{self.offset:X}(0x{self.size:X}){bit}"


@dataclass
class ClassInfo:
    name: str
    size: int | None = None
    bases: list[str] = field(default_factory=list)
    members: dict[str, Member] = field(default_factory=dict)
    functions: dict[str, str] = field(default_factory=dict)  # Name -> Parameterliste


class Sdk:
    def __init__(self, classes: dict[str, ClassInfo]):
        self.classes = classes
        self.short: dict[str, str] = {}
        for name in classes:
            self.short.setdefault(name, name)
            if len(name) > 1 and name[0] in PREFIXES and name[1].isupper():
                self.short.setdefault(name[1:], name)

    def resolve(self, name: str) -> ClassInfo | None:
        key = self.short.get(name)
        return self.classes.get(key) if key else None

    def find(self, cls: ClassInfo, member: str, kind: str):
        """Sucht ein Feld oder eine Funktion auch in den Oberklassen (wie die Reflexion der Mod)."""
        seen = set()
        while cls and cls.name not in seen:
            seen.add(cls.name)
            table = cls.members if kind == "field" else cls.functions
            if member in table:
                return table[member]
            cls = next((self.classes[b] for b in cls.bases if b in self.classes), None)
        return None


def sdk_dir(path: Path) -> Path:
    for candidate in (path / "CppSDK" / "SDK", path / "SDK", path):
        if candidate.is_dir() and any(candidate.glob("*_classes.hpp")):
            return candidate
    raise SystemExit(f"Kein Dumper-7-SDK (*_classes.hpp) unter {path}")


def parse_text(text: str, classes: dict[str, ClassInfo]) -> None:
    lines = text.splitlines()
    current: ClassInfo | None = None
    depth = 0
    pending_size: int | None = None
    for line in lines:
        stripped = line.strip()
        if current is None:
            size = SIZE_COMMENT.match(stripped)
            if size:
                pending_size = int(size.group("size"), 16)
                continue
            head = CLASS_HEAD.match(stripped)
            if head:
                bases = []
                if head.group("bases"):
                    for part in head.group("bases").split(","):
                        words = part.replace("public", " ").replace("private", " ").replace("protected", " ").split()
                        if words:
                            bases.append(words[-1])
                current = classes.setdefault(head.group("name"), ClassInfo(head.group("name")))
                current.size, current.bases = pending_size, bases
                depth, pending_size = 0, None
            elif stripped and not stripped.startswith("//"):
                pending_size = None
            continue
        depth += line.count("{") - line.count("}")
        if depth <= 0 and "}" in line:
            current = None
            continue
        if depth != 1:
            continue
        member = MEMBER.match(line)
        if member and not member.group("type").lstrip().startswith("static"):
            name = member.group("name")
            if not name.startswith(("Pad_", "UnknownData")):
                bit = BIT_INDEX.search(member.group("rest"))
                current.members[name] = Member(int(member.group("off"), 16), int(member.group("size"), 16),
                                               int(bit.group("bit"), 16) if bit else None)
            continue
        function = FUNCTION.match(line)
        if function and function.group("name") not in SKIP_FUNCTIONS:
            current.functions[function.group("name")] = " ".join(function.group("params").split())


def load_sdk(path: Path) -> Sdk:
    classes: dict[str, ClassInfo] = {}
    for file in sorted(sdk_dir(path).glob("*.hpp")):
        if file.name.endswith(("_classes.hpp", "_structs.hpp")) or file.name == "Basic.hpp":
            parse_text(file.read_text(encoding="utf-8", errors="replace"), classes)
    return Sdk(classes)


# ---------------------------------------------------------------- Nutzung im Repo

DOTTED = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\.([A-Za-z_]\w*)\b")
OFFSETOF = re.compile(r"offsetof\(SDK::(\w+),\s*(\w+)\)")
FIND_OBJECT = re.compile(r"\b(?:FindObject|Function)\(\s*\"(\w+)\",\s*\"(\w+)\"")
FIELD_SPEC = re.compile(r"\"/Script/\w+\",\s*\"(\w+)\",\s*\"(\w+)\"")
LITERAL = re.compile(r"\"([A-Za-z_]\w{3,})\"")


@dataclass(frozen=True)
class Use:
    cls: str
    member: str | None  # None: nur die Klasse
    kind: str           # field, function, class, member (Feld oder Funktion)


def collect_uses(repo: Path, sdks: list[Sdk]) -> dict[Use, str]:
    """Liefert jede Nutzung mit der ersten Fundstelle (Datei:Zeile)."""
    def known(cls: str) -> bool:
        return any(s.resolve(cls) for s in sdks)

    uses: dict[Use, str] = {}

    def add(use: Use, where: str) -> None:
        uses.setdefault(use, where)

    files = sorted(p for p in (repo / "src").rglob("*") if p.suffix in (".cpp", ".h", ".inc"))
    offsets_md = repo / "docs" / "OFFSETS.md"
    if offsets_md.is_file():
        files.append(offsets_md)
    for path in files:
        rel = path.relative_to(repo).as_posix()
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            where = f"{rel}:{number}"
            for cls, member in OFFSETOF.findall(line):
                add(Use(cls, member, "field"), where)
            for name, cls in FIND_OBJECT.findall(line):
                if known(cls):
                    add(Use(cls, name, "function"), where)
            for cls, name in FIELD_SPEC.findall(line):
                if known(cls):
                    add(Use(cls, name, "field"), where)
            for cls, member in DOTTED.findall(line):
                if known(cls):
                    add(Use(cls, member, "member"), where)
            if path.suffix != ".md":
                for literal in LITERAL.findall(line):
                    if known(literal):
                        add(Use(literal, None, "class"), where)
    return uses


# ---------------------------------------------------------------- Vergleich

def lookup(sdk: Sdk, use: Use):
    cls = sdk.resolve(use.cls)
    if cls is None:
        return None, None
    if use.member is None:
        return "class", cls
    for kind in (("field", "function") if use.kind == "member" else (use.kind,)):
        found = sdk.find(cls, use.member, kind)
        if found is not None:
            return kind, found
    return None, None


def describe(kind, value, signature: bool) -> str:
    if kind == "class":
        return f"Größe 0x{value.size:X}" if value.size is not None else "vorhanden"
    if kind == "field":
        return value.text()
    return f"({value})" if signature else "vorhanden"


def canonical(old: Sdk, new: Sdk, cls: str) -> str:
    found = old.resolve(cls) or new.resolve(cls)
    return found.name if found else cls


def compare(old: Sdk, new: Sdk, uses: dict[Use, str]) -> list[dict]:
    rows, seen = [], set()
    for use, where in sorted(uses.items(), key=lambda item: (canonical(old, new, item[0].cls), item[0].member or "", item[1])):
        old_kind, old_value = lookup(old, use)
        new_kind, new_value = lookup(new, use)
        key = (canonical(old, new, use.cls), use.member)
        if key in seen or (old_kind is None and new_kind is None and use.kind == "member"):
            continue  # doppelt (etwa Actor und AActor) oder Text wie "Memory.Read", den kein SDK kennt
        seen.add(key)
        if old_kind is None and new_kind is None:
            status = "unbekannt"
        elif old_kind is None:
            status = "neu"
        elif new_kind is None:
            status = "verschwunden"
        elif old_kind != new_kind:
            status = "verschoben"
        elif old_kind == "class":
            status = "unverändert" if old_value.size == new_value.size else "Größe geändert"
        elif old_kind == "field":
            status = "unverändert" if old_value == new_value else "verschoben"
        else:
            status = "unverändert" if old_value == new_value else "Signatur geändert"
        signature = status == "Signatur geändert"
        rows.append({
            "status": status,
            "name": key[0] + (f".{use.member}" if use.member else ""),
            "kind": (old_kind or new_kind or use.kind),
            "old": describe(old_kind, old_value, signature) if old_kind else "-",
            "new": describe(new_kind, new_value, signature) if new_kind else "-",
            "where": where,
        })
    return rows


ORDER = ["verschwunden", "verschoben", "Signatur geändert", "Größe geändert", "unbekannt", "neu", "unverändert"]
FAILING = {"verschwunden", "verschoben", "Signatur geändert"}


def report(rows: list[dict], show_all: bool) -> str:
    counts = {status: sum(1 for r in rows if r["status"] == status) for status in ORDER}
    out = ["Zusammenfassung: " + ", ".join(f"{counts[s]} {s}" for s in ORDER if counts[s])]
    for status in ORDER:
        group = [r for r in rows if r["status"] == status]
        if not group or (status == "unverändert" and not show_all):
            continue
        out.append(f"\n{status} ({len(group)})")
        for r in group:
            change = r["old"] if r["old"] == r["new"] else f"{r['old']} -> {r['new']}"
            out.append(f"  {r['name']:<55} {r['kind']:<8} {change:<32} {r['where']}")
    if counts["unverändert"] and not show_all:
        out.append(f"\n{counts['unverändert']} unverändert (mit --all anzeigen)")
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Vergleicht zwei Dumper-7-SDKs für alles, was die Mod nutzt.")
    parser.add_argument("--old", required=True, type=Path, help="SDK des alten Builds")
    parser.add_argument("--new", required=True, type=Path, help="SDK des neuen Builds")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1], help="Mod-Checkout")
    parser.add_argument("--all", action="store_true", help="auch Unverändertes auflisten")
    parser.add_argument("--json", action="store_true", help="Ergebnis als JSON")
    args = parser.parse_args(argv)
    old, new = load_sdk(args.old), load_sdk(args.new)
    rows = compare(old, new, collect_uses(args.repo, [old, new]))
    if args.json:
        print(json.dumps(rows, ensure_ascii=False, indent=1))
    else:
        print(report(rows, args.all))
    return 1 if any(r["status"] in FAILING for r in rows) else 0


if __name__ == "__main__":
    sys.exit(main())
