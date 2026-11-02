"""Liest Speicher einer laufenden Spielinstanz. Offsets gelten nur fuer Build 26187885, siehe docs/OFFSETS.md."""
import ctypes
import struct
from ctypes import wintypes as W

GOBJECTS, GNAMES, GWORLD = 0x0AA61B00, 0x0A993AC0, 0x0AAE46F0

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi')
kernel.OpenProcess.restype = W.HANDLE
kernel.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
psapi.EnumProcessModules.argtypes = [W.HANDLE, ctypes.c_void_p, W.DWORD, ctypes.POINTER(W.DWORD)]


class Game:
    def __init__(self, pid):
        self.pid = pid
        self.handle = kernel.OpenProcess(0x0410, False, pid)
        if not self.handle:
            raise SystemExit(f'{pid}: kein Zugriff auf den Prozess')
        modules = (ctypes.c_void_p * 1)()
        needed = W.DWORD()
        psapi.EnumProcessModules(self.handle, modules, ctypes.sizeof(modules), ctypes.byref(needed))
        self.base = modules[0]
        self.names = {}

    def read(self, address, size):
        buffer = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not address or not kernel.ReadProcessMemory(self.handle, address, buffer, size, ctypes.byref(got)):
            return None
        return buffer.raw[:got.value]

    def ptr(self, address):
        data = self.read(address, 8)
        return struct.unpack('<Q', data)[0] if data and len(data) == 8 else 0

    def i32(self, address):
        data = self.read(address, 4)
        return struct.unpack('<i', data)[0] if data and len(data) == 4 else 0

    def name_from_index(self, index):
        if index in self.names:
            return self.names[index]
        entry = self.ptr(self.base + GNAMES + 0x10 + 8 * (index >> 16)) + (index & 0xFFFF) * 2
        header = self.read(entry, 2)
        if not header:
            return '?'
        value = struct.unpack('<H', header)[0]
        wide = value & 1
        text = (self.read(entry + 2, (value >> 6) * (2 if wide else 1)) or b'').decode('utf-16le' if wide else 'latin-1', 'replace')
        self.names[index] = text
        return text

    def name(self, obj):
        return self.name_from_index(self.i32(obj + 0x18)) if obj else None

    def world(self):
        return self.ptr(self.base + GWORLD)

    def objects(self):
        chunks = self.ptr(self.base + GOBJECTS)
        count = self.i32(self.base + GOBJECTS + 8)
        for chunk_index in range((count >> 16) + 1):
            chunk = self.ptr(chunks + 8 * chunk_index)
            raw = self.read(chunk, 0x10000 * 0x18) if chunk else None
            if not raw:
                continue
            for i in range(min(0x10000, count - (chunk_index << 16), len(raw) // 0x18)):
                obj = struct.unpack_from('<Q', raw, i * 0x18 + 8)[0]
                if obj:
                    yield obj
