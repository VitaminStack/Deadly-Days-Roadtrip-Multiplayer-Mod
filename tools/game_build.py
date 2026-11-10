"""Unabhaengige Entwicklungskopie erstellen/pruefen, ohne Steam zu veraendern."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tempfile

RECEIPT = '.ddcoop-build.json'
# Standard fuer Skripte ohne --build. Weitere unterstuetzte Builds stehen in REFERENCES.
BUILD = '26495302'
# Lokal belegte Referenzen je Build aus docs/GAME_ANALYSIS.md, keine Versionsangabe aus einer editierbaren ACF.
REFERENCES = {
  # update-basis, 07.11.2026: Steam-Build nach dem Update, Depot 3026452 Manifest 2302428105354984436.
  '26495302': {
    'DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe': '4d012c685183b5b7d261ad0010fe12c5ef5dd2390faff11942cab7399967c9f5',
    'DDSurvivors/Content/Paks/DDSurvivors-Windows.pak': 'b6b50dc23cfec3717b28f46e26a03c23975434ab867b879710f66f0b62270b09',
    'DDSurvivors/Content/Paks/DDSurvivors-Windows.ucas': 'dffb4486259e309be9b71a1cb95ea86b5a00d65b4c190b57fff94f87976296a7',
    'DDSurvivors/Content/Paks/DDSurvivors-Windows.utoc': '8ff67e4d8fcb57a02dc512ec055e447588a9a0ea67efc9a6dfb2268770c664f6',
    'DDSurvivors/Content/Paks/global.ucas': '34661a41b09a06e3ccaf94f34732b497c1d8131232974f0fe44a09759326ebd2',
    'DDSurvivors/Content/Paks/global.utoc': '9c4476acd65a2da64d5cd50f8bdbe2536da94fbf218ad56bced52497a6eff71d',
  },
}
MUTABLE_DIRS = ('ddsurvivors/saved', 'ddsurvivors/binaries/win64/ddcoop')
MOD_PROXY = 'ddsurvivors/binaries/win64/version.dll'


def ignored(relative):
    name = relative.lower()
    return (name in (RECEIPT, MOD_PROXY) or
            any(name == part or name.startswith(part + '/') for part in MUTABLE_DIRS))


def reject_link(path):
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
        raise ValueError('Symlink/Junction nicht erlaubt: ' + path.name)
    if path.is_file() and info.st_nlink != 1:
        raise ValueError('Hardlink nicht erlaubt: ' + path.name)


def independent_path(path):
    path = Path(os.path.abspath(path))
    for parent in (path, *path.parents):
        if parent.exists() or parent.is_symlink():
            reject_link(parent)
    resolved = path.resolve()
    if 'steamapps' in (part.lower() for part in resolved.parts):
        raise ValueError('Entwicklungskopie muss ausserhalb aller steamapps-Ordner liegen.')
    return resolved


def inventory(root):
    result = {}
    def fail_walk(error):
        # os.walk ueberspringt sonst unlesbare Unterordner still: kein vollstaendiger Beleg.
        raise error
    for directory, dirs, files in os.walk(root, onerror=fail_walk):
        for name in dirs[:]:
            path = Path(directory) / name
            reject_link(path)
            if ignored(path.relative_to(root).as_posix()):
                dirs.remove(name)
        for name in files:
            path = Path(directory) / name
            reject_link(path)
            relative = path.relative_to(root).as_posix()
            if ignored(relative):
                continue
            before = path.stat()
            with path.open('rb') as stream:
                hasher = hashlib.sha256()
                for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                    hasher.update(chunk)
                digest = hasher.hexdigest()
            after = path.stat()
            if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                raise ValueError('Datei waehrend Pruefung geaendert: ' + relative)
            result[relative] = {'size': after.st_size, 'sha256': digest}
    return result


def assert_reference(files, build):
    reference = REFERENCES.get(build)
    if reference is None:
        raise ValueError('Unbekannte Build-Referenz: ' + build)
    for name, digest in reference.items():
        if files.get(name, {}).get('sha256') != digest:
            raise ValueError('Nicht unterstuetzte oder veraenderte Spieldatei: ' + name)
    def content_packages(names):
        return {name for name in names if name.lower().startswith('ddsurvivors/content/paks/') and
                PurePosixPath(name).suffix.lower() in ('.pak', '.ucas', '.utoc')}
    if content_packages(files) != content_packages(reference):
        raise ValueError('Zusaetzliche oder abweichende Inhaltscontainer in der Quelle/im Beleg.')


def verify(directory, build=BUILD):
    root = independent_path(directory)
    reject_link(root / RECEIPT)
    with (root / RECEIPT).open(encoding='utf-8') as stream:
        receipt = json.load(stream)
    if receipt.get('schema') != 1 or receipt.get('build_id') != build:
        raise ValueError('Unpassender Beleg der Entwicklungskopie.')
    expected = receipt.get('files')
    if not isinstance(expected, dict) or not expected:
        raise ValueError('Leerer oder ungueltiger Dateibeleg.')
    for name in expected:
        path = PurePosixPath(name)
        if (not name or path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name or
                path.as_posix() != name or ignored(name)):
            raise ValueError('Ungueltiger relativer Dateipfad im Beleg.')
    assert_reference(expected, build)
    actual = inventory(root)
    if actual != expected:
        changed = sorted(name for name in actual.keys() | expected.keys() if actual.get(name) != expected.get(name))
        raise ValueError('Entwicklungskopie veraendert/ergänzt/unvollstaendig: ' + ', '.join(changed[:5]))
    return receipt


def freeze(source, destination, build=BUILD):
    source = Path(source).resolve(strict=True)
    destination = independent_path(destination)
    if destination.exists():
        raise ValueError('Ziel existiert bereits und bleibt unveraendert.')
    if source == destination or source in destination.parents or destination in source.parents:
        raise ValueError('Quell- und Zielordner duerfen nicht ineinander liegen.')
    original = inventory(source)
    assert_reference(original, build)
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Nur ein selbst erzeugter, frischer Zwischenordner wird bei einem Fehler aufgeraeumt.
    staging = Path(tempfile.mkdtemp(prefix=destination.name + '.partial-', dir=destination.parent))
    try:
        for name in original:
            target = staging / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / name, target)  # echte Kopien, keine Hardlinks zum Steam-Depot
        receipt = {'schema': 1, 'build_id': build, 'files': original}
        (staging / RECEIPT).write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        verify(staging, build)
        if inventory(source) != original:
            raise ValueError('Quellinstallation waehrend der Kopie veraendert; erneut versuchen.')
        # Unter Windows ersetzt rename kein bestehendes Ziel. Es gibt keinen Overwrite-Schalter.
        if destination.exists():
            raise ValueError('Ziel wurde inzwischen angelegt und bleibt unveraendert.')
        staging.rename(destination)
        return receipt
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    create = commands.add_parser('freeze')
    create.add_argument('--source', required=True)
    create.add_argument('--destination', required=True)
    check = commands.add_parser('verify')
    check.add_argument('--directory', required=True)
    for command in (create, check):
        command.add_argument('--build', default=BUILD)
    args = parser.parse_args()
    try:
        receipt = (freeze(args.source, args.destination, args.build) if args.command == 'freeze' else
                   verify(args.directory, args.build))
        print(f"OK: eingefrorener Build {receipt['build_id']}, {len(receipt['files'])} Dateien geprueft.")
    except (OSError, ValueError, TypeError, AttributeError) as error:
        print('FEHLER: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
