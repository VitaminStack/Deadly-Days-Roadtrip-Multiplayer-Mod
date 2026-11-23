"""Read-only HUD-Portraets einer Spielinstanz: portraits.py <pid> [<pid> ...]. Build 26786272; nicht atomar.

Zeigt je WP_PlayerPortrait_C die Outer-Kette, den Controller (lokal mit LocalPlayer oder ohne Player), FlipIfPlayer2,
IndexFindAttempts, das Bild der Figur (Textur des Materials an BlinkingChar) und den farbigen Hintergrund
(Vektorparameter `Color` des Materials an ColorableBackground) samt Zuordnung zur Spielerfarbe P1 (Blau) oder P2 (Gruen).
Mit --overlay nur die Porträts im Spieler-HUD (WP_PlayerOverlay_C). Schreibt nichts, ruft nichts.
Befund und Offsets: docs/GAME_ANALYSIS.md#hud-porträt-opt-portraet."""
import struct
import sys

OUTER = 0x20
PORTRAIT_BACKGROUND = 0x330    # UImage* ColorableBackground
PORTRAIT_CHAR = 0x340          # UImage* BlinkingChar
PORTRAIT_CONTROLLER = 0x348    # APlayerController* Player_Controller
PORTRAIT_FLIP = 0x350          # bool FlipIfPlayer2
PORTRAIT_ATTEMPTS = 0x354      # int32 IndexFindAttempts
IMAGE_RESOURCE = 0x180 + 0x38  # UImage.Brush.ResourceObject
CONTROLLER_PLAYER = 0x348      # APlayerController.Player (UPlayer*, LocalPlayer oder NetConnection)
MATERIAL_PARENT = 0x170        # UMaterialInstance.Parent
MATERIAL_VECTORS = 0x200       # TArray<FVectorParameterValue>, Element 0x30: FName, FLinearColor bei +0x10
MATERIAL_TEXTURES = 0x220      # TArray<FTextureParameterValue>, Element 0x28: FName, UTexture* bei +0x10
# Parameter `Color` = ShadowColor von DA_PlayerColor_Blue (PlayerColorDeveloperSettings.AllPlayerColors[0]), gemessen.
# P2 waere DA_PlayerColor_Green (AllPlayerColors[1]); sie war in keiner Messung geladen, ihr Wert ist unbekannt.
PLAYER_COLORS = {'P1': (0.07, 0.27, 0.42)}


def player_slot(color, known=None, tolerance=0.02):
    """Ordnet eine Hintergrundfarbe (r, g, b) einer bekannten Spielerfarbe zu, sonst 'abweichend'."""
    if color is None:
        return 'ohne Farbe'
    for name, value in (known or PLAYER_COLORS).items():
        if all(abs(a - b) <= tolerance for a, b in zip(color, value)):
            return name
    return 'abweichend'


def class_name(game, obj):
    return game.name(game.ptr(obj + 0x10)) if obj else 'null'


def chain(game, obj, depth=6):
    names = []
    for _ in range(depth):
        obj = game.ptr(obj + OUTER)
        if not obj:
            break
        names.append(class_name(game, obj))
    return ' < '.join(names)


def parameters(game, material, offset, size):
    """Liste (Name, Adresse des Werts) eines Parameter-Arrays einer MaterialInstance."""
    head = game.read(material + offset, 16)
    if not head or len(head) != 16:
        return []
    data, count, _ = struct.unpack('<Qii', head)
    if not data or not 0 <= count <= 32:
        return []
    result = []
    for i in range(count):
        raw = game.read(data + size * i, 4)
        name = game.name_from_index(struct.unpack('<i', raw)[0]) if raw and len(raw) == 4 else '?'
        result.append((name, data + size * i + 0x10))
    return result


def material(game, image):
    """Material des Bildes mit Textur- und Farbparametern, None ohne Bild."""
    if not image:
        return None
    resource = game.ptr(image + IMAGE_RESOURCE)
    info = {'name': game.name(resource) if resource else 'null', 'class': class_name(game, resource),
            'textures': {}, 'colors': {}}
    if resource and 'MaterialInstance' in info['class']:
        for name, value in parameters(game, resource, MATERIAL_TEXTURES, 0x28):
            texture = game.ptr(value)
            info['textures'][name] = game.name(texture) if texture else 'null'
        for name, value in parameters(game, resource, MATERIAL_VECTORS, 0x30):
            raw = game.read(value, 16)
            if raw and len(raw) == 16:
                info['colors'][name] = struct.unpack('<4f', raw)[:3]
    return info


def collect(game, overlay_only=False):
    portraits = []
    for obj in game.objects():
        if class_name(game, obj) != 'WP_PlayerPortrait_C' or game.name(obj).startswith('Default__'):
            continue
        outers = chain(game, obj)
        if overlay_only and 'WP_PlayerOverlay_C' not in outers:
            continue
        controller = game.ptr(obj + PORTRAIT_CONTROLLER)
        player = game.ptr(controller + CONTROLLER_PLAYER) if controller else 0
        figure = material(game, game.ptr(obj + PORTRAIT_CHAR))
        background = material(game, game.ptr(obj + PORTRAIT_BACKGROUND))
        color = background['colors'].get('Color') if background else None
        portraits.append({
            'address': obj, 'name': game.name(obj), 'outers': outers,
            'controller': class_name(game, controller), 'player': class_name(game, player) if player else 'kein Player',
            'flip': (game.read(obj + PORTRAIT_FLIP, 1) or b'\0')[0], 'attempts': game.i32(obj + PORTRAIT_ATTEMPTS),
            'figure': figure['textures'].get('Texture') if figure else None, 'color': color,
            'slot': player_slot(color) if background and background['colors'] else 'ohne Farbe'})
    return portraits


def report(pid, overlay_only=False):
    from ddmem import Game  # erst hier: Windows-Speicherzugriff, die Tests laufen ohne
    print(f'== {pid}')
    for item in collect(Game(pid), overlay_only):
        color = '(%.2f %.2f %.2f)' % item['color'] if item['color'] else '-'
        print(f"  {item['name']} {hex(item['address'])} in {item['outers']}")
        print(f"    Controller {item['controller']} ({item['player']}) Flip={item['flip']} Versuche={item['attempts']}")
        print(f"    Figur {item['figure'] or '-'}, Hintergrund {color} = {item['slot']}")


if __name__ == '__main__':
    arguments = [a for a in sys.argv[1:] if a != '--overlay']
    if not arguments:
        raise SystemExit(__doc__)
    for argument in arguments:
        report(int(argument), '--overlay' in sys.argv)
