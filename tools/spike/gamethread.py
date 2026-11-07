"""Zeigt bei einem Haenger, wo der Game Thread steht: gamethread.py <pid>

Der aktivste Thread wird angehalten, sein Stack nach Adressen in der Exe und nach Blueprint-Funktionen durchsucht.
Die Stack-Suche ist grob und kann alte Eintraege zeigen.
"""
import ctypes
import struct
import sys
import time
from ctypes import wintypes as W

from ddmem import Game, kernel

STACK_BYTES, BLOCK = 0x7000, 0x400
TEXT_SIZE = 0x8276000
THREAD_ACCESS = 0x005A
CONTEXT_FULL = 0x100003
kernel.OpenThread.restype = W.HANDLE
kernel.CreateToolhelp32Snapshot.restype = W.HANDLE
kernel.SuspendThread.argtypes = [W.HANDLE]
kernel.ResumeThread.argtypes = [W.HANDLE]
kernel.GetThreadTimes.argtypes = [W.HANDLE] + [ctypes.POINTER(W.FILETIME)] * 4


class ThreadEntry(ctypes.Structure):
    _fields_ = [('size', W.DWORD), ('usage', W.DWORD), ('thread', W.DWORD), ('process', W.DWORD), ('base', W.LONG), ('delta', W.LONG), ('flags', W.DWORD)]


def thread_ids(pid):
    snapshot = kernel.CreateToolhelp32Snapshot(4, 0)
    entry = ThreadEntry()
    entry.size = ctypes.sizeof(ThreadEntry)
    more = kernel.Thread32First(snapshot, ctypes.byref(entry))
    while more:
        if entry.process == pid:
            yield entry.thread
        more = kernel.Thread32Next(snapshot, ctypes.byref(entry))


def cpu_time(handle):
    created, exited, kernel_time, user_time = W.FILETIME(), W.FILETIME(), W.FILETIME(), W.FILETIME()
    kernel.GetThreadTimes(handle, created, exited, kernel_time, user_time)
    return sum(t.dwHighDateTime << 32 | t.dwLowDateTime for t in (kernel_time, user_time))


def busiest_thread(pid):
    handles = {t: kernel.OpenThread(THREAD_ACCESS, False, t) for t in thread_ids(pid)}
    before = {t: cpu_time(h) for t, h in handles.items() if h}
    time.sleep(1)
    return handles[max(before, key=lambda t: cpu_time(handles[t]) - before[t])]


def stack_of(game, thread):
    buffer = ctypes.create_string_buffer(1232 + 16)
    context = (ctypes.addressof(buffer) + 15) & ~15
    kernel.SuspendThread(thread)
    try:
        ctypes.c_uint32.from_address(context + 0x30).value = CONTEXT_FULL
        kernel.GetThreadContext(thread, ctypes.c_void_p(context))
        rip = ctypes.c_uint64.from_address(context + 0xF8).value
        rsp = ctypes.c_uint64.from_address(context + 0x98).value
        stack = b''.join(game.read(rsp + offset, BLOCK) or b'' for offset in range(0, STACK_BYTES, BLOCK))
    finally:
        kernel.ResumeThread(thread)
    return rip, stack


game = Game(int(sys.argv[1]))
rip, stack = stack_of(game, busiest_thread(game.pid))
print('RIP', hex(rip - game.base))
addresses, functions = [], []
for offset in range(0, len(stack) - 8, 8):
    value = struct.unpack_from('<Q', stack, offset)[0]
    if game.base + 0x1000 < value < game.base + getattr(game, 'text_size', TEXT_SIZE):
        addresses.append(hex(value - game.base))
    elif 0x10000 < value < 0x7FFFFFFFFFFF and not value & 7:
        cls = game.ptr(value + 0x10)
        if cls and game.read(cls + 0x18, 4) and game.name(cls) in ('Function', 'DelegateFunction'):
            entry = f'{game.name(game.ptr(value + 0x20))}.{game.name(value)}'
            if entry not in functions:
                functions.append(entry)
print('Adressen:', ' '.join(addresses[:40]))
print('Funktionen:')
for entry in functions:
    print(' ', entry)
