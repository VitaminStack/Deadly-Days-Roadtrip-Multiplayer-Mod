"""Laedt eine DLL per LoadLibraryW in einen Prozess: inject.py <pid> <dll>"""
import ctypes, sys
from ctypes import wintypes as W
k = ctypes.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.restype = W.HANDLE
k.VirtualAllocEx.restype = ctypes.c_void_p
k.VirtualAllocEx.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_size_t, W.DWORD, W.DWORD]
k.WriteProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
k.GetModuleHandleW.restype = ctypes.c_void_p
k.GetModuleHandleW.argtypes = [W.LPCWSTR]
k.GetProcAddress.restype = ctypes.c_void_p
k.GetProcAddress.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
k.CreateRemoteThread.restype = W.HANDLE
k.CreateRemoteThread.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p, W.DWORD, ctypes.c_void_p]
pid, dll = int(sys.argv[1]), sys.argv[2]
h = k.OpenProcess(0x1F0FFF, False, pid); assert h
b = dll.encode('utf-16le') + b'\0\0'
m = k.VirtualAllocEx(h, None, len(b), 0x3000, 4); assert m
assert k.WriteProcessMemory(h, m, b, len(b), None)
t = k.CreateRemoteThread(h, None, 0, k.GetProcAddress(k.GetModuleHandleW('kernel32'), b'LoadLibraryW'), m, 0, None); assert t
k.WaitForSingleObject(t, 10000)
c = W.DWORD(); k.GetExitCodeThread(t, ctypes.byref(c)); print('LoadLibrary low32', hex(c.value))
