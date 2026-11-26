"""Tagesstempel der Rundenübersicht je Instanz lesen: round_stamps.py <pid> [<pid> ...] [--json DATEI].

Liest nur Speicher (Build 26786272, Layout in docs/OFFSETS.md, opt-rundenstempel). Je Instanz:
- Tageskennung des DDGameStatsSubsystem, aus der GenerateStats die Stempel setzt,
- CurrentRoute.CurrentMissionIndex und CachedCurrentMissionIndex der GameInstance (verschieden: Event-Stempel statt Tagesstempel),
- SelectedCharacters des CharacterSelectionManager (Schlüssel 0 liefert die Bilder des Tagesstempels),
- je offener WP_Round_Stats_C und Tag 1 bis 8: Sichtbarkeit und Bild von StickerField und EventStickerField, verbleibende
  Animationsbilder, Animationsflag und Zeitgeber.
"""
import json
import struct
import sys

from ddmem import Game
from round_stats import TIME_OFFSET, elements, find_subsystem

FLAG_CLASS_DEFAULT_OBJECT = 0x10
ROUTE_CURRENT, ROUTE_CACHED, ROUTE_CHAIN_NUM = 0x2C0, 0x2C4, 0x2B8  # UDDMissionGameInstance.CurrentRunInfo.CurrentRoute
SELECTED_CHARACTERS = 0x68  # UCharacterSelectionManager, TMap<int32, UPlayerCharacterDefinition*>
STAMPS = 0x4D8  # UWP_Round_Stats_C: Day8Stamp bis Day1Stamp
STICKER, EVENT_STICKER = 0x328, 0x330  # URoundEndSticker: UImage*
REMAINING, REMAINING_EVENT = 0x360, 0x370  # TArray<UTexture2D*>
ANIMATING, TIMER = 0x390, 0x394
VISIBILITY, BRUSH_RESOURCE = 0xDD, 0x180 + 0x38  # UWidget.Visibility, UImage.Brush.ResourceObject
VISIBILITIES = ['Visible', 'Collapsed', 'Hidden', 'HitTestInvisible', 'SelfHitTestInvisible']


def class_chain(game, obj):
    names, current = [], game.ptr(obj + 0x10)
    for _ in range(10):
        if not current:
            break
        names.append(game.name(current))
        current = game.ptr(current + 0x40)
    return names


def instances(game, class_name):
    found = []
    for obj in game.objects():
        cls = game.ptr(obj + 0x10)
        if not cls or class_name not in class_chain(game, obj):
            continue
        flags = struct.unpack('<I', game.read(obj + 8, 4) or b'\x10\0\0\0')[0]
        if not flags & FLAG_CLASS_DEFAULT_OBJECT:
            found.append(obj)
    return found


def image(game, widget):
    if not widget:
        return None
    vis = (game.read(widget + VISIBILITY, 1) or b'\xff')[0]
    resource = game.ptr(widget + BRUSH_RESOURCE)
    return {'vis': VISIBILITIES[vis] if vis < len(VISIBILITIES) else vis, 'bild': game.name(resource) if resource else None}


def read_instance(pid):
    game = Game(pid)
    out = {'pid': pid}
    subsystem = find_subsystem(game)
    out['tag'] = '{}.{}'.format(*struct.unpack('<ii', game.read(subsystem + TIME_OFFSET, 8) or b'\0' * 8)) if subsystem else None
    for gi in instances(game, 'DDMissionGameInstance'):
        out['route'] = {'current': game.i32(gi + ROUTE_CURRENT), 'cached': game.i32(gi + ROUTE_CACHED),
                        'kette': game.i32(gi + ROUTE_CHAIN_NUM)}
    for manager in instances(game, 'CharacterSelectionManager'):
        selected = {}
        for element in elements(game, game.read(manager + SELECTED_CHARACTERS, 0x50), 0x18):
            key, value = struct.unpack_from('<i4xQ', element, 0)
            selected[key] = game.name(value)
        out['figuren'] = selected
    panels = []
    for panel in instances(game, 'WP_Round_Stats_C'):
        days = {}
        for index in range(8):
            sticker = game.ptr(panel + STAMPS + 8 * (7 - index))
            if not sticker:
                continue
            days[index + 1] = {
                'stempel': image(game, game.ptr(sticker + STICKER)),
                'event': image(game, game.ptr(sticker + EVENT_STICKER)),
                'rest': game.i32(sticker + REMAINING + 8), 'rest_event': game.i32(sticker + REMAINING_EVENT + 8),
                'anim': (game.read(sticker + ANIMATING, 1) or b'\0')[0],
                'timer': round(struct.unpack('<f', game.read(sticker + TIMER, 4) or b'\0' * 4)[0], 2),
            }
        panels.append({'panel': hex(panel), 'tage': days})
    out['panels'] = panels
    return out


def line(day, state):
    def show(img):
        return '-' if not img else f'{img["vis"]}:{img["bild"]}'
    return (f'  Tag {day}: stempel {show(state["stempel"])} event {show(state["event"])} rest {state["rest"]}/{state["rest_event"]}'
            f' anim {state["anim"]} timer {state["timer"]}')


def main(argv):
    out = None
    if '--json' in argv:
        at = argv.index('--json')
        out = argv[at + 1]
        argv = argv[:at] + argv[at + 2:]
    pids = [int(a) for a in argv]
    if not pids:
        print(__doc__)
        return 2
    sides = [read_instance(pid) for pid in pids]
    for side in sides:
        print(f'{side["pid"]}: Tag {side["tag"]} Route {side.get("route")} Figuren {side.get("figuren")}')
        for panel in side['panels']:
            print(f' Übersicht {panel["panel"]}')
            for day, state in panel['tage'].items():
                print(line(day, state))
    if out:
        with open(out, 'w', encoding='utf-8') as handle:
            json.dump(sides, handle, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
