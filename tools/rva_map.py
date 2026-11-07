"""Code- und Datenadressen der Mod vom Referenz-Build auf einen neuen Build umrechnen.

Liest die RVAs aus src/ (benannte Konstanten und At(0x...)), sucht sie in der neuen Exe und erzeugt die Tabelle
src/Game/RvaMap<build>.inc fuer Game::AddressMap. Liest nur lokale Exe-Dateien, schreibt nie Spielbytes ins Repo.

  py tools/rva_map.py --old <exe des Referenz-Builds> --new <neue exe> --build 26495302 [--write]

Verfahren je RVA:
  Code (.text): Befehle ab der Adresse mit Capstone lesen, relative Sprungziele und RIP-relative Verschiebungen
    maskieren und das kuerzeste eindeutige Muster in der neuen .text suchen. Ohne Treffer ab der Adresse wird
    der Funktionsanfang aus .pdata gesucht und die Stelle im selben Abstand geprueft.
  vtable (.rdata): die ersten Eintraege als Funktionen umrechnen und die neue Zeigerfolge in .rdata suchen.
  Global (.data): einen Code-Verweis im alten Exe suchen, diesen Befehl im neuen Exe finden und dessen Ziel lesen.
Status: eindeutig, ueber-funktion, vtable, global, fehlt. Alles ausser eindeutig vor der Uebernahme ansehen;
die Byte-Pruefungen der Patches (Code::Expect) bleiben die letzte Sicherung zur Laufzeit.
"""
import argparse
import bisect
import re
import struct
import sys
from pathlib import Path

try:
    import capstone
except ImportError:  # pragma: no cover - nur Hinweis fuer die Einrichtung
    capstone = None

REPO = Path(__file__).resolve().parents[1]
IMAGE_BASE = 0x140000000
MIN_RVA = 0x100000
NAMED = re.compile(r'constexpr\s+uintptr_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+)')
LITERAL = re.compile(r'\bAt\((0x[0-9A-Fa-f]+)\)')


