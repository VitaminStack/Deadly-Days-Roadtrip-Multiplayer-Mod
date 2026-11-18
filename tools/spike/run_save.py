"""Liest den Run-Stand (RunPersistence) aus einem Spielstand, nur lesend: run_save.py <SaveSlot_DDR_0.sav> [...]

Je Datei eine Zeile: Zustand (ERunDataState, ohne Feld `leer`), Zeitstempel, Zahl der Komponenten, Level und Items je Spieler,
dazu ein SHA-256 über den Abschnitt, damit sich zwei Stände ohne Blick in den Inhalt vergleichen lassen. `--json` gibt den Inhalt aus.
"""
import hashlib
import json
import re
import struct
import sys


def section(data: bytes) -> bytes:
    start = data.find(b"RunPersistence\x00")
    if start < 0:
        return b""
    # Der Abschnitt endet am nächsten Eintrag der SaveDataMap: None, None, dann der nächste Schlüssel.
    end = data.find(b"None\x00", data.find(b"SerializedJson\x00", start))
    return data[start : end if end > 0 else len(data)]


def serialized_json(part: bytes):
    at = part.find(b"SerializedJson\x00")
    if at < 0:
        return None
    for pos in range(at, min(at + 96, len(part) - 5)):
        length = struct.unpack_from("<i", part, pos)[0]
        if 0 < length < len(part) and part[pos + 4 : pos + 5] == b"{":
            return part[pos + 4 : pos + 4 + length - 1].decode("utf-8", "replace")
        if length < 0 and part[pos + 4 : pos + 6] == b"{\x00":
            return part[pos + 4 : pos + 4 - 2 * length - 2].decode("utf-16-le", "replace")
    return None


def read(path: str) -> dict:
    with open(path, "rb") as file:
        data = file.read()
    part = section(data)
    # Zwischen dem Enum-Namen und dem JSON stehen sein Paket und der Wert, der Wert zuletzt.
    begin, end = part.find(b"ERunDataState\x00"), part.find(b"SerializedJson\x00")
    head = part[begin:end] if 0 <= begin < end else b""
    names = [n for n in re.findall(rb"([A-Za-z]{4,})\x00", head) if n not in (b"ERunDataState", b"DDSurvivors")]
    text = serialized_json(part)
    try:
        content = json.loads(text) if text else {}
    except ValueError:
        content = {"Components": {"unlesbar": {}}}
    components = content.get("Components") or {}
    result = {
        "file": path,
        "state": names[-1].decode() if names else "leer",
        "timestamp": content.get("Timestamp", ""),
        "components": len(components),
        "level": {},
        "items": {},
        "sha256": hashlib.sha256(part).hexdigest()[:16],
        "content": content,
    }
    for key, value in components.items():
        if not isinstance(value, dict):
            continue
        if key.startswith("LevelSystem"):
            result["level"][key] = next((v for k, v in value.items() if "level" in k.lower() and isinstance(v, (int, float))), value)
        if key.startswith("Inventory") and isinstance(value.get("items"), list):
            result["items"][key] = len(value["items"])
    return result


def main() -> int:
    args = [a for a in sys.argv[1:] if a != "--json"]
    if not args:
        print(__doc__)
        return 2
    for path in args:
        info = read(path)
        if "--json" in sys.argv:
            print(json.dumps(info["content"], indent=1))
            continue
        print(
            f"{info['state']:<14} {info['timestamp'] or '-':<26} Komponenten {info['components']:<3} "
            f"Level {info['level'] or '-'} Items {info['items'] or '-'} sha {info['sha256']}  {path}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
