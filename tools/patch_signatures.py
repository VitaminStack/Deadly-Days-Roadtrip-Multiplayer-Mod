"""Byte-Signaturen der Patch-Adressen erzeugen und pruefen (patch-signaturen).

Die Tabelle src/Game/PatchSignatures.inc ordnet Code-Adressen des Referenz-Builds ein Byte-Muster zu. Zur Laufzeit prueft
die Mod jedes Muster an der erwarteten Stelle (Game::Signatures); passt es nicht, bleibt nur der Patch aus, der diese
Adresse braucht. Dass ein Muster in der Exe genau einmal vorkommt, prueft dieses Werkzeug offline. Liest nur lokale
Exe-Dateien; die Muster sind maskierte Befehlsbytes ohne Sprungziele und RIP-relative Verschiebungen.

  py tools/patch_signatures.py generate --exe <ref.exe> [--other <exe> <build> ...] [--file AirdropPatches.cpp ...] [--write]
  py tools/patch_signatures.py check --exe <exe> --build <build> [--strict]

generate: Fuer jede Code-Adresse (.text) der genannten Dateien das kuerzeste Muster, das in jeder angegebenen Exe genau
  einmal und an der erwarteten Stelle vorkommt (andere Builds ueber RvaMap<build>.inc), Build-Spalte 0. Zuerst ab der
  Adresse, sonst ab einem frueheren Befehl; die Lage der Adresse im Muster steht dann als Abstand in der Zeile. Weichen
  einzelne Bytes zwischen den Builds ab, werden sie Platzhalter. Hat sich der Code zu stark geaendert, bekommt jeder Build
  eine eigene Zeile mit seiner Build-Nummer. --write fuegt die Zeilen ein, Zeilen anderer Adressen bleiben erhalten.
check: Jede fuer den Build geltende Zeile muss in der Exe genau einmal und an der erwarteten Stelle passen. Im Referenz-
  Build listet es ausserdem Code-Adressen der Patch-Dateien ohne Zeile ("ohne-signatur"); mit --strict ist das ein Fehler.
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rva_map import LITERAL, MIN_RVA, NAMED, Image, disassembler, masked, to_regex  # noqa: E402
from expect_check import REFERENCE_BUILD, load_table  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
GAME = REPO / 'src' / 'Game'
TABLE = GAME / 'PatchSignatures.inc'
ROW = re.compile(r'^\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(\d+)\s*,\s*"([0-9A-F? ]+)"\s*,\s*(\d+)\s*,\s*"([^"]*)"\s*\},?\s*(?://\s*(.*))?$')
ALL_BUILDS = 0
MIN_FIXED = 8        # feste Bytes im ganzen Muster
MIN_AT_SITE = 4      # feste Bytes ab der Adresse, damit das Muster die Stelle selbst belegt
MIN_SHARE = 0.5      # Anteil fester Bytes; darunter ist ein zusammengelegtes Muster zu schwach
MAX_DIFFERENT = 4    # zwischen den Builds abweichende Bytes, die ein gemeinsames Muster noch als Platzhalter aufnimmt
COUNTS = (3, 4, 6, 8, 12, 16, 24, 32, 48)
BACK = 24
HEADER = [
    '// Erzeugt von tools/patch_signatures.py: Referenz-RVA, Build (0 = alle), Byte-Muster (?? beliebig), Lage der Adresse',
    '// im Muster, Name. Aufsteigend nach RVA, je RVA erst Build 0, dann die eigenen Zeilen einzelner Builds.',
    '// Die Mod prueft das Muster an der erwarteten Stelle, das Werkzeug zusaetzlich die Eindeutigkeit in der Exe.',
]


def patch_files(game=GAME):
    return sorted(game.glob('*Patches.cpp')) + [game / 'CodeCave.cpp']


def code_sites(paths, image):
    """RVA -> (Name, Datei) fuer alle Adressen der Dateien, die im Referenz-Exe in .text liegen."""
    found = {}
    for path in paths:
        text = path.read_text(encoding='utf-8')
        for match in NAMED.finditer(text):
            found.setdefault(int(match.group(2), 16), (match.group(1), path.name))
        for match in LITERAL.finditer(text):
            found.setdefault(int(match.group(1), 16), (f'At_{match.group(1)[2:].upper()}', path.name))
    return {rva: site for rva, site in found.items() if rva >= MIN_RVA and image.section_of(rva) == '.text'}


def render_pattern(pattern):
    return ' '.join('??' if value is None else f'{value:02X}' for value in pattern)


def parse_pattern(text):
    return [None if token in ('?', '??') else int(token, 16) for token in text.split()]


def load_rows(path=TABLE):
    """(RVA, Build) -> (Muster, Lage, Name, Kommentar)."""
    rows = {}
    if path.exists():
        for line in path.read_text(encoding='utf-8').splitlines():
            match = ROW.match(line.strip())
            if match:
                key = (int(match.group(1), 16), int(match.group(2)))
                rows[key] = (parse_pattern(match.group(3)), int(match.group(4)), match.group(5), match.group(6) or '')
    return rows


def render_rows(rows):
    lines = list(HEADER)
    for rva, build in sorted(rows):
        pattern, offset, name, comment = rows[(rva, build)]
        line = f'{{0x{rva:08X}, {build}, "{render_pattern(pattern)}", {offset}, "{name}"}},'
        lines.append(line + (f' // {comment}' if comment else ''))
    return '\n'.join(lines) + '\n'


def row_for(rows, rva, build):
    """Zeile eines Builds: seine eigene, sonst die fuer alle Builds."""
    return rows.get((rva, build)) or rows.get((rva, ALL_BUILDS))


def hits(image, pattern, limit=2):
    """RVAs der Treffer in .text, hoechstens limit."""
    text_va, text = image.section_bytes('.text')
    found = []
    for match in to_regex(pattern).finditer(text):
        found.append(text_va + match.start())
        if len(found) >= limit:
            break
    return found


def merge(patterns):
    """Muster zusammenlegen, abweichende Bytes werden Platzhalter, die Laenge ist die kuerzeste."""
    size = min(len(p) for p in patterns)
    return [p0 if all(p[i] == p0 for p in patterns) else None for i, p0 in enumerate(patterns[0][:size])]


def strong(pattern, offset):
    fixed = sum(value is not None for value in pattern)
    return (fixed >= MIN_FIXED and fixed >= MIN_SHARE * len(pattern)
            and sum(value is not None for value in pattern[offset:]) >= MIN_AT_SITE)


def starts_before(engine, image, rva):
    """Befehlsanfaenge vor rva, naechster zuerst: aus der umgebenden Funktion, am Funktionsanfang aus der vorigen."""
    start = image.function_start(rva - 1 if image.function_start(rva) == rva else rva)
    if start is None or start >= rva:
        return []
    found = [insn.address for insn in engine.disasm(image.bytes_at(start, rva - start), start)]
    return [address for address in reversed(found) if address < rva][:BACK]


def window(engine, images, offset):
    """Muster, das in allen Builds an Adresse - offset beginnt, oder (None, Grund)."""
    reason = 'nicht eindeutig'
    starts = [site - offset for _, site in images]
    for count in COUNTS:
        own = [masked(engine, image, start, count) for (image, _), start in zip(images, starts)]
        pattern = merge(own)
        # Mehr als ein paar abweichende Bytes heisst geaenderter Code: dann lieber je Build eine eigene Zeile.
        if sum(value is None for value in pattern) - sum(value is None for value in own[0][:len(pattern)]) > MAX_DIFFERENT:
            return None, 'Code zwischen den Builds geaendert'
        if not strong(pattern, offset):
            continue
        found = [hits(image, pattern) for image, _ in images]
        if any(not f for f in found):
            return None, 'kein Treffer'
        if all(f == [start] for f, start in zip(found, starts)):
            return pattern, None
        if any(start not in f for f, start in zip(found, starts)):
            reason = 'Treffer an anderer Stelle'
    return None, reason


def signature(engine, images):
    """images: [(Image, erwartete RVA)]. (Muster, Lage, None) oder (None, 0, Grund).

    Zuerst ab der Adresse, sonst ab einem frueheren Befehl derselben oder der vorigen Funktion (im ersten Image)."""
    pattern, reason = window(engine, images, 0)
    if pattern is not None:
        return pattern, 0, None
    first, site = images[0]
    for start in starts_before(engine, first, site):
        found, _ = window(engine, images, site - start)
        if found is not None:
            return found, site - start, None
    return None, 0, reason


def signatures_for(engine, images, builds):
    """{Build: (Muster, Lage)} oder (None, Grund): erst gemeinsam fuer alle Builds, sonst je Build einzeln."""
    pattern, offset, reason = signature(engine, images)
    if pattern is not None:
        return {ALL_BUILDS: (pattern, offset)}, None
    if len(images) == 1:
        return None, reason
    result = {}
    for image, build in zip(images, builds):
        own, own_offset, own_reason = signature(engine, [image])
        if own is None:
            return None, f'{reason}; Build {build}: {own_reason}'
        result[build] = (own, own_offset)
    return result, None


def check_rows(image, rows, table, build):
    """(RVA, Name, Status, Detail) je fuer den Build geltender Zeile. table None heisst Referenz-Build."""
    for rva in sorted({rva for rva, _ in rows}):
        row = row_for(rows, rva, build)
        if row is None:
            continue
        pattern, offset, name, _ = row
        site = rva if table is None else table.get(rva)
        if site is None:
            yield rva, name, 'fehlt', 'keine Adresse in RvaMap'
            continue
        expected = site - offset
        found = hits(image, pattern)
        if found == [expected]:
            yield rva, name, 'ok', f'0x{site:08X}'
        elif not found:
            yield rva, name, 'abweichend', f'kein Treffer, erwartet 0x{expected:08X}'
        elif len(found) > 1:
            yield rva, name, 'mehrdeutig', ', '.join(f'0x{h:08X}' for h in found) + ' ...'
        else:
            yield rva, name, 'abweichend', f'Treffer 0x{found[0]:08X}, erwartet 0x{expected:08X}'


def generate(args):
    reference = Image(Path(args.exe).read_bytes())
    others = []
    for exe, build in zip(args.other[::2], args.other[1::2]):
        others.append((Image(Path(exe).read_bytes()), load_table(int(build)), int(build)))
    paths = [GAME / name for name in args.file] if args.file else patch_files()
    sites = code_sites(paths, reference)
    engine = disassembler()
    rows = load_rows()
    failed = 0
    for rva, (name, file) in sorted(sites.items()):
        missing = [str(build) for _, table, build in others if table.get(rva) is None]
        if missing:
            print(f'fehlt          0x{rva:08X} {name} ({file}): keine Adresse in RvaMap {",".join(missing)}')
            failed += 1
            continue
        images = [(reference, rva)] + [(image, table[rva]) for image, table, _ in others]
        builds = [REFERENCE_BUILD] + [build for _, _, build in others]
        found, reason = signatures_for(engine, images, builds)
        if found is None:
            print(f'ohne-muster    0x{rva:08X} {name} ({file}): {reason}')
            failed += 1
            continue
        for key in [key for key in rows if key[0] == rva]:
            del rows[key]
        for build, (pattern, offset) in found.items():
            rows[(rva, build)] = (pattern, offset, name, file)
            print(f'ok             0x{rva:08X} {name} ({file}) Build {build}: +{offset} {render_pattern(pattern)}')
    if args.write:
        TABLE.write_text(render_rows(rows), encoding='utf-8', newline='\n')
        print('Geschrieben: ' + TABLE.relative_to(REPO).as_posix())
    return 1 if failed else 0


def check(args):
    image = Image(Path(args.exe).read_bytes())
    table = None if args.build == REFERENCE_BUILD else load_table(args.build)
    rows = load_rows()
    counts = {}
    for rva, name, status, detail in check_rows(image, rows, table, args.build):
        counts[status] = counts.get(status, 0) + 1
        print(f'{status:13} 0x{rva:08X} {name}: {detail}')
    if table is None:
        listed = {rva for rva, _ in rows}
        for rva, (name, file) in sorted(code_sites(patch_files(), image).items()):
            if rva not in listed:
                counts['ohne-signatur'] = counts.get('ohne-signatur', 0) + 1
                print(f'ohne-signatur 0x{rva:08X} {name} ({file})')
    print('Summe: ' + ', '.join(f'{key} {count}' for key, count in sorted(counts.items())))
    errors = ('abweichend', 'mehrdeutig', 'fehlt') + (('ohne-signatur',) if args.strict else ())
    return 1 if any(counts.get(key) for key in errors) else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    gen = sub.add_parser('generate')
    gen.add_argument('--exe', required=True, help=f'Exe des Referenz-Builds {REFERENCE_BUILD}')
    gen.add_argument('--other', nargs='*', default=[], help='weitere Builds als Paare <exe> <build>')
    gen.add_argument('--file', nargs='*', help='nur diese Dateien aus src/Game (Standard: alle Patch-Dateien)')
    gen.add_argument('--write', action='store_true')
    chk = sub.add_parser('check')
    chk.add_argument('--exe', required=True)
    chk.add_argument('--build', required=True, type=int)
    chk.add_argument('--strict', action='store_true', help='Code-Adressen ohne Signatur sind ein Fehler')
    args = parser.parse_args()
    if args.command == 'generate' and len(args.other) % 2:
        parser.error('--other erwartet Paare aus Exe und Build')
    return generate(args) if args.command == 'generate' else check(args)


if __name__ == '__main__':
    sys.exit(main())
