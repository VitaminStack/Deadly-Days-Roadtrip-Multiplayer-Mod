"""run_save.py liest den Run-Abschnitt eines Spielstands, hier an nachgebauten Bytes ohne Spieldatei."""
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / 'tools' / 'spike' / 'run_save.py'
SPEC = importlib.util.spec_from_file_location('run_save', SOURCE)
run_save = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(run_save)


def fstring(text: str) -> bytes:
    raw = text.encode('ascii') + b'\x00'
    return struct.pack('<i', len(raw)) + raw


def save(content: dict, state: str = '', wide: bool = False) -> bytes:
    """Nachbau der Reihenfolge im Spielstand: Abschnittsname, optional der Zustand, dann das JSON als FString."""
    text = json.dumps(content)
    if wide:
        raw = (text + '\x00').encode('utf-16-le')
        value = struct.pack('<i', -(len(text) + 1)) + raw
    else:
        value = fstring(text)
    head = fstring('RunPersistence') + fstring('RunData') + fstring('RunDataContainer')
    if state:
        head += fstring('RunDataState') + fstring('ByteProperty') + fstring('ERunDataState') + fstring('/Script/DDSurvivors') + fstring(state)
    body = head + fstring('SerializedJson') + fstring('StrProperty') + b'\x00' * 9 + value + fstring('None')
    return b'GVAS' + fstring('Achievements') + fstring('None') + body + fstring('MetaCurrencyWallet') + fstring('None')


class RunSaveTests(unittest.TestCase):
    def read(self, data: bytes) -> dict:
        with tempfile.NamedTemporaryFile(suffix='.sav', delete=False) as file:
            file.write(data)
        try:
            return run_save.read(file.name)
        finally:
            os.unlink(file.name)

    def test_empty_run(self):
        info = self.read(save({'Timestamp': '', 'Components': {}}))
        self.assertEqual(info['state'], 'leer')
        self.assertEqual(info['components'], 0)
        self.assertEqual(info['level'], {})

    def test_open_run_with_level_and_items(self):
        content = {'Timestamp': '2026-11-18T21:09:09.726Z', 'Components': {
            'LevelSystem_Player0': {'level': 3, 'experience': 109},
            'Inventory_Player0': {'items': [{}, {}, {}]},
            'Inventory_Player1': {'items': [{}]},
            'GameStats_Player0': {}}}
        info = self.read(save(content, 'EndOfDay'))
        self.assertEqual(info['state'], 'EndOfDay')
        self.assertEqual(info['timestamp'], '2026-11-18T21:09:09.726Z')
        self.assertEqual(info['components'], 4)
        self.assertEqual(info['level'], {'LevelSystem_Player0': 3})
        self.assertEqual(info['items'], {'Inventory_Player0': 3, 'Inventory_Player1': 1})

    def test_wide_string_and_begin_of_day(self):
        info = self.read(save({'Timestamp': 't', 'Components': {'LevelSystem_Player0': {'level': 1, 'experience': 0}}}, 'BeginOfDay', wide=True))
        self.assertEqual(info['state'], 'BeginOfDay')
        self.assertEqual(info['level'], {'LevelSystem_Player0': 1})

    def test_hash_covers_only_the_run_section(self):
        content = {'Timestamp': 't', 'Components': {'LevelSystem_Player0': {'level': 2}}}
        first = self.read(save(content, 'EndOfDay'))
        other_meta = save(content, 'EndOfDay').replace(b'MetaCurrencyWallet', b'MetaCurrencyWallex')
        self.assertEqual(first['sha256'], self.read(other_meta)['sha256'])
        changed = self.read(save({'Timestamp': 't', 'Components': {'LevelSystem_Player0': {'level': 3}}}, 'EndOfDay'))
        self.assertNotEqual(first['sha256'], changed['sha256'])

    def test_file_without_run_section(self):
        info = self.read(b'GVAS' + fstring('Achievements') + fstring('None'))
        self.assertEqual(info['state'], 'leer')
        self.assertEqual(info['components'], 0)


if __name__ == '__main__':
    unittest.main()
