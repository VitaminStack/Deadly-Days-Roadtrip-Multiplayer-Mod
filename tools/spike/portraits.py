"""Read-only HUD-Portraets einer Spielinstanz: portraits.py <pid> [<pid> ...]. Build 26786272; nicht atomar.

Zeigt je WP_PlayerPortrait_C den Controller (Klasse, lokal oder entfernt), FlipIfPlayer2, IndexFindAttempts, das Bild der Figur
(BlinkingChar) und den farbigen Hintergrund (ColorableBackground) mit Farbe und Material, dazu die Outer-Kette bis zum Overlay.
Schreibt nichts, ruft nichts. Befund: docs/GAME_ANALYSIS.md#hud-portraet-opt-portraet."""
import struct
import sys

from ddmem import Game

OUTER = 0x20
PORTRAIT_BACKGROUND = 0x330  # UImage* ColorableBackground
PORTRAIT_CHAR = 0x340        # UImage* BlinkingChar
PORTRAIT_CONTROLLER = 0x348  # APlayerController* Player_Controller
PORTRAIT_FLIP = 0x350        # bool FlipIfPlayer2
PORTRAIT_ATTEMPTS = 0x354    # int32 IndexFindAttempts
IMAGE_TINT = 0x180 + 0x08    # UImage.Brush.TintColor.SpecifiedColor (FLinearColor)
IMAGE_RESOURCE = 0x180 + 0x38  # UImage.Brush.ResourceObject
IMAGE_COLOR = 0x240          # UImage.ColorAndOpacity
CONTROLLER_PLAYER = 0x348    # APlayerController.Player (UPlayer*, LocalPlayer oder NetConnection)
MATERIAL_PARENT = 0x170      # UMaterialInstance.Parent
MATERIAL_VECTORS = 0x200     # TArray<FVectorParameterValue> (Element 0x30: Name, Farbe bei +0x10)
MATERIAL_TEXTURES = 0x220    # TArray<FTextureParameterValue> (Element 0x28: Name, UTexture* bei +0x10)


def color(game, address):
    data = game.read(address, 16)
    return '(%.2f %.2f %.2f %.2f)' % struct.unpack('<4f', data) if data and len(data) == 16 else '?'


def chain(game, obj, depth=6):
    names = []
    for _ in range(depth):
        obj = game.ptr(obj + OUTER)
        if not obj:
            break
        names.append(game.name(game.ptr(obj + 0x10)))
    return ' < '.join(names)


def parameters(game, material):
    text = ''
    for offset, size, texture in ((MATERIAL_VECTORS, 0x30, False), (MATERIAL_TEXTURES, 0x28, True)):
        head = game.read(material + offset, 16)
        if not head or len(head) != 16:
            continue
        data, count, _ = struct.unpack('<Qii', head)
        for i in range(count if data and 0 <= count <= 32 else 0):
            element = data + size * i
            raw = game.read(element, 8)
            name = game.name_from_index(struct.unpack('<i', raw[:4])[0]) if raw else '?'
            value = game.ptr(element + 0x10)
            text += f' {name}=' + ((game.name(value) if value else 'null') if texture else color(game, element + 0x10))
    return text


def image(game, widget):
    if not widget:
        return 'null'
    resource = game.ptr(widget + IMAGE_RESOURCE)
    text = game.name(resource) if resource else 'null'
    if resource:
        cls = game.name(game.ptr(resource + 0x10))
        text += f' [{cls}]'
        if 'MaterialInstance' in cls:
            parent = game.ptr(resource + MATERIAL_PARENT)
            text += f' Parent={game.name(parent) if parent else "?"}' + parameters(game, resource)
    return f'{text} tint={color(game, widget + IMAGE_TINT)} color={color(game, widget + IMAGE_COLOR)}'


def report(pid):
    game = Game(pid)
    print(f'== {pid}')
    for obj in game.objects():
        if game.name(game.ptr(obj + 0x10)) != 'WP_PlayerPortrait_C' or game.name(obj).startswith('Default__'):
            continue
        controller = game.ptr(obj + PORTRAIT_CONTROLLER)
        player = game.ptr(controller + CONTROLLER_PLAYER) if controller else 0
        owner = game.name(game.ptr(player + 0x10)) if player else 'kein Player'
        flip = (game.read(obj + PORTRAIT_FLIP, 1) or b'\0')[0]
        print(f'  {game.name(obj)} {hex(obj)} in {chain(game, obj)}')
        print(f'    Controller {game.name(controller) if controller else "null"} ({owner}) Flip={flip} '
              f'Versuche={game.i32(obj + PORTRAIT_ATTEMPTS)}')
        print(f'    Figur: {image(game, game.ptr(obj + PORTRAIT_CHAR))}')
        print(f'    Hintergrund: {image(game, game.ptr(obj + PORTRAIT_BACKGROUND))}')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for argument in sys.argv[1:]:
        report(int(argument))
