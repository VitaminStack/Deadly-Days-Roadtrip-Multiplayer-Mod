"""Kontrollierter Lauf in der eigenen Sitzung mit geladener ItemLootProbe.

Erzeugt Saecke neben den vorhandenen Figuren, ruft selbst niemals Loot auf.
Prueft natuerliche Anziehung/Aufsammlung und exakte Inventarzuordnung.
Rohberichte bleiben unter research/local; kein Spielstart und kein Prozessende.
"""
import argparse
import json
import os
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "spike"))
from ddmem import Game
from inventory import snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--players", type=int, choices=[0, 1], nargs="+", default=[1, 1, 1],
                        help="Erwartete Sammler je Sack; vor Host-Gegenproben Figuren auseinanderstellen")
    args = parser.parse_args()
    session = json.loads((ROOT / "research/local/session/session.json").read_text(encoding="utf-8-sig"))
    host, client = session["Host"], session["Client"]
    game = Game(host)
    state = game.ptr(game.world() + 0x1B0)
    players = game.ptr(state + 0x2C0)
    if game.i32(state + 0x2C8) != 2:
        raise RuntimeError("Braucht genau zwei aktuelle PlayerStates")
    ids = [game.i32(game.ptr(players + 8 * i) + 0x2AC) for i in range(2)]
    if len(set(ids)) != 2:
        raise RuntimeError("Spielerzuordnung nicht eindeutig")
    output = ROOT / "research/local/itemsack-absturz"
    output.mkdir(parents=True, exist_ok=True)
    report = output / f"natural-pickups-{time.time_ns()}.json"
    command = Path(os.environ["TEMP"]) / "dditemsack" / f"{host}.cmd"
    results = []
    for index, player in enumerate(args.players, 1):
        if command.exists():
            raise RuntimeError("Vorheriger Probe-Befehl noch nicht abgeholt")
        before = snapshot(host)
        before_client = snapshot(client)
        if before != before_client:
            raise RuntimeError("Inventare vor Aufsammlung nicht gleich")
        expected = {pid: len(inventory["items"]) + (pid == ids[player]) for pid, inventory in before.items()}
        temporary = command.with_suffix(".tmp")
        temporary.write_text(str(player + 1), encoding="ascii")
        temporary.replace(command)
        deadline = time.monotonic() + 20
        while True:
            after, after_client = snapshot(host), snapshot(client)
            counts = {pid: len(inventory["items"]) for pid, inventory in after.items()}
            if counts == expected and after == after_client:
                break
            if time.monotonic() >= deadline:
                results.append({"case": index, "player_index": player, "expected": expected,
                                "before": before, "after": after, "client": after_client, "passed": False})
                report.write_text(json.dumps(results, indent=2), encoding="utf-8")
                raise RuntimeError(f"Aufsammlung {index}: erwartet {expected}, erhalten {counts}; Client gleich: {after == after_client}")
            time.sleep(0.25)
        other = ids[1 - player]
        if after[other] != before[other]:
            raise RuntimeError("Fremdes Inventar veraendert")
        result = {"case": index, "player_index": player, "player_id": ids[player],
                  "before": before, "after": after, "client": after_client,
                  "equal": after == after_client, "counts": counts}
        results.append(result)
        report.write_text(json.dumps(results, indent=2), encoding="utf-8")
        print(f"Fall {index}: Spieler {player}, genau +1 Item, anderer Spieler unveraendert, Client gleich", flush=True)


if __name__ == "__main__":
    main()