class Image:
    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        count = struct.unpack_from('<H', data, pe + 6)[0]
        optional = pe + 24
        self.size = struct.unpack_from('<I', data, optional + 56)[0]
        table = optional + struct.unpack_from('<H', data, pe + 20)[0]
        self.sections = {}
        for index in range(count):
            name, vsize, va, rsize, raw = struct.unpack_from('<8sIIII', data, table + 40 * index)
            self.sections[name.rstrip(b'\0').decode()] = (va, vsize, raw, rsize)
        self._functions = None

    def section_of(self, rva):
        for name, (va, vsize, _, _) in self.sections.items():
            if va <= rva < va + vsize:
                return name
        return None

    def bytes_at(self, rva, size):
        for va, vsize, raw, rsize in self.sections.values():
            if va <= rva < va + min(vsize, rsize):
                start = raw + rva - va
                return self.data[start:start + size]
        return b''

    def section_bytes(self, name):
        va, vsize, raw, rsize = self.sections[name]
        return va, self.data[raw:raw + min(vsize, rsize)]

    def functions(self):
        """Sortierte Funktionsanfaenge aus .pdata (RUNTIME_FUNCTION: Begin, End, Unwind)."""
        if self._functions is None:
            _, table = self.section_bytes('.pdata')
            starts = {begin for begin, _, _ in struct.iter_unpack('<III', table[:len(table) // 12 * 12]) if begin}
            self._functions = sorted(starts)
        return self._functions

    def function_start(self, rva):
        starts = self.functions()
        index = bisect.bisect_right(starts, rva) - 1
        return starts[index] if index >= 0 else None


def disassembler():
    if capstone is None:
        sys.exit('capstone fehlt: py -m pip install --user capstone')
    engine = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    engine.detail = True
    return engine


def masked(engine, image, rva, count):
    """Muster aus count Befehlen ab rva, None steht fuer ein beliebiges Byte."""
    code = image.bytes_at(rva, 16 * count)
    pattern = []
    for number, insn in enumerate(engine.disasm(code, rva)):
        if number == count:
            break
        raw = list(insn.bytes)
        holes = set()
        if insn.disp_size and _rip_relative(insn):
            holes.update(range(insn.disp_offset, insn.disp_offset + insn.disp_size))
        if insn.imm_size and insn.group(capstone.CS_GRP_BRANCH_RELATIVE):
            holes.update(range(insn.imm_offset, insn.imm_offset + insn.imm_size))
        pattern.extend(None if index in holes else value for index, value in enumerate(raw))
    return pattern


def _rip_relative(insn):
    from capstone import x86
    return any(op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP for op in insn.operands)


def to_regex(pattern):
    return re.compile(b''.join(b'.' if value is None else re.escape(bytes([value])) for value in pattern), re.DOTALL)


def find_unique(text_va, text, pattern):
    hits = []
    for match in to_regex(pattern).finditer(text):
        hits.append(text_va + match.start())
        if len(hits) > 1:
            break
    return hits


def map_code(engine, old, new, new_text, rva):
    text_va, text = new_text
    for count in (4, 6, 8, 12, 16, 24, 32):
        pattern = masked(engine, old, rva, count)
        if sum(value is not None for value in pattern) < 8:
            continue
        hits = find_unique(text_va, text, pattern)
        if len(hits) == 1:
            return hits[0], 'eindeutig'
        if not hits:
            break
    start = old.function_start(rva)
    if start is not None and start != rva:
        mapped, _ = map_code(engine, old, new, new_text, start) if start != rva else (None, None)
        if mapped is not None:
            site = mapped + rva - start
            if matches(new.bytes_at(site, 64), masked(engine, old, rva, 3)):
                return site, 'ueber-funktion'
    return None, 'fehlt'


def matches(data, pattern):
    return len(data) >= len(pattern) and all(value is None or data[i] == value for i, value in enumerate(pattern))


def map_vtable(engine, old, new, new_text, rva, entries=6):
    pointers = struct.unpack_from(f'<{entries}Q', old.bytes_at(rva, 8 * entries))
    targets = []
    for pointer in pointers:
        mapped, _ = map_code(engine, old, new, new_text, pointer - IMAGE_BASE)
        if mapped is None:
            return None, 'fehlt'
        targets.append(mapped + IMAGE_BASE)
    va, rdata = new.section_bytes('.rdata')
    needle = struct.pack(f'<{entries}Q', *targets)
    first = rdata.find(needle)
    if first < 0 or rdata.find(needle, first + 1) >= 0:
        return None, 'fehlt'
    return va + first, 'vtable'


# mov/lea/cmp mit RIP-relativem Speicheroperand, REX.W 48/4C
RIP_REFERENCE = re.compile(rb'[\x48\x4C][\x8B\x8D\x89\x39\x3B\x83\xC7](?=[\x05\x0D\x15\x1D\x25\x2D\x35\x3D])', re.DOTALL)


def map_global(engine, old, new, new_text, rva, tries=8):
    text_va, text = old.section_bytes('.text')
    seen = 0
    for match in RIP_REFERENCE.finditer(text):
        site = text_va + match.start()
        insn = next(engine.disasm(text[match.start():match.start() + 15], site), None)
        if insn is None or not insn.disp_size or not _rip_relative(insn):
            continue
        if insn.address + insn.size + insn.disp != rva:
            continue
        seen += 1
        mapped, _ = map_code(engine, old, new, new_text, site)
        if mapped is not None:
            moved = next(engine.disasm(new.bytes_at(mapped, 15), mapped), None)
            if moved is not None and moved.disp_size:
                return moved.address + moved.size + moved.disp, 'global'
        if seen >= tries:
            break
    return None, 'fehlt'


def source_rvas(size):
    found = {}
    for path in sorted((REPO / 'src').rglob('*')):
        if path.suffix not in ('.cpp', '.h'):
            continue
        for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            where = f'{path.relative_to(REPO).as_posix()}:{number}'
            for match in NAMED.finditer(line):
                value = int(match.group(2), 16)
                if MIN_RVA <= value < size:
                    found.setdefault(value, (where, match.group(1)))
            for match in LITERAL.finditer(line):
                value = int(match.group(1), 16)
                if MIN_RVA <= value < size:
                    found.setdefault(value, (where, 'At'))
    return found


def build_map(old, new, rvas):
    engine = disassembler()
    new_text = new.section_bytes('.text')
    rows = []
    for rva, (where, name) in sorted(rvas.items()):
        section = old.section_of(rva)
        if section == '.text':
            mapped, status = map_code(engine, old, new, new_text, rva)
        elif section == '.rdata':
            mapped, status = map_vtable(engine, old, new, new_text, rva)
        elif section == '.data':
            mapped, status = map_global(engine, old, new, new_text, rva)
        else:
            mapped, status = None, 'fehlt'
        rows.append((rva, mapped, status, where, name))
    return rows


def render(rows, build):
    lines = [f'// Erzeugt von tools/rva_map.py fuer Build {build}. Referenz-RVA -> RVA in diesem Build, aufsteigend sortiert.',
             '// Nur Zeilen mit geprueftem Ergebnis eintragen; fehlende Adressen bleiben Kommentar und liefern zur Laufzeit 0.',
             '// Haendisch nachgetragene Zeilen mit "// von Hand" kennzeichnen, damit ein erneuter Lauf sie nicht still ersetzt.']
    for rva, mapped, status, where, name in rows:
        comment = f'{status}, {name}, {where}'
        lines.append(f'{{0x{rva:08X}, 0x{mapped:08X}}}, // {comment}' if mapped is not None else f'// FEHLT 0x{rva:08X} // {comment}')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--old', required=True)
    parser.add_argument('--new', required=True)
    parser.add_argument('--build', required=True)
    parser.add_argument('--write', action='store_true', help='src/Game/RvaMap<build>.inc schreiben')
    args = parser.parse_args()
    old = Image(Path(args.old).read_bytes())
    new = Image(Path(args.new).read_bytes())
    rows = build_map(old, new, source_rvas(old.size))
    counts = {}
    for rva, mapped, status, where, name in rows:
        counts[status] = counts.get(status, 0) + 1
        shown = f'0x{mapped:08X}' if mapped is not None else '-'
        print(f'{status:15} 0x{rva:08X} -> {shown:10} {name} ({where})')
    print('Summe: ' + ', '.join(f'{key} {value}' for key, value in sorted(counts.items())))
    if args.write:
        target = REPO / 'src' / 'Game' / f'RvaMap{args.build}.inc'
        target.write_text(render(rows, args.build), encoding='utf-8', newline='\n')
        print('Geschrieben: ' + target.relative_to(REPO).as_posix())
    return 0


if __name__ == '__main__':
    sys.exit(main())
