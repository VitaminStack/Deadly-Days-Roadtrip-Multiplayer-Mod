"""Read-only PE32+-Pruefung; nur Standardbibliothek, kein Spielstart oder Dump."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


# Kurze Identifikatoren, keine Funktionskopien. ?? maskiert RIP-relative Daten.
PATTERNS = {
    "ProcessEvent": (0x160C040,
        "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 00 01 00 00 "
        "48 8D 6C 24 30 48 89 9D 28 01 00 00 48 8B 05 ?? ?? ?? ??"),
    "SetClientTravel": (0x3262830,
        "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 81 B0 12 00 00 "
        "41 8B F9 48 63 89 B8 12 00 00 49 8B F0"),
    "SeamlessTravelToLevel": (0x5976AD0,
        "48 89 5C 24 18 48 89 54 24 10 55 56 57 48 8D 6C 24 B9 "
        "48 81 EC 90 00 00 00 48 8B FA 48 8B F1 80 79 30 00"),
}


def matches(data, pattern):
    tokens = pattern.split()
    values = [None if token == "??" else int(token, 16) for token in tokens]
    if not values or values[0] is None:
        raise ValueError("Muster braucht ein festes erstes Byte")
    hits, start = [], 0
    while (offset := data.find(bytes([values[0]]), start)) != -1:
        if offset + len(values) <= len(data) and all(
                value is None or data[offset + i] == value
                for i, value in enumerate(values)):
            hits.append(offset)
        start = offset + 1
    return hits


class PE:
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("DOS-Signatur fehlt")
        pe = self.unpack("I", 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0" or self.unpack("H", pe + 4)[0] != 0x8664:
            raise ValueError("Kein AMD64-PE")
        count = self.unpack("H", pe + 6)[0]
        optional_size = self.unpack("H", pe + 20)[0]
        optional = pe + 24
        if self.unpack("H", optional)[0] != 0x20B:
            raise ValueError("Kein PE32+")
        self.import_rva = self.unpack("I", optional + 120)[0]
        self.sections = []
        for i in range(count):
            section = optional + optional_size + i * 40
            size, rva, raw_size, raw = self.unpack("IIII", section + 8)
            flags = self.unpack("I", section + 36)[0]
            self.sections.append((rva, raw, raw_size, flags))

    def unpack(self, fmt, offset):
        return struct.unpack_from("<" + fmt, self.data, offset)

    def offset(self, rva):
        for va, raw, size, _ in self.sections:
            if va <= rva < va + size:
                return raw + rva - va
        raise ValueError(f"RVA nicht dateigestuetzt: {rva:#x}")

    def string(self, rva):
        offset = self.offset(rva)
        return self.data[offset:self.data.index(b"\0", offset)].decode("ascii")

    def imports(self):
        result = {}
        if not self.import_rva:
            return result
        offset = self.offset(self.import_rva)
        while True:
            lookup, stamp, chain, name, address = self.unpack("IIIII", offset)
            if not any((lookup, stamp, chain, name, address)):
                break
            symbols = []
            thunk = self.offset(lookup or address)
            while (entry := self.unpack("Q", thunk)[0]):
                symbols.append(f"ordinal:{entry & 0xFFFF}" if entry >> 63 else self.string(entry + 2))
                thunk += 8
            result[self.string(name).lower()] = symbols
            offset += 20
        return result

    def scan(self, pattern):
        hits = []
        for rva, raw, size, flags in self.sections:
            if flags & 0x20000000:  # IMAGE_SCN_MEM_EXECUTE
                hits.extend(rva + i for i in matches(self.data[raw:raw + size], pattern))
        return hits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    args = parser.parse_args()
    data = args.exe.read_bytes()
    pe = PE(data)
    results = {}
    for name, (expected, pattern) in PATTERNS.items():
        hits = pe.scan(pattern)
        results[name] = {"expected_rva": hex(expected), "hits": [hex(hit) for hit in hits],
                         "unique_expected": hits == [expected]}
    imports = pe.imports()
    print(json.dumps({"sha256": hashlib.sha256(data).hexdigest(),
                      "regular_proxy_imports": {name: imports.get(name, [])
                                                for name in ("version.dll", "dwmapi.dll", "dxgi.dll")},
                      "signatures": results}, indent=2))
    return 0 if all(result["unique_expected"] for result in results.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
