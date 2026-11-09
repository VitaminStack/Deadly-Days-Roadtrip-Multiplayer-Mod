"""Begrenzter In-Memory-Kismet-Reader fuer Build 26495302, kein Codeexport.

Validiert Ausdrucksgrenzen und statische Sprungziele. Unbekannte/mehrdeutige
Formate werden abgelehnt. Die Ausgabe enthaelt nur Pruefzaehler und symbolische
Aufrufmengen, keine Funktionskoerper oder Rohzeiger.
Token-Bezeichnungen: oeffentliche UAssetAPI-EExprToken-Dokumentation.
"""
from collections import Counter


class InvalidScript(ValueError):
    pass


class Reader:
    def __init__(self, data, functions, name_size):
        self.data, self.functions, self.name_size = data, functions, name_size
        self.pos, self.depth = 0, 0
        self.tokens, self.calls = Counter(), set()
        self.roots, self.targets = set(), []
        self.dispatches = []

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise InvalidScript("truncated operand")
        value = self.data[self.pos:self.pos + size]
        self.pos += size
        return value

    def integer(self, size=4):
        return int.from_bytes(self.take(size), "little")

    def target(self):
        self.targets.append(self.integer())

    def sequence(self, end):
        while True:
            if self.pos >= len(self.data):
                raise InvalidScript("missing terminator")
            if self.data[self.pos] == end:
                self.take(1)
                return
            self.expression()

    def expression(self):
        self.depth += 1
        if self.depth > 128:
            raise InvalidScript("excessive nesting")
        try:
            op = self.integer(1)
            self.tokens[op] += 1
            if op in (0x00, 0x01, 0x02, 0x20, 0x33, 0x48, 0x6C):
                self.take(8)
            elif op in (0x0B, 0x17, 0x25, 0x26, 0x27, 0x28, 0x2A, 0x2D,
                        0x4D, 0x50, 0x5A, 0x5E):
                pass
            elif op in (0x04, 0x4E, 0x4F, 0x51, 0x5D, 0x67):
                self.expression()
            elif op in (0x06, 0x4C):
                self.target()
            elif op == 0x07:
                self.target()
                self.expression()
            elif op in (0x0C, 0x1D, 0x1E, 0x5B):
                self.take(4)
            elif op in (0x24, 0x2C):
                self.take(1)
            elif op in (0x35, 0x36, 0x37):
                self.take(8)
            elif op in (0x21, 0x4B):
                self.take(self.name_size)
            elif op in (0x1F, 0x34):
                width = 2 if op == 0x34 else 1
                while any(self.take(width)):
                    pass
            elif op in (0x22, 0x23, 0x41, 0x2B):
                self.take({0x22: 24, 0x23: 24, 0x41: 12, 0x2B: 80}[op])
            elif op in (0x13, 0x2E, 0x42, 0x52, 0x54, 0x55, 0x64):
                self.take(8)
                self.expression()
            elif op == 0x38:
                self.take(1)
                self.expression()
            elif op == 0x0F:
                self.take(8)
                self.expression()
                self.expression()
            elif op in (0x14, 0x43, 0x44, 0x5C, 0x5F, 0x60, 0x62, 0x6B):
                self.expression()
                self.expression()
            elif op in (0x12, 0x19, 0x1A):
                self.expression()
                self.take(4)  # Relative Skip-Laenge, kein absoluter CFG-Sprung.
                self.take(8)
                self.expression()
            elif op == 0x18:
                self.take(4)
                self.expression()
            elif op in (0x1B, 0x45):
                self.take(self.name_size)
                self.sequence(0x16)
            elif op in (0x1C, 0x46, 0x68, 0x63):
                pointer = self.integer(8)
                if pointer not in self.functions:
                    raise InvalidScript("unresolved final function")
                self.calls.add(self.functions[pointer])
                if ".ExecuteUbergraph_" in self.functions[pointer]:
                    entry = None
                    if self.pos < len(self.data):
                        tag = self.data[self.pos]
                        if tag == 0x1D and self.pos + 5 <= len(self.data):
                            entry = int.from_bytes(self.data[self.pos + 1:self.pos + 5], "little", signed=True)
                        elif tag in (0x25, 0x26):
                            entry = tag - 0x25
                    self.dispatches.append((self.functions[pointer], entry))
                if op == 0x63:
                    self.expression()
                self.sequence(0x16)
            elif op == 0x2F:
                self.take(8)
                self.take(4)
                self.sequence(0x30)
            elif op == 0x31:
                self.expression()
                self.sequence(0x32)
            elif op in (0x65, 0x3D, 0x3F):
                self.take(16 if op == 0x3F else 8)
                self.take(4)
                self.sequence({0x65: 0x66, 0x3D: 0x3E, 0x3F: 0x40}[op])
            elif op in (0x39, 0x3B):
                self.expression()
                self.take(4)
                self.sequence(0x3A if op == 0x39 else 0x3C)
            elif op == 0x61:
                self.take(self.name_size)
                self.expression()
                self.expression()
            elif op == 0x69:
                cases = self.integer(2)
                if cases > len(self.data):
                    raise InvalidScript("invalid switch size")
                self.target()
                self.expression()
                for _ in range(cases):
                    self.expression()
                    self.target()
                    self.expression()
                self.expression()
            else:
                raise InvalidScript(f"unsupported token 0x{op:02x}")
        finally:
            self.depth -= 1

    def run(self):
        while self.pos < len(self.data) and self.data[self.pos] != 0x53:
            self.roots.add(self.pos)
            self.expression()
        self.roots.add(self.pos)
        if self.take(1) != b"\x53" or self.pos != len(self.data):
            raise InvalidScript("missing/followed end-of-script")
        if any(target not in self.roots for target in self.targets):
            raise InvalidScript("jump is not a statement boundary")
        return {"complete": True, "statements": len(self.roots) - 1,
                "static_targets": len(self.targets), "static_targets_valid": True,
                "conditional_jumps": self.tokens[0x07] + self.tokens[0x4F],
                "computed_jumps": self.tokens[0x4E],
                "verified_calls": sorted(self.calls),
                "_layout": (tuple(sorted(self.roots)), tuple(sorted(self.tokens.items()))),
                "_dispatches": tuple(self.dispatches)}


def audit(script, functions, scripts=None):
    if not script:
        return {"complete": True, "empty": True, "verified_calls": []}
    attempts, failures = [], []
    for size in (8, 12):
        try:
            attempts.append(Reader(script, functions, size).run())
        except InvalidScript as error:
            failures.append(str(error))
    if not attempts:
        return {"complete": False, "reason": "; ".join(sorted(set(failures)))}
    if any(attempt != attempts[0] for attempt in attempts[1:]):
        return {"complete": False, "reason": "ambiguous name layout"}
    result = attempts[0]
    result.pop("_layout")
    dispatches = result.pop("_dispatches")
    if scripts is not None and dispatches:
        valid = True
        for callee, entry in dispatches:
            if callee not in scripts or entry is None:
                valid = False
                continue
            layouts = []
            for size in (8, 12):
                try:
                    layouts.append(Reader(scripts[callee], functions, size).run()["_layout"][0])
                except InvalidScript:
                    pass
            if not layouts or any(entry not in roots for roots in layouts):
                valid = False
        result["dispatch_entries_checked"] = len(dispatches)
        result["dispatch_entries_valid"] = valid
    return result
