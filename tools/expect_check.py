"""Byte-Pruefungen der Patches (Code::Expect) offline gegen eine lokale Exe pruefen, ohne das Spiel zu starten.

Liest Code::Expect(At(X[, Offset]) [+/- n], {Bytes}, "Ort") und ExpectBranch(At(X), CallRel32|JumpRel32, Ziel, "Ort") aus src/Game, rechnet X ueber die Tabelle
src/Game/RvaMap<build>.inc um (wie Game::AddressMap) und vergleicht mit der Exe. Liest nur lokale Dateien.

  py tools/expect_check.py --exe <exe> --build 26495302 [--file ClientPatches.cpp ...]

Ausgabe je Pruefung: ok, abweichend (mit gelesenen Bytes), fehlt (Adresse ohne Tabelleneintrag) oder
uebersprungen (Ausdruck ohne At, etwa lokale Variablen; diese prueft nur das Spiel) oder anderer-build (die Startzeile
der Pruefung endet mit "// Build <id>" eines anderen Builds, etwa in einer Verzweigung nach Core::ActiveBuild()).
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rva_map import Image, NAMED  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
GAME = REPO / 'src' / 'Game'
REFERENCE_BUILD = 26187885
EXPECT = re.compile(r'Code::Expect\(\s*(?P<site>[^{;]*?)\s*,\s*\{(?P<bytes>[^}]*)\}\s*,\s*(?://[^\n]*\s*)?"(?P<where>[^"]*)"', re.DOTALL)
SITE = re.compile(r'^(?:Patches::)?At\(\s*(?P<base>\w+)\s*(?:,\s*(?P<offset>\w+)\s*)?\)\s*(?:(?P<sign>[+-])\s*(?P<delta>\w+))?$')
BRANCH = re.compile(r'ExpectBranch\(\s*(?P<site>[^;]*?)\s*,\s*(?P<op>CallRel32|JumpRel32)\s*,\s*(?P<target>\w+)\s*,\s*"(?P<where>[^"]*)"',
                    re.DOTALL)
OPCODES = {'CallRel32': 0xE8, 'JumpRel32': 0xE9}
ROW = re.compile(r'^\{(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)\}')


def load_table(build, game=GAME):
    """Referenz-RVA -> RVA im Build; None fuer den Referenz-Build (Adressen unveraendert)."""
    if build == REFERENCE_BUILD:
        return None
    table = {}
    for line in (game / f'RvaMap{build}.inc').read_text(encoding='utf-8').splitlines():
        match = ROW.match(line.strip())
        if match:
            table[int(match.group(1), 16)] = int(match.group(2), 16)
    return table


def constants(root=REPO / 'src'):
    """Benannte Konstanten aller Quellen. Gleiche Namen in mehreren Dateien: der erste Fund gilt."""
    values = {}
    for path in sorted(root.rglob('*')):
        if path.suffix in ('.cpp', '.h'):
            for match in NAMED.finditer(path.read_text(encoding='utf-8')):
                values.setdefault(match.group(1), int(match.group(2), 16))
    return values


def value(token, names):
    token = token.split('::')[-1]
    if re.fullmatch(r'0x[0-9A-Fa-f]+|\d+', token):
        return int(token, 0)
    return names.get(token)


def checks(text):
    """(Zeile, Ort, Ausdruck, Treffer von SITE oder None, erwartete Bytes) je Code::Expect im Text."""
    for match in EXPECT.finditer(text):
        line = text.count('\n', 0, match.start()) + 1
        expected = bytes(int(item, 16) for item in re.findall(r'0x[0-9A-Fa-f]+', match.group('bytes')))
        site = re.sub(r'\s+', ' ', match.group('site')).replace('Offsets::', '')
        yield line, match.group('where'), site, SITE.match(site), expected


def branches(text):
    """(Zeile, Ort, Ausdruck, Treffer von SITE oder None, Opcode, Ziel-Name) je ExpectBranch im Text."""
    for match in BRANCH.finditer(text):
        line = text.count('\n', 0, match.start()) + 1
        site = re.sub(r'\s+', ' ', match.group('site'))
        yield line, match.group('where'), site, SITE.match(site), OPCODES[match.group('op')], match.group('target')


def resolve(parsed, names, table):
    """RVA im Build oder (None, Grund)."""
    base = value(parsed.group('base'), names)
    if base is None:
        return None, 'unbekannt'
    mapped = base if table is None else table.get(base)
    if mapped is None:
        return None, 'fehlt'
    for key in ('offset', 'delta'):
        if parsed.group(key):
            amount = value(parsed.group(key), names)
            if amount is None:
                return None, 'unbekannt'
            mapped += -amount if key == 'delta' and parsed.group('sign') == '-' else amount
    return mapped, None


BUILD_MARK = re.compile(r'//\s*Build\s+(\d+)\s*$')


def other_build(text, line, build):
    """Wahr, wenn die Startzeile der Pruefung fuer einen anderen Build markiert ist."""
    lines = text.splitlines()
    mark = BUILD_MARK.search(lines[line - 1]) if build is not None and 0 < line <= len(lines) else None
    return bool(mark) and int(mark.group(1)) != build


def evaluate(image, text, names, table, build=None):
    for line, where, site, parsed, expected in checks(text):
        if other_build(text, line, build):
            yield line, where, 'anderer-build', site
            continue
        if parsed is None:
            yield line, where, 'uebersprungen', site
            continue
        rva, problem = resolve(parsed, names, table)
        if problem:
            yield line, where, problem, site
            continue
        actual = image.bytes_at(rva, len(expected))
        if actual == expected:
            yield line, where, 'ok', f'0x{rva:08X}'
        else:
            yield line, where, 'abweichend', f'0x{rva:08X} erwartet {expected.hex(" ")} gelesen {actual.hex(" ")}'
    for line, where, site, parsed, opcode, target in branches(text):
        if other_build(text, line, build):
            yield line, where, 'anderer-build', site
            continue
        rva, problem = resolve(parsed, names, table) if parsed else (None, 'uebersprungen')
        goal = value(target, names)
        goal = None if goal is None else goal if table is None else table.get(goal)
        if problem or goal is None:
            yield line, where, problem or 'fehlt', f'{site} -> {target}'
            continue
        raw = image.bytes_at(rva, 5)
        actual = rva + 5 + int.from_bytes(raw[1:5], 'little', signed=True) if len(raw) == 5 and raw[0] == opcode else None
        if actual == goal:
            yield line, where, 'ok', f'0x{rva:08X} -> 0x{goal:08X}'
        else:
            yield line, where, 'abweichend', f'0x{rva:08X} erwartet {opcode:02X} -> 0x{goal:08X} gelesen {raw.hex(" ")}'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--exe', required=True)
    parser.add_argument('--build', required=True, type=int)
    parser.add_argument('--file', nargs='*', help='nur diese Dateien aus src/Game (Standard: alle *Patches.cpp)')
    args = parser.parse_args()
    image = Image(Path(args.exe).read_bytes())
    table = load_table(args.build)
    names = constants()
    files = [GAME / name for name in args.file] if args.file else sorted(GAME.glob('*Patches.cpp'))
    counts = {}
    for path in files:
        for line, where, status, detail in evaluate(image, path.read_text(encoding='utf-8'), names, table, args.build):
            counts[status] = counts.get(status, 0) + 1
            print(f'{status:13} {path.name}:{line} {where}: {detail}')
    print('Summe: ' + ', '.join(f'{key} {count}' for key, count in sorted(counts.items())))
    return 1 if any(counts.get(key) for key in ('abweichend', 'fehlt', 'unbekannt')) else 0


if __name__ == '__main__':
    sys.exit(main())
