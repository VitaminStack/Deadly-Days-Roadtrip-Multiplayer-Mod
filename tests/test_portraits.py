"""portraits.py mit synthetischem Speicherbild, ohne Spiel, SDK und Windows."""
import importlib.util
from pathlib import Path
import struct
import unittest

SPEC = importlib.util.spec_from_file_location("portraits", Path(__file__).parents[1] / "tools/spike/portraits.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class Memory:
    def __init__(self):
        self.rows, self.bytes, self.names, self.fnames = [], {}, {}, {}
        self.next = 0x10000000

    def alloc(self):
        self.next += 0x10000
        return self.next

    def put(self, address, data):
        for i, byte in enumerate(data):
            self.bytes[address + i] = byte

    def pointer(self, address, value):
        self.put(address, struct.pack('<Q', value))

    def obj(self, cls, name, outer=0, listed=False):
        address, class_address = self.alloc(), self.alloc()
        self.names.update({address: name, class_address: cls})
        self.pointer(address + 0x10, class_address)
        self.pointer(address + 0x20, outer)
        if listed:
            self.rows.append(address)
        return address

    def array(self, owner, offset, elements, size):
        data = self.alloc()
        self.put(owner + offset, struct.pack('<Qii', data, len(elements), len(elements)))
        for i, (name_index, payload) in enumerate(elements):
            self.put(data + size * i, struct.pack('<i', name_index))
            self.put(data + size * i + 0x10, payload)

    # Schnittstelle wie ddmem.Game
    def objects(self):
        return iter(self.rows)

    def read(self, address, size):
        return bytes(self.bytes.get(address + i, 0) for i in range(size))

    def ptr(self, address):
        return struct.unpack('<Q', self.read(address, 8))[0]

    def i32(self, address):
        return struct.unpack('<i', self.read(address, 4))[0]

    def name(self, obj):
        return self.names.get(obj, '?')

    def name_from_index(self, index):
        return self.fnames.get(index, '?')


def portrait(memory, overlay, local, texture, color):
    widget_tree = memory.obj('WidgetTree', 'WidgetTree', overlay)
    widget = memory.obj('WP_PlayerPortrait_C', 'WP_PlayerPortrait', widget_tree, listed=True)
    controller = memory.obj('BP_DDPlayerController_C', 'BP_DDPlayerController_C_0')
    if local:
        memory.pointer(controller + MODULE.CONTROLLER_PLAYER, memory.obj('LocalPlayer', 'LocalPlayer_0'))
    memory.pointer(widget + MODULE.PORTRAIT_CONTROLLER, controller)
    memory.put(widget + MODULE.PORTRAIT_FLIP, b'\x01')
    memory.put(widget + MODULE.PORTRAIT_ATTEMPTS, struct.pack('<i', 5))
    for field, mid, params in ((MODULE.PORTRAIT_CHAR, 'MID_M_CharacterPortrait_Master', 'texture'),
                               (MODULE.PORTRAIT_BACKGROUND, 'MID_M_hud_player_bg', 'color')):
        image = memory.obj('Image', 'Image_0')
        material = memory.obj('MaterialInstanceDynamic', mid)
        memory.pointer(widget + field, image)
        memory.pointer(image + MODULE.IMAGE_RESOURCE, material)
        if params == 'texture':
            memory.array(material, MODULE.MATERIAL_TEXTURES,
                         [(1, struct.pack('<Q', memory.obj('Texture2D', texture)))], 0x28)
        else:
            memory.array(material, MODULE.MATERIAL_VECTORS, [(2, struct.pack('<4f', *color, 1.0))], 0x30)
    return widget


class PortraitTests(unittest.TestCase):
    def setUp(self):
        self.memory = Memory()
        self.memory.fnames = {1: 'Texture', 2: 'Color'}
        self.overlay = self.memory.obj('WP_PlayerOverlay_C', 'WP_PlayerOverlay_C_0')

    def test_local_overlay_is_p1_with_its_figure(self):
        portrait(self.memory, self.overlay, True, 'sps_char_survivor_idle', (0.07, 0.27, 0.42))
        [item] = MODULE.collect(self.memory, overlay_only=True)
        self.assertEqual(item['player'], 'LocalPlayer')
        self.assertEqual(item['figure'], 'sps_char_survivor_idle')
        self.assertEqual(item['slot'], 'P1')
        self.assertEqual((item['flip'], item['attempts']), (1, 5))
        self.assertIn('WP_PlayerOverlay_C', item['outers'])

    def test_stale_controller_and_other_color_are_reported(self):
        portrait(self.memory, self.overlay, False, 'sps_char_banana_idle', (0.45, 0.10, 0.60))
        [item] = MODULE.collect(self.memory)
        self.assertEqual(item['player'], 'kein Player')
        self.assertEqual(item['slot'], 'abweichend')

    def test_overlay_filter_skips_other_portraits(self):
        panel = self.memory.obj('WP_Inventory_Panel_C', 'Panel')
        portrait(self.memory, panel, False, 'x', (0.07, 0.27, 0.42))
        self.assertEqual(MODULE.collect(self.memory, overlay_only=True), [])
        self.assertEqual(len(MODULE.collect(self.memory)), 1)

    def test_player_slot(self):
        self.assertEqual(MODULE.player_slot((0.071, 0.27, 0.42)), 'P1')
        self.assertEqual(MODULE.player_slot((0.5, 0.2, 0.6)), 'abweichend')
        self.assertEqual(MODULE.player_slot(None), 'ohne Farbe')


if __name__ == '__main__':
    unittest.main()
